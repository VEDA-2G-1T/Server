#include "led_fade_manager.h"

DebouncedFadeController::DebouncedFadeController(LedController& controller,
                                                 uint64_t debounce_interval_ms)
    : controller_(controller),
      last_fade_time_(std::chrono::steady_clock::now() - std::chrono::milliseconds(debounce_interval_ms)),
      debounce_interval_(std::chrono::milliseconds(debounce_interval_ms)) {}

void DebouncedFadeController::triggerFade() {
    auto now = std::chrono::steady_clock::now();
    std::lock_guard<std::mutex> lock(mutex_);
    if (now - last_fade_time_ >= debounce_interval_) {
        std::cout << "[LED Fade Triggered] at "
                  << std::chrono::duration_cast<std::chrono::milliseconds>(
                         now.time_since_epoch()).count()
                  << " ms" << std::endl;

        controller_.fade(500'000, 300, 30, 0);
        last_fade_time_ = now;
    }
}
