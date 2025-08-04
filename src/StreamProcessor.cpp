#include "StreamProcessor.h"
#include "SharedState.h"
#include "detector.h"
#include "segmenter.h"
#include "fall.h"
#include "DatabaseManager.h"
#include "SerialCommunicator.h" 
#include "STM32Protocol.h"    
#include "AnomalyDetector.h"
#include "driver/led_pwm/led_controller/led_pwm_controller.h"

#include <iostream>
#include <thread>
#include <iomanip>
#include <sstream>
#include <termios.h>
#include <fstream>
#include <sys/sysinfo.h>

// 생성자
StreamProcessor::StreamProcessor(DatabaseManager& dbManager) : db_manager_(dbManager), brightness_beta_(0) {
    color_map_["person"] = cv::Scalar(0, 255, 0);
    color_map_["helmet"] = cv::Scalar(255, 178, 51);
    color_map_["safety-vest"] = cv::Scalar(0, 128, 255);

    color_map_["fall"] = cv::Scalar(0, 0, 255); 
    color_map_["stand"] = cv::Scalar(255, 0, 0); 

    // 시리얼 통신 객체 생성
    serial_comm_ = std::make_unique<SerialCommunicator>("/dev/ttyACM0", B115200);
    if (!serial_comm_->isOpen()) {
        std::cerr << "경고: UART 통신을 시작할 수 없습니다." << std::endl;
    }
    
    // 이상탐지 객체 생성
    try {
        anomaly_detector_ = std::make_unique<AnomalyDetector>();
        anomaly_detector_->start();
        std::cout << "이상탐지 시스템 초기화 완료" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "이상탐지 시스템 초기화 실패: " << e.what() << std::endl;
        anomaly_detector_ = nullptr;
    }

    try {
        led_fade_controller_ = std::make_unique<DebouncedFadeController>(LedController::instance(), 3000);
        std::cout << "[INFO] LED controller initialized successfully." << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[WARN] LED controller disabled: " << e.what() << std::endl;
        led_fade_controller_ = nullptr; 
    }
}

// 소멸자
StreamProcessor::~StreamProcessor() {
    if (anomaly_detector_) {
        anomaly_detector_->stop();
    }
    if (proc_processed_) pclose(proc_processed_);
    if (cap_.isOpened()) cap_.release();
}

bool StreamProcessor::isAnomalyDetected() const {
    // anomaly_detected_ 변수의 현재 값을 안전하게 읽어서 반환합니다.
    return anomaly_detected_.load();
}

void StreamProcessor::setBrightness(int beta) {
    std::lock_guard<std::mutex> lock(image_processing_settings_mutex_);
    brightness_beta_ = beta;
    std::cout << "밝기 설정 변경: " << brightness_beta_ << std::endl;
}

