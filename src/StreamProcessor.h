#pragma once

#include <opencv2/opencv.hpp>
#include <string>
#include <memory>
#include <atomic>
#include <map>
#include <cstdio> // FILE*

// 클래스 전방 선언 (순환 참조 방지)
class Detector;
class Segmenter;
class DatabaseManager;
class SerialCommunicator;
class AnomalyDetector;
struct DetectionResult;

class StreamProcessor {
public:
    // 생성자: DB 매니저에 대한 참조를 받습니다.
    StreamProcessor(DatabaseManager& dbManager);
    ~StreamProcessor();

    // 이 함수를 호출하면 메인 루프가 시작됩니다.
    void run();

    bool isAnomalyDetected() const;

private:
    // 초기화 헬퍼 함수
    bool initialize_camera();
    bool initialize_streamers();

    void process_frame_and_stream(cv::Mat& frame);
    void handle_mode_change();
    void handle_anomaly_detection();  // 이상탐지 처리 함수 추가

    // 그리기 및 스트리밍
    void draw_and_stream_output(cv::Mat& frame, const std::vector<DetectionResult>& results);

    // 헬퍼 함수
    FILE* create_ffmpeg_process(const std::string& rtsp_url);
    std::string gstreamer_pipeline();

    // --- 멤버 변수 ---
    DatabaseManager& db_manager_;
    std::unique_ptr<SerialCommunicator> serial_comm_;
    std::unique_ptr<AnomalyDetector> anomaly_detector_;  // 이상탐지 객체 추가

    // 카메라 및 스트림 설정
    const int camera_id_ = 1;
    int capture_width_ = 320;
    int capture_height_ = 240;
    int framerate_ = 15;
    cv::VideoCapture cap_;
    FILE* proc_processed_ = nullptr;
    FILE* proc_raw_ = nullptr;
    std::string rtsp_url_processed_ = "rtsps://127.0.0.1:8555/processed";
    std::string rtsp_url_raw_ = "rtsps://127.0.0.1:8555/raw";

    // 모델 관리
    std::unique_ptr<Detector> detector_;
    std::unique_ptr<Segmenter> segmenter_;
    std::string last_loaded_mode_ = "none";
    std::string detection_model_path_ = "../models/best_test.onnx";
    std::string segmentation_model_path_ = "../models/yolo11n-seg.onnx";

    // 그리기 및 DB 저장 주기
    std::map<std::string, cv::Scalar> color_map_;
    time_t last_save_time_ = 0;
    
    // 이상탐지 관련
    std::atomic<bool> anomaly_detected_{false};
    time_t last_anomaly_check_ = 0;
    constexpr static int ANOMALY_CHECK_INTERVAL = 1;  // 1초마다 체크
};
