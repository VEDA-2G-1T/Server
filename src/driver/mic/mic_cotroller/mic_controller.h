// mic_controller.h
#ifndef MIC_CONTROLLER_H
#define MIC_CONTROLLER_H

#include <string>
#include <cstdint>

class MicController {
public:
    static MicController& getInstance();

    bool openDevice(const std::string& path = "/dev/adc_device");
    void closeDevice();
    bool readRaw(int16_t& outRaw);
    float toVoltage(int16_t raw) const;

private:
    MicController();                       // 생성자 비공개
    ~MicController();                      // 소멸자
    MicController(const MicController&) = delete;
    MicController& operator=(const MicController&) = delete;

    int fd_;                               // 파일 디스크립터
};

#endif // MIC_CONTROLLER_H
