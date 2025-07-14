#include "AnomalyDetector.h"
#include <iostream>
#include <iomanip>
#include <numeric>
#include <cmath>
#include <unistd.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <stdexcept>

// --- ADS1115 클래스 구현 ---
ADS1115::ADS1115(const char* bus) {
    if ((fd = open(bus, O_RDWR)) < 0) {
        throw std::runtime_error("I2C 버스 열기 실패");
    }
    if (ioctl(fd, I2C_SLAVE, addr) < 0) {
        throw std::runtime_error("ADS1115 접근 실패");
    }
}

ADS1115::~ADS1115() {
    if (fd >= 0) close(fd);
}

int16_t ADS1115::readRaw(uint8_t ch) {
    uint8_t cfg[3];
    cfg[0] = 0x01;
    cfg[1] = 0b11000011 | ((ch & 3) << 4);
    cfg[2] = 0b11100011;
    write(fd, cfg, 3);
    usleep(1200);
    uint8_t ptr = 0; 
    write(fd, &ptr, 1);
    uint8_t d[2]; 
    read(fd, d, 2);
    return (int16_t)((d[0] << 8) | d[1]);
}

// --- AnomalyDetector 클래스 구현 ---
AnomalyDetector::AnomalyDetector() {
    try {
        adc_ = std::make_unique<ADS1115>();
        fft_out_ = (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * (WIN_SZ / 2 + 1));
        plan_ = fftw_plan_dft_r2c_1d(WIN_SZ, nullptr, fft_out_, FFTW_ESTIMATE);
        init_hamming_window();
    } catch (const std::exception& e) {
        std::cerr << "ADC 초기화 실패: " << e.what() << std::endl;
    }
}

AnomalyDetector::~AnomalyDetector() {
    stop();
    if (plan_) fftw_destroy_plan(plan_);
    if (fft_out_) fftw_free(fft_out_);
}

void AnomalyDetector::init_hamming_window() {
    if (hamming_win_.empty()) {
        hamming_win_.resize(WIN_SZ);
        for (int i = 0; i < WIN_SZ; ++i) {
            hamming_win_[i] = 0.54 - 0.46 * cos(2 * M_PI * i / (WIN_SZ - 1));
        }
    }
}

void AnomalyDetector::start() {
    if (!adc_) {
        std::cerr << "ADC가 초기화되지 않았습니다." << std::endl;
        return;
    }
    
    if (running_.load()) return;
    
    running_ = true;
    anomaly_detected_ = false;
    
    // 캘리브레이션 수행
    calibrate();
    
    // 탐지 스레드 시작
    detection_thread_ = std::thread(&AnomalyDetector::detection_loop, this);
    std::cout << "이상탐지 시작됨" << std::endl;
}

void AnomalyDetector::stop() {
    if (!running_.load()) return;
    
    running_ = false;
    if (detection_thread_.joinable()) {
        detection_thread_.join();
    }
    std::cout << "이상탐지 중지됨" << std::endl;
}

void AnomalyDetector::calibrate() {
    std::cout << "배경 캘리브레이션 중..." << std::endl;
    
    std::vector<double> hf_values, flux_values;
    std::vector<double> prev_spectrum;
    
    std::vector<double> window(WIN_SZ);
    for (int frame = 0; frame < cali_frames_; ++frame) {
        for (int i = 0; i < WIN_SZ; ++i) {
            int16_t raw = adc_->readRaw();
            double voltage = raw * (4.096 / 32768.0);
            window[i] = voltage;
            
            // 버퍼에 추가
            {
                std::lock_guard<std::mutex> lock(buf_mutex_);
                buf_.push_back(voltage);
                if (buf_.size() > MAX_BUF_SAMPS) buf_.pop_front();
            }
            
            usleep(1000000 / SAMPLE_RATE);
        }
        
        Feature feat = extract_features(window, prev_spectrum);
        hf_values.push_back(feat.hf_ratio);
        flux_values.push_back(feat.flux);
        
        // prev_spectrum 갱신
        int spec_size = WIN_SZ / 2 + 1;
        prev_spectrum.resize(spec_size);
        for (int k = 0; k < spec_size; ++k) {
            prev_spectrum[k] = fft_out_[k][0] * fft_out_[k][0] + fft_out_[k][1] * fft_out_[k][1];
        }
    }
    
    auto [hf_mean, hf_std] = calculate_stats(hf_values);
    auto [flux_mean, flux_std] = calculate_stats(flux_values);
    
    hf_mean_ = hf_mean;
    hf_std_ = hf_std;
    flux_mean_ = flux_mean;
    flux_std_ = flux_std;
    
    std::cout << std::fixed << std::setprecision(4)
              << "캘리브레이션 완료:\n"
              << " HF μ=" << hf_mean_ << " σ=" << hf_std_
              << "\n FL μ=" << flux_mean_ << " σ=" << flux_std_ << std::endl;
}

