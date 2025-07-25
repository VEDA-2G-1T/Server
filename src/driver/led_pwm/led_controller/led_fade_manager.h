#pragma once
#include "led_pwm_controller.h"
#include <chrono>
#include <mutex>
#include <iostream>

class DebouncedFadeController {
public:
    explicit DebouncedFadeController(LedController& controller,
                                     uint64_t debounce_interval_ms = 300);

    void triggerFade();

private:
    LedController& controller_;
    std::chrono::steady_clock::time_point last_fade_time_;
    std::chrono::milliseconds debounce_interval_;
    std::mutex mutex_;
};
