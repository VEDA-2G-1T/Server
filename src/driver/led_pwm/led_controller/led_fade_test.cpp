#include "led_fade_manager.h"
#include <thread>
// g++ -std=c++17 -o led_test led_fade_test.cpp led_fade_manager.cpp led_pwm_controller.cpp -lpthread

int main() {
    try {
        DebouncedFadeController fade_controller(LedController::instance(), 300);

        while (true) {
            fade_controller.triggerFade();
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        fade_controller.triggerFade();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        fade_controller.triggerFade();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        fade_controller.triggerFade();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        fade_controller.triggerFade();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        fade_controller.triggerFade();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        fade_controller.triggerFade();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        fade_controller.triggerFade();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        fade_controller.triggerFade();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        fade_controller.triggerFade();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        fade_controller.triggerFade();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        
        
    } catch (const std::exception& e) {
        std::cerr << "[Error] " << e.what() << std::endl;
        return 1;
    }

    return 0;
}



// 캐벛 -> 감지 -> 부저들이랑 스피커 + stm led + led  거의 무한루프식으로 