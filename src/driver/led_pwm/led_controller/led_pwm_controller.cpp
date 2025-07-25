#include "led_pwm_controller.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <cstring>
#include <stdexcept>

#define LEDPWM_IOC_MAGIC 'L'
#define LEDPWM_IOC_FADE _IOW(LEDPWM_IOC_MAGIC, 0x02, struct ledpwm_fade_req)

struct ledpwm_fade_req {
    uint64_t period_ns;
    uint64_t duration_ms;
    uint8_t  steps;
    uint8_t  polarity;
    uint8_t  reserved[6];
};

LedController& LedController::instance() {
    static LedController instance("/dev/ledpwm0");
    return instance;
}

LedController::LedController(const char* device_path) {
    fd_ = open(device_path, O_RDWR);
    if (fd_ < 0) {
        throw std::runtime_error("Failed to open device: " + std::string(device_path));
    }
}

LedController::~LedController() {
    if (fd_ >= 0) close(fd_);
}

void LedController::fade(uint64_t period_ns, uint64_t duration_ms, uint8_t steps, uint8_t polarity) {
    ledpwm_fade_req req{};
    req.period_ns   = period_ns;
    req.duration_ms = duration_ms;
    req.steps       = steps;
    req.polarity    = polarity;

    if (ioctl(fd_, LEDPWM_IOC_FADE, &req) < 0) {
        throw std::runtime_error("ioctl(LEDPWM_IOC_FADE) failed");
    }
}
