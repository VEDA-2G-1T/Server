#pragma once
#include <cstdint>
#include <stdexcept>

class LedController {
public:
    static LedController& instance();

    void fade(uint64_t period_ns, uint64_t duration_ms, uint8_t steps, uint8_t polarity = 0);

    // 복사/이동 금지
    LedController(const LedController&) = delete;
    LedController& operator=(const LedController&) = delete;

private:
    explicit LedController(const char* device_path);
    ~LedController();

    int fd_;
};
