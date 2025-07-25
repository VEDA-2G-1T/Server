// mic_controller.cpp
#include "mic_controller.h"
#include <fcntl.h>
#include <unistd.h>
#include <iostream>
#include <cstring>

#define PGA_RANGE 4.096f
#define ADC_RESOLUTION 32768.0f

MicController::MicController() : fd_(-1) {}

MicController::~MicController() {
    closeDevice();
}

MicController& MicController::getInstance() {
    static MicController instance;
    return instance;
}

bool MicController::openDevice(const std::string& path) {
    if (fd_ >= 0) return true;  // 이미 열림

    fd_ = open(path.c_str(), O_RDONLY);
    if (fd_ < 0) {
        std::perror("Failed to open ADC device");
        return false;
    }
    return true;
}

void MicController::closeDevice() {
    if (fd_ >= 0) {
        close(fd_);
        fd_ = -1;
    }
}

bool MicController::readRaw(int16_t& outRaw) {
    if (fd_ < 0) return false;

    ssize_t bytes = read(fd_, &outRaw, sizeof(outRaw));
    if (bytes != sizeof(outRaw)) {
        std::perror("Failed to read from ADC device");
        return false;
    }
    return true;
}

float MicController::toVoltage(int16_t raw) const {
    return (raw * PGA_RANGE) / ADC_RESOLUTION;
}
