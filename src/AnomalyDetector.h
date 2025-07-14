#pragma once

#include <vector>
#include <deque>
#include <mutex>
#include <atomic>
#include <thread>
#include <memory>
#include <fftw3.h>

// ADS1115 ADC 클래스
class ADS1115 {
    int fd;
    uint8_t addr = 0x48;
public:
    ADS1115(const char* bus = "/dev/i2c-1");
    ~ADS1115();
    int16_t readRaw(uint8_t ch = 0);
};

// 이상탐지 기능 클래스
class AnomalyDetector {
public:
    AnomalyDetector();
    ~AnomalyDetector();
    
    // 이상탐지 시작/중지
    void start();
    void stop();
    
    // 이상탐지 상태 확인
    bool isAnomalyDetected() const { return anomaly_detected_.load(); }
    
    // 설정값 변경
    void setThreshold(double threshold) { z_threshold_ = threshold; }
    void setCalibrationFrames(int frames) { cali_frames_ = frames; }

private:
    // ADC 관련
    std::unique_ptr<ADS1115> adc_;
    
    // FFT 관련
    fftw_plan plan_;
    fftw_complex* fft_out_;
    std::vector<double> hamming_win_;
    
    // 설정값
    constexpr static int SAMPLE_RATE = 860;      // ADS1115 최대 싱글샷 SPS
    constexpr static int WIN_SZ = 256;           // FFT 윈도우 크기
    constexpr static int HOP_SZ = 128;           // 홉 크기
    constexpr static double HF_CUTOFF = 50.0;    // HF 컷오프 (Hz)
    constexpr static int MAX_BUF_SAMPS = WIN_SZ * 8;
    
    int cali_frames_ = 10;                       // 캘리브레이션 윈도우 개수
    double z_threshold_ = 3.0;                   // Z-score 임계값
    
    // 상태 관리
    std::atomic<bool> running_{false};
    std::atomic<bool> anomaly_detected_{false};
    std::thread detection_thread_;
    
    // 데이터 버퍼
    std::deque<double> buf_;
    std::mutex buf_mutex_;
    
    // 캘리브레이션 통계
    double hf_mean_ = 0.0, hf_std_ = 1.0;
    double flux_mean_ = 0.0, flux_std_ = 1.0;
    
    // 내부 함수들
    void detection_loop();
    void init_hamming_window();
    struct Feature { double hf_ratio, flux; };
    Feature extract_features(const std::vector<double>& window, const std::vector<double>& prev_spectrum);
    std::pair<double, double> calculate_stats(const std::vector<double>& values);
    void calibrate();
}; 