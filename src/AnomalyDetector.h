#pragma once

#include <vector>
#include <atomic>
#include <thread>
#include <complex> 
#include <fftw3.h>  

#include "driver/mic/mic_cotroller/mic_controller.h"

// 이상탐지 기능 클래스
class AnomalyDetector {
public:
    AnomalyDetector();
    ~AnomalyDetector();
    
    // 이상탐지 시작/중지
    void start();
    void stop();
    
    // 이상탐지 상태 확인
    bool isAnomalyDetected() const;

private:
    // 이상 탐지 로직을 실행하는 메인 스레드 함수
    void run();
    
    // 오디오 데이터에서 통계치를 계산하는 헬퍼 함수
    std::pair<double, double> calculate_stats(const std::vector<double>& values);

    // --- 멤버 변수 ---
    MicController& mic_controller_; // MicController 싱글턴 인스턴스에 대한 참조

    std::thread thread_;
    std::atomic<bool> stop_flag_{false};
    std::atomic<bool> anomaly_detected_{false};

    // FFT(고속 푸리에 변환) 관련 멤버 변수
    const int fft_size_ = 1024; // FFT 분석을 위한 샘플 개수
    double* fft_in_;            // FFT 입력 버퍼
    fftw_complex* fft_out_;     // FFT 출력 버퍼
    fftw_plan fft_plan_;        // FFT 계획
}; 