// 메인 루프 실행
void StreamProcessor::run() {
    if (!initialize_camera() || !initialize_streamers()) {
        g_keep_running = false;
        return;
    }

    std::cout << "영상 처리 및 스트리밍 루프를 시작합니다..." << std::endl;
    cv::Mat frame;

    while (g_keep_running) {
        if (!cap_.read(frame)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        process_frame_and_stream(frame);
        
        // 이상탐지 처리
        handle_anomaly_detection();

        // 시스템 정보 모니터링 처리
        handle_system_info_monitoring();
    }
}

// 콜백 등록 함수
void StreamProcessor::onAnomalyStatusChanged(std::function<void(bool)> callback) { anomaly_callback_ = callback; }
void StreamProcessor::onNewDetection(std::function<void(const DetectionData&)> callback) { detection_callback_ = callback; }
void StreamProcessor::onNewBlur(std::function<void(const PersonCountData&)> callback) { blur_callback_ = callback; }
void StreamProcessor::onNewFall(std::function<void(const FallCountData&)> callback) { fall_callback_ = callback; }
void StreamProcessor::onNewTrespass(std::function<void(const TrespassLogData&)> callback) { trespass_callback_ = callback; }
void StreamProcessor::onSystemInfoUpdate(std::function<void(double, double)> callback) { system_info_callback_ = callback; }


void StreamProcessor::handle_anomaly_detection() {
    // 1초마다 이상탐지 상태 확인
    time_t current_time = time(0);
    if (current_time - last_anomaly_check_ >= ANOMALY_CHECK_INTERVAL) {
        last_anomaly_check_ = current_time;
        
        if (anomaly_detector_) {
            bool current_anomaly = anomaly_detector_->isAnomalyDetected();

            // [핵심] 이전에 정상이였다가(false) 지금 이상이 감지된(true) 첫 순간에만 실행
            if (current_anomaly && !anomaly_detected_.load()) {

                // 등록된 콜백이 있으면 호출 
                if (anomaly_callback_) {
                anomaly_callback_(current_anomaly);
                }
                
                std::cout << "🚨 이상탐지! 알람을 1회 울립니다." << std::endl;

                if (serial_comm_ && serial_comm_->isOpen()) {
                    // --- 1. 알람을 켜기 위해 첫 번째 TOGGLE 신호 전송 ---
                    uint8_t seq1 = serial_comm_->getNextSeq();
                    auto frame_on = STM32Protocol::buildToggleFrame(seq1);
                    serial_comm_->sendAndReceive(frame_on, "Sent TOGGLE (ON)");
                }
            }

            // 현재 상태를 마지막으로 업데이트
            anomaly_detected_ = current_anomaly;
        }
    }
}

void StreamProcessor::handle_system_info_monitoring() {
    static time_t last_system_info_check = 0;
    constexpr static int SYSTEM_INFO_CHECK_INTERVAL = 1; // 5초마다 체크

    time_t current_time = time(0);
    if (current_time - last_system_info_check >= SYSTEM_INFO_CHECK_INTERVAL) {
        last_system_info_check = current_time;

        if (system_info_callback_) {
            // CPU 사용률 계산
            double cpu_usage = get_cpu_usage();
            double memory_usage = get_memory_usage();

            // 콜백 호출
            system_info_callback_(cpu_usage, memory_usage);
        }
    }
}

double StreamProcessor::get_cpu_usage() {
    static struct {
        long long user, nice, system, idle, iowait, irq, softirq;
        bool first_run = true;
    } cpu_stats;

    std::ifstream file("/proc/stat");
    std::string line;

    if (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string cpu;
        long long user, nice, system, idle, iowait, irq, softirq;

        iss >> cpu >> user >> nice >> system >> idle >> iowait >> irq >> softirq;

        if (cpu_stats.first_run) {
            cpu_stats.user = user;
            cpu_stats.nice = nice;
            cpu_stats.system = system;
            cpu_stats.idle = idle;
            cpu_stats.iowait = iowait;
            cpu_stats.irq = irq;
            cpu_stats.softirq = softirq;
            cpu_stats.first_run = false;
            return 0.0;
        }

        long long total_diff = (user + nice + system + idle + iowait + irq + softirq) - 
                              (cpu_stats.user + cpu_stats.nice + cpu_stats.system + 
                               cpu_stats.idle + cpu_stats.iowait + cpu_stats.irq + cpu_stats.softirq);

        long long active_diff = (user + nice + system + irq + softirq) - 
                               (cpu_stats.user + cpu_stats.nice + cpu_stats.system + 
                                cpu_stats.irq + cpu_stats.softirq);

        cpu_stats.user = user;
        cpu_stats.nice = nice;
        cpu_stats.system = system;
        cpu_stats.idle = idle;
        cpu_stats.iowait = iowait;
        cpu_stats.irq = irq;
        cpu_stats.softirq = softirq;

        if (total_diff > 0) {
            return (static_cast<double>(active_diff) / total_diff) * 100.0;
        }
    }

    return 0.0;
}

double StreamProcessor::get_memory_usage() {
    struct sysinfo info;
    if (sysinfo(&info) != 0) {
        return -1.0;
    }

    unsigned long total_ram = info.totalram * info.mem_unit;
    unsigned long free_ram = info.freeram * info.mem_unit;
    unsigned long used_ram = total_ram - free_ram;

    return (static_cast<double>(used_ram) / total_ram) * 100.0;
}

void StreamProcessor::process_frame_and_stream(cv::Mat& original_frame) {
    // 1. 모드 변경 확인 및 모델 로드
    handle_mode_change();

    std::string active_mode;
    {
        std::lock_guard<std::mutex> lock(g_mode_mutex);
        active_mode = g_current_mode;
    }

    // 2. 모든 모드에 공통적으로 적용될 프레임 생성
    cv::Mat processed_frame = original_frame.clone();

    // 3. 공통 이미지 처리 필터 적용
    // 3-1. 기본 가우시안 블러 (항상 적용)
    cv::GaussianBlur(processed_frame, processed_frame, cv::Size(gaussian_blur_kernel_size_, gaussian_blur_kernel_size_), 0);

    // 3-2. 클라이언트가 조절한 밝기 적용 (항상 적용)
    {
        std::lock_guard<std::mutex> lock(image_processing_settings_mutex_);
        if (brightness_beta_ != 0) {
            // alpha(대비)는 1.0으로 고정하고 beta(밝기)만 조절합니다.
            processed_frame.convertTo(processed_frame, -1, 1.0, brightness_beta_);
        }
    }

    // 4. 모드별 특화 처리 (AI 탐지 및 그리기)
    //    이제 모든 모드는 이미 필터가 적용된 'processed_frame'을 사용합니다.
    if (active_mode == "detect" && detector_) {
        auto results = detector_->detect(processed_frame, 0.4, 0.45);

        int person_count = 0, helmet_count = 0, vest_count = 0;
        const auto& class_names = detector_->get_class_names();
        for (const auto& res : results) {
            if (res.class_id < class_names.size()) {
                const std::string& class_name = class_names[res.class_id];
                if (class_name == "person") person_count++;
                else if (class_name == "helmet") helmet_count++;
                else if (class_name == "safety-vest") vest_count++;
            }
        }

        bool is_unsafe = (helmet_count < person_count || vest_count < person_count);

            // 음성 안내
        if (is_unsafe) {
            if (led_fade_controller_) {
                led_fade_controller_->triggerFade();
            }
            if (!audio_notifier_.isPlaying()) {
                bool only_helmet_missing = (helmet_count < person_count) && (vest_count >= person_count);
                bool only_vest_missing = (vest_count < person_count) && (helmet_count >= person_count);

                if (only_helmet_missing) {
                    std::cout << "[INFO] Playing sound: helmet_ment.wav" << std::endl;
                    audio_notifier_.play("sounds/helmet_ment.wav");
                } else if (only_vest_missing) {
                    std::cout << "[INFO] Playing sound: vest_ment.wav" << std::endl;
                    audio_notifier_.play("sounds/vest_ment.wav");
                } else {
                    std::cout << "[INFO] Playing sound: safety_ment.wav" << std::endl;
                    audio_notifier_.play("sounds/safety_ment.wav");
                }
            }
        }

        // STM32 신호 전송
        if (is_unsafe && serial_comm_ && serial_comm_->isOpen()) {
            uint8_t seq = serial_comm_->getNextSeq();
            auto frame_to_send = STM32Protocol::buildToggleFrame(seq);
            // 응답을 기다리지 않는 sendOnly로 변경하는 것을 고려해볼 수 있습니다.
            serial_comm_->sendAndReceive(frame_to_send, "Sent TOGGLE (seq=" + std::to_string(seq) + ")");
        }

        // 탐지 결과 그리기
        for (const auto& res : results) {
            if (res.class_id < class_names.size()) {
                std::string class_name = class_names[res.class_id];
                cv::Scalar color = color_map_.count(class_name) ? color_map_[class_name] : cv::Scalar(0, 0, 255);
                    
                // 사각형 그리기
                cv::rectangle(processed_frame, res.box, color, 2);

                // 텍스트 그리기 로직 
                std::stringstream label_ss;
                label_ss << class_name << " " << std::fixed << std::setprecision(2) << res.confidence;
                std::string label = label_ss.str();
                    
                int baseLine;
                cv::Size label_size = cv::getTextSize(label, cv::FONT_HERSHEY_SIMPLEX, 0.5, 1, &baseLine);

                int text_y = res.box.y - 10;
                if (text_y < label_size.height) {
                    text_y = res.box.y + label_size.height + 10;
                }

                cv::rectangle(processed_frame, 
                            cv::Point(res.box.x, text_y - label_size.height - 5),
                            cv::Point(res.box.x + label_size.width, text_y + baseLine - 5),
                            color, -1);
                cv::putText(processed_frame, label, cv::Point(res.box.x, text_y - 5), 
                            cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(255, 255, 255), 1);
            }
        }


            // DB 저장
        if (time(0) - last_save_time_ >= 3) {
            auto saved_data = db_manager_.saveDetectionLog(camera_id_, results, processed_frame, *detector_);
            if (detection_callback_ && saved_data.has_value()) {
                detection_callback_(saved_data.value());
            }
            last_save_time_ = time(0);
        }
    } else if (active_mode == "trespass" && detector_) {
        auto results = detector_->detect(processed_frame, 0.4, 0.45);
            
        int person_count = 0;
        const auto& class_names = detector_->get_class_names();
        for (const auto& res : results) {
            if (res.class_id < class_names.size() && class_names[res.class_id] == "person") {
                person_count++;
            }
        }
            
        bool trespass_detected = (person_count > 0);

        if(trespass_detected) {
            if (led_fade_controller_) {
                led_fade_controller_->triggerFade();
            }

            if(serial_comm_ && serial_comm_->isOpen()) {
                uint8_t seq = serial_comm_->getNextSeq();
                auto frame_to_send = STM32Protocol::buildToggleFrame(seq);
                // 응답을 기다리지 않는 sendOnly로 변경하는 것을 고려해볼 수 있습니다.
                serial_comm_->sendAndReceive(frame_to_send, "Sent TOGGLE (seq=" + std::to_string(seq) + ")");
            }
        }
            
        // 결과 그리기 (person만 빨간색으로)
        for (const auto& res : results) {
            if (res.class_id < class_names.size() && class_names[res.class_id] == "person") {
                cv::rectangle(processed_frame, res.box, cv::Scalar(0, 0, 255), 2);
                // 1. 표시할 라벨 생성 ("person" + 신뢰도 점수)
                std::string label = "person " + cv::format("%.2f", res.confidence);
                cv::Scalar color = cv::Scalar(0, 0, 255); // 빨간색

                // 2. 텍스트 배경을 위한 설정
                int baseLine;
                cv::Size label_size = cv::getTextSize(label, cv::FONT_HERSHEY_SIMPLEX, 0.5, 1, &baseLine);
                int text_y = res.box.y - 10;
                if (text_y < label_size.height) {
                    text_y = res.box.y + res.box.height + label_size.height + 5;
                }

                // 3. 텍스트 배경 사각형 그리기
                cv::rectangle(processed_frame,
                            cv::Point(res.box.x, text_y - label_size.height - 5),
                            cv::Point(res.box.x + label_size.width, text_y + baseLine - 5),
                            color, -1);

                // 4. 실제 텍스트 그리기
                cv::putText(processed_frame, label, cv::Point(res.box.x, text_y - 5),
                            cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(255, 255, 255), 1);
            }
        }

            // DB 저장 및 웹소켓 알림
        if (time(0) - last_save_time_ >= 3) {
            if (trespass_detected) {
                auto saved_data = db_manager_.saveTrespassLog(camera_id_, person_count, processed_frame);
                if (trespass_callback_ && saved_data.has_value()) {
                    trespass_callback_(saved_data.value());
                }
            }
            last_save_time_ = time(0);
        }
    } else if (active_mode == "fall" && fall_) {
        // 1. 넘어짐 탐지 실행
        auto results = fall_->detect(processed_frame, 0.4, 0.45);
    
        // 2. 넘어짐 상황 판단
        bool fall_detected = false;
        const auto& class_names = fall_->get_class_names();
        for (const auto& res : results) {
            if (res.class_id < class_names.size()) {
                if (class_names[res.class_id] == "fall") {
                    fall_detected = true;
                    break; // 한 명이라도 넘어지면 즉시 알림
                }
            }
        }
    
        // 3. 넘어짐 발생 시 음성 안내 및 STM32 신호 전송
        if (fall_detected) {
            if (led_fade_controller_) {
                led_fade_controller_->triggerFade();
            }

            if (!audio_notifier_.isPlaying()) {
                audio_notifier_.play("sounds/fall_ment.wav");
                std::cout << "[INFO] Playing sound: fall_ment.wav" << std::endl;
            }
            if (serial_comm_ && serial_comm_->isOpen()) {
                uint8_t seq = serial_comm_->getNextSeq();
                auto frame_to_send = STM32Protocol::buildToggleFrame(seq);
                serial_comm_->sendAndReceive(frame_to_send, "Sent FALL ALERT");
            }
        }
            
        // 4. 탐지 결과를 display_frame에 그리기
        for (const auto& res : results) {
            if (res.class_id < class_names.size()) {
                std::string class_name = class_names[res.class_id];
                cv::Scalar color = color_map_.count(class_name) ? color_map_[class_name] : cv::Scalar(255, 255, 255);
                    
                cv::rectangle(processed_frame, res.box, color, 2);
                std::string label = class_name + " " + cv::format("%.2f", res.confidence);
                
                int baseLine;
                cv::Size label_size = cv::getTextSize(label, cv::FONT_HERSHEY_SIMPLEX, 0.5, 1, &baseLine);
                int text_y = res.box.y - 10;
                if (text_y < label_size.height) {
                    text_y = res.box.y + label_size.height + 10;
                }
                cv::rectangle(processed_frame, cv::Point(res.box.x, text_y - label_size.height - 5), cv::Point(res.box.x + label_size.width, text_y + baseLine), color, -1);
                cv::putText(processed_frame, label, cv::Point(res.box.x, text_y - 5), cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(255, 255, 255), 1);
            }
        }

        // 5. DB 저장 (필요 시 구현, 여기서는 예시로 넘어짐 카운트만 저장)
        if (time(0) - last_save_time_ >= 3) {
            if(fall_detected) {
                auto saved_data = db_manager_.saveFallLog(camera_id_, fall_detected, processed_frame);
                if (fall_callback_ && saved_data.has_value()) {
                    fall_callback_(saved_data.value());
                }
            }
            last_save_time_ = time (0);
        }

    } else if (active_mode == "blur" && segmenter_) {
        SegmentationResult seg_result = segmenter_->process_frame(processed_frame);
        int blur_count = seg_result.person_count;

        if (time(0) - last_save_time_ >= 3) {
            if(blur_count > 0) {
                auto saved_data = db_manager_.saveBlurLog(camera_id_, blur_count);
                if (blur_callback_ && saved_data.has_value()) {
                    blur_callback_(saved_data.value());
                }
            }
            last_save_time_ = time(0);
        }
    } else if (active_mode == "stop") {
        cv::putText(processed_frame, "STOPPED", cv::Point(10, 60), cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(0, 0, 255), 2);
    }
    // "raw" 모드일 경우, 위 if-else 문을 모두 건너뛰고 필터만 적용된 processed_frame이 남게 됩니다.

    // 5. 최종 프레임에 공통 상태 정보를 그리고 스트리밍합니다.
    if (!processed_frame.empty()) {
        //cv::putText(processed_frame, "MODE: " + active_mode, cv::Point(10, 30), cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(255, 0, 0), 2);

        if (proc_processed_) {
            fwrite(processed_frame.data, 1, processed_frame.total() * processed_frame.elemSize(), proc_processed_);
        }
    }
}


// (handle_mode_change 및 나머지 헬퍼 함수들은 이전과 동일)
void StreamProcessor::handle_mode_change() {
    std::string active_mode;
    {
        std::lock_guard<std::mutex> lock(g_mode_mutex);
        active_mode = g_current_mode;
    }

    if (active_mode != last_loaded_mode_) {
        detector_.reset();
        segmenter_.reset();
        std::cout << "모드 변경 시도: " << active_mode << std::endl;
        
        // ✅ "raw" 또는 "stop" 모드는 모델 로딩이 필요 없음
        if (active_mode == "raw" || active_mode == "stop") {
            last_loaded_mode_ = active_mode;
            std::cout << active_mode << " 모드로 전환 (모델 로드 없음)" << std::endl;
            return; // 모델 로드 없이 함수 종료
        }

        try {
            if (active_mode == "detect" || active_mode == "trespass") {
                if (!detector_ || last_loaded_mode_ != "detect" && last_loaded_mode_ != "trespass") {
                    detector_ = std::make_unique<Detector>(detection_model_path_);
                }
            } else if (active_mode == "blur") {
                segmenter_ = std::make_unique<Segmenter>(segmentation_model_path_);
            } else if (active_mode == "fall") {
                fall_ = std::make_unique<Fall>(fall_model_path_);
            }
            last_loaded_mode_ = active_mode;
            last_save_time_ = time(0); 
            std::cout << "다음 모드를 위한 모델 로드 완료: " << active_mode << std::endl;
        } catch (const Ort::Exception& e) {
            std::cerr << "모델 로딩 중 오류 발생: " << e.what() << std::endl;
            g_keep_running = false;
        }
    }
}
bool StreamProcessor::initialize_camera() {
    cap_.open(gstreamer_pipeline(), cv::CAP_GSTREAMER);
    if (!cap_.isOpened()) {
        std::cerr << "오류: 카메라를 열 수 없습니다." << std::endl;
        return false;
    }
    return true;
}

bool StreamProcessor::initialize_streamers() {
    proc_processed_ = create_ffmpeg_process(rtsp_url_);
    if (!proc_processed_) {
        std::cerr << "오류: FFmpeg 프로세스를 생성할 수 없습니다." << std::endl;
        return false;
    }
    return true;
}


FILE* StreamProcessor::create_ffmpeg_process(const std::string& rtsp_url) {
    std::string cmd = "ffmpeg -f rawvideo -pixel_format bgr24 -video_size " +
                      std::to_string(capture_width_) + "x" + std::to_string(capture_height_) +
                      " -framerate " + std::to_string(framerate_) + " -i - "
                      "-c:v h264_v4l2m2m -b:v 2M -bufsize 2M -maxrate 2M "
                      " -g 30 -keyint_min 30 -sc_threshold 0 "
                      "-pix_fmt yuv420p -f rtsp -rtsp_transport tcp " + rtsp_url;
    std::cout << "FFmpeg 실행 명령어: " << cmd << std::endl;
    return popen(cmd.c_str(), "w");
}

std::string StreamProcessor::gstreamer_pipeline() {
    // Python의 picam2 설정과 유사한 최적화된 파이프라인
    // sync=false, max-buffers=1, drop=true로 설정하여 최대한 딜레이 감소
    // RGB888 포맷으로 직접 설정 (Python의 format="RGB888"과 동일)
    return "libcamerasrc ! "
           "video/x-raw, width=" + std::to_string(capture_width_) +
           ", height=" + std::to_string(capture_height_) +
           ", framerate=" + std::to_string(framerate_) + "/1 ! "
           "videoconvert ! "
           "video/x-raw, format=BGR ! "
           "appsink sync=false max-buffers=1 drop=true";
}

SerialCommunicator& StreamProcessor::getSerialCommunicator() {
    // unique_ptr이 소유한 객체의 참조를 반환
    return *serial_comm_;
}