void AnomalyDetector::detection_loop() {
    std::vector<double> window(WIN_SZ), current_spectrum;
    std::vector<double> prev_spectrum;
    int consecutive_anomalies = 0;
    
    while (running_.load()) {
        for (int i = 0; i < WIN_SZ; ++i) {
            if (!running_.load()) break;
            
            int16_t raw = adc_->readRaw();
            double voltage = raw * (4.096 / 32768.0);
            window[i] = voltage;
            
            // 버퍼에 추가
            {
                std::lock_guard<std::mutex> lock(buf_mutex_);
                buf_.push_back(voltage);
                if (buf_.size() > MAX_BUF_SAMPS) buf_.pop_front();
            }
            
            usleep(1000000 / SAMPLE_RATE);
        }
        
        Feature feat = extract_features(window, prev_spectrum);
        
        // prev_spectrum 갱신
        int spec_size = WIN_SZ / 2 + 1;
        current_spectrum.resize(spec_size);
        for (int k = 0; k < spec_size; ++k) {
            current_spectrum[k] = fft_out_[k][0] * fft_out_[k][0] + fft_out_[k][1] * fft_out_[k][1];
        }
        prev_spectrum.swap(current_spectrum);
        
        // Z-score 계산
        double z_hf = (feat.hf_ratio - hf_mean_) / (hf_std_ > 0 ? hf_std_ : 1.0);
        double z_flux = (feat.flux - flux_mean_) / (flux_std_ > 0 ? flux_std_ : 1.0);
        
        bool anomaly = (z_hf > z_threshold_ && z_flux > z_threshold_);
        
        if (anomaly) {
            if (++consecutive_anomalies >= 3) {
                anomaly_detected_ = true;
                std::cout << "\n🚨 ANOMALY DETECTED! zHF=" << z_hf << " zFL=" << z_flux << std::endl;
            }
        } else {
            if (consecutive_anomalies >= 3) {
                std::cout << "<<< 정상 회복" << std::endl;
                anomaly_detected_ = false;
            }
            consecutive_anomalies = 0;
        }
        
        std::cout << "\rHF:" << feat.hf_ratio << "/" << hf_mean_
                  << " FL:" << feat.flux << "/" << flux_mean_ << "    " << std::flush;
    }
}

AnomalyDetector::Feature AnomalyDetector::extract_features(const std::vector<double>& window, const std::vector<double>& prev_spectrum) {
    init_hamming_window();
    
    static std::vector<double> input;
    input.resize(WIN_SZ);
    for (int i = 0; i < WIN_SZ; ++i) {
        input[i] = window[i] * hamming_win_[i];
    }
    
    fftw_execute_dft_r2c(plan_, input.data(), fft_out_);
    
    int spec_size = WIN_SZ / 2 + 1;
    double total_energy = 0, hf_energy = 0;
    int hf_bin = int(HF_CUTOFF * WIN_SZ / SAMPLE_RATE);
    
    std::vector<double> power(spec_size);
    for (int k = 0; k < spec_size; ++k) {
        double magnitude = std::hypot(fft_out_[k][0], fft_out_[k][1]);
        double p = magnitude * magnitude;
        power[k] = p;
        total_energy += p;
        if (k >= hf_bin) hf_energy += p;
    }
    
    double hf_ratio = (total_energy > 0 ? hf_energy / total_energy : 0);
    double flux = 0;
    
    if (prev_spectrum.size() == spec_size) {
        for (int k = 0; k < spec_size; ++k) {
            double diff = power[k] - prev_spectrum[k];
            if (diff > 0) flux += diff;
        }
    }
    
    return {hf_ratio, flux};
}

std::pair<double, double> AnomalyDetector::calculate_stats(const std::vector<double>& values) {
    double mean = std::accumulate(values.begin(), values.end(), 0.0) / values.size();
    double variance = 0;
    for (double x : values) {
        variance += (x - mean) * (x - mean);
    }
    return {mean, std::sqrt(variance / values.size())};
} 