#include "mic_controller.h"
#include <iostream>
#include <unistd.h>

// g++ -Wall -O2 mic_controller.cpp main.cpp -o adc_test

int main() {
    auto& mic = MicController::getInstance();
    if (!mic.openDevice()) return 1;

    for (int i = 0; i < 20; ++i) {
        int16_t raw;
        if (!mic.readRaw(raw)) break;

        float voltage = mic.toVoltage(raw);
        std::cout << "Raw: " << raw << ", Voltage: " << voltage << " V\n";
        usleep(100000);
    }

    mic.closeDevice();
    return 0;
}
