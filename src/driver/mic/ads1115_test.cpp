#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <cstdint>
#include <cstring>
#include <cmath>

#define DEVICE_PATH "/dev/adc_device"
// #define PGA_RANGE 4.096f   // ±4.096V
#define PGA_RANGE 3.3f   // ±4.096V
#define ADC_RESOLUTION 32768.0f  // 16-bit signed

float to_voltage(int16_t raw) {
    return (raw * PGA_RANGE) / ADC_RESOLUTION;
}

int main() {
    int fd = open(DEVICE_PATH, O_RDONLY);
    if (fd < 0) {
        std::perror("Failed to open device");
        return 1;
    }

    std::cout << "Reading ADC values from " << DEVICE_PATH << "...\n";

    for (int i = 0; i < 1000000; ++i) {
        int16_t raw = 0;
        ssize_t bytes = read(fd, &raw, sizeof(raw));
        if (bytes != sizeof(raw)) {
            std::perror("Failed to read ADC");
            break;
        }

        float voltage = to_voltage(raw);
        std::cout << "Raw: " << raw << ", Voltage: " << voltage << " V\n";
        usleep(100000);  // 100ms
    }

    close(fd);
    return 0;
}
