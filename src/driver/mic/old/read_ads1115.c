#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>

#define I2C_DEV "/dev/i2c-1"
#define ADS1115_ADDR 0x48

// ADS1115 레지스터
#define REG_CONVERSION 0x00
#define REG_CONFIG     0x01

// 설정값 (A0 입력, FS ±4.096V, 860SPS)
#define CONFIG_MSB 0b11000010  // OS=1 (start single), MUX=100 (A0-GND), PGA=001 (±4.096V)
#define CONFIG_LSB 0b11100011  // Mode=1 (single), 860SPS, Disable comparator

int16_t read_adc_a0(int fd) {
    uint8_t config[3];

    // 설정 레지스터 작성
    config[0] = REG_CONFIG;
    config[1] = CONFIG_MSB;
    config[2] = CONFIG_LSB;

    if (write(fd, config, 3) != 3) {
        perror("Failed to write config");
        exit(1);
    }

    // 변환 시간 대기 (860SPS 기준 최소 약 1.2ms)
    usleep(2000);

    // 변환 레지스터 요청
    uint8_t reg = REG_CONVERSION;
    if (write(fd, &reg, 1) != 1) {
        perror("Failed to request conversion");
        exit(1);
    }

    // 결과 읽기 (2바이트)
    uint8_t result[2];
    if (read(fd, result, 2) != 2) {
        perror("Failed to read result");
        exit(1);
    }

    int16_t value = (result[0] << 8) | result[1];
    return value;
}

float to_voltage(int16_t value) {
    // ±4.096V → 16bit = 32768
    return (value * 4.096f) / 32768.0f;
}

#define VOLTAGE_MIN 0.0f
#define VOLTAGE_MAX 3.3f  // MAX4466 출력은 VCC 기준, 일반적으로 3.3V

#define BAR_WIDTH 20  // 출력 너비 (20칸 막대)

void print_volume_bar(float voltage) {
    float ratio = (voltage - VOLTAGE_MIN) / (VOLTAGE_MAX - VOLTAGE_MIN);
    if (ratio < 0) ratio = 0;
    if (ratio > 1) ratio = 1;

    int bars = (int)(ratio * BAR_WIDTH);

    printf("\r[");
    for (int i = 0; i < BAR_WIDTH; ++i) {
        printf(i < bars ? "#" : " ");
    }
    printf("] %.3f V", voltage);
    fflush(stdout);
}



int main() {
    int fd = open(I2C_DEV, O_RDWR);
    if (fd < 0) {
        perror("Failed to open I2C device");
        return 1;
    }

    if (ioctl(fd, I2C_SLAVE, ADS1115_ADDR) < 0) {
        perror("Failed to set I2C address");
        return 1;
    }

    FILE *fp = fopen("adc_log.csv", "w");
    if (!fp) {
        perror("Failed to open output file");
        return 1;
    }

    fprintf(fp, "index,voltage\n");


    int index = 0;

    while (index < 20000) {  // 예: 1000개 샘플 저장
        int16_t raw = read_adc_a0(fd);
        float voltage = to_voltage(raw);
        print_volume_bar(voltage);  // ✅ 여기서 막대 그래프 출력
        fprintf(fp, "%d,%.5f\n", index, voltage);
        index++;
        usleep(1200); // 860 SPS에 맞춰서
    }
    fclose(fp);
    close(fd);
    return 0;
}
