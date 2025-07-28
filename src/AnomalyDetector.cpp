#include "AnomalyDetector.h"
#include <iostream>
#include <cmath>
#include <numeric>
#include <stdexcept>

// 생성자
AnomalyDetector::AnomalyDetector() : mic_controller_(MicController::getInstance()) {
    // MicController 디바이스 열기
    if (!mic_controller_.openDevice()) {
        // 디바이스 열기 실패 시, 예외를 던져서 객체 생성이 실패했음을 알림
        throw std::runtime_error("AnomalyDetector: Failed to open mic device (/dev/adc_device).");
    }

    // FFTW3 라이브러리 초기화
    fft_in_ = (double*)fftw_malloc(sizeof(double) * fft_size_);
    fft_out_ = (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * (fft_size_ / 2 + 1));
    fft_plan_ = fftw_plan_dft_r2c_1d(fft_size_, fft_in_, fft_out_, FFTW_ESTIMATE);
}

// 소멸자
AnomalyDetector::~AnomalyDetector() {
    stop(); // 스레드가 실행 중이면 안전하게 종료

    // FFTW3 리소스 해제
    fftw_destroy_plan(fft_plan_);
    fftw_free(fft_in_);
    fftw_free(fft_out_);

    // MicController는 프로그램 종료 시 자동으로 소멸자에서 closeDevice()가 호출
}

void AnomalyDetector::start() {
    if (thread_.joinable()) return; // 이미 시작되었다면 무시
    stop_flag_ = false;
    thread_ = std::thread(&AnomalyDetector::run, this);
}

void AnomalyDetector::stop() {
    stop_flag_ = true;
    if (thread_.joinable()) {
        thread_.join();
    }
}

bool AnomalyDetector::isAnomalyDetected() const {
    return anomaly_detected_.load();
}

// 이상 탐지를 실행하는 메인 루프 (별도 스레드에서 실행)
void AnomalyDetector::run() {
    std::vector<double> audio_buffer;
    audio_buffer.reserve(fft_size_);

    while (!stop_flag_) {
        int16_t raw_value;
        // MicController를 통해 ADC raw 값을 읽어옴
        if (mic_controller_.readRaw(raw_value)) {
            audio_buffer.push_back(static_cast<double>(raw_value));

            // 버퍼가 FFT 분석에 필요한 만큼 채워졌는지 확인합니다.
            if (audio_buffer.size() >= fft_size_) {
                // 1. FFT 입력 버퍼에 데이터 복사
                for (int i = 0; i < fft_size_; ++i) {
                    fft_in_[i] = audio_buffer[i];
                }

                // 2. FFT 실행
                fftw_execute(fft_plan_);

                // 3. 주파수 대역별 에너지 계산 (예시: 1kHz ~ 2kHz 대역)
                double target_energy = 0.0;
                // 실제 주파수 계산: freq = i * (sample_rate / fft_size)
                // 예시에서는 특정 인덱스 범위의 에너지를 계산
                for (int i = 100; i < 200; ++i) { 
                    double real = fft_out_[i][0];
                    double imag = fft_out_[i][1];
                    target_energy += (real * real + imag * imag);
                }

                // 4. 임계값 기반으로 이상 상태 판단
                const double ANOMALY_THRESHOLD = 0.0005; // ★★★ 실제 환경에서 테스트하며 조절해야 함
                if (target_energy > ANOMALY_THRESHOLD) {
                    anomaly_detected_ = true;
                    std::cout << "[ANOMALY] High energy detected in target frequency band!" << std::endl;
                } else {
                    anomaly_detected_ = false;
                }
                
                // 다음 분석을 위해 버퍼를 비움
                audio_buffer.clear();
            }
        } else {
            // 읽기 실패 시 잠시 대기
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
}

// (이 함수는 현재 run() 루프에서 직접 사용되지는 않지만, 다른 분석을 위해 남겨둠)
std::pair<double, double> AnomalyDetector::calculate_stats(const std::vector<double>& values) {
    if (values.empty()) {
        return {0.0, 0.0};
    }
    double sum = std::accumulate(values.begin(), values.end(), 0.0);
    double mean = sum / values.size();
    double sq_sum = std::inner_product(values.begin(), values.end(), values.begin(), 0.0);
    double std_dev = std::sqrt(sq_sum / values.size() - mean * mean);
    return {mean, std_dev};
}