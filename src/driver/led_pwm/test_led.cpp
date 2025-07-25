#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <cstring>
#include <stdexcept>
#include <cstdint>

#define LEDPWM_IOC_MAGIC 'L'
#define LEDPWM_IOC_FADE _IOW(LEDPWM_IOC_MAGIC, 0x02, ledpwm_fade_req)

struct ledpwm_fade_req {
    uint64_t period_ns;   
    uint64_t duration_ms; 
    uint8_t  steps;       
    uint8_t  polarity;    
    uint8_t  reserved[6]; 
};

class LedPwmController {
public:
    explicit LedPwmController(const char* device_path = "/dev/ledpwm0") {
        fd_ = open(device_path, O_RDWR);
        if (fd_ < 0) {
            throw std::runtime_error("Failed to open device: " + std::string(device_path));
        }
    }

    ~LedPwmController() {
        if (fd_ >= 0) close(fd_);
    }

    void fade(uint64_t period_ns, uint64_t duration_ms, uint8_t steps, uint8_t polarity = 0) {
        ledpwm_fade_req req{};
        req.period_ns   = period_ns;
        req.duration_ms = duration_ms;
        req.steps       = steps;
        req.polarity    = polarity;

        if (ioctl(fd_, LEDPWM_IOC_FADE, &req) < 0) {
            throw std::runtime_error("ioctl(LEDPWM_IOC_FADE) failed");
        }
    }

private:
    int fd_;
};

int main() {
    try {
        LedPwmController led;

        
        uint64_t period_ns   = 500'000;  
        uint64_t duration_ms = 300;       
        uint8_t  steps        = 30;
        uint8_t  polarity     = 0;

        std::cout << "Starting LED fade..." << std::endl;
        led.fade(period_ns, duration_ms, steps, polarity);
        std::cout << "Fade complete." << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "[Error] " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
