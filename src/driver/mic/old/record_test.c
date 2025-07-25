#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <string.h>

#define I2C_DEV "/dev/i2c-1"
#define ADS1115_ADDR 0x48
#define SAMPLE_RATE 860
#define RECORD_SECONDS 5
#define SAMPLES (SAMPLE_RATE * RECORD_SECONDS)

// ADS1115 레지스터
#define REG_CONVERSION 0x00
#define REG_CONFIG     0x01

#define CONFIG_MSB 0b11000010
#define CONFIG_LSB 0b11100011

int16_t read_adc_a0(int fd) {
    uint8_t config[3] = {REG_CONFIG, CONFIG_MSB, CONFIG_LSB};

    if (write(fd, config, 3) != 3) {
        perror("Failed to write config");
        exit(1);
    }

    usleep(2000); // ADS1115 conversion time

    uint8_t reg = REG_CONVERSION;
    if (write(fd, &reg, 1) != 1) {
        perror("Failed to request conversion");
        exit(1);
    }

    uint8_t result[2];
    if (read(fd, result, 2) != 2) {
        perror("Failed to read result");
        exit(1);
    }

    return (result[0] << 8) | result[1];
}

void write_wav_header(FILE *fp, int sample_rate, int num_samples) {
    int byte_rate = sample_rate * 2; // mono 16bit
    int data_chunk_size = num_samples * 2;
    int fmt_chunk_size = 16;
    int riff_chunk_size = 4 + (8 + fmt_chunk_size) + (8 + data_chunk_size);

    // RIFF header
    fwrite("RIFF", 1, 4, fp);
    fwrite(&riff_chunk_size, 4, 1, fp);
    fwrite("WAVE", 1, 4, fp);

    // fmt subchunk
    fwrite("fmt ", 1, 4, fp);
    uint32_t subchunk1_size = 16;
    uint16_t audio_format = 1;
    uint16_t num_channels = 1;
    uint16_t bits_per_sample = 16;
    uint16_t block_align = num_channels * bits_per_sample / 8;

    fwrite(&subchunk1_size, 4, 1, fp);
    fwrite(&audio_format, 2, 1, fp);
    fwrite(&num_channels, 2, 1, fp);
    fwrite(&sample_rate, 4, 1, fp);
    fwrite(&byte_rate, 4, 1, fp);
    fwrite(&block_align, 2, 1, fp);
    fwrite(&bits_per_sample, 2, 1, fp);

    // data subchunk
    fwrite("data", 1, 4, fp);
    fwrite(&data_chunk_size, 4, 1, fp);
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

    FILE *fp = fopen("recorded_adc.wav", "wb");
    if (!fp) {
        perror("Failed to open output file");
        return 1;
    }

    // Write WAV header placeholder
    write_wav_header(fp, SAMPLE_RATE, SAMPLES);

    printf("Recording %d samples...\n", SAMPLES);

    for (int i = 0; i < SAMPLES; i++) {
        int16_t raw = read_adc_a0(fd);
        fwrite(&raw, 2, 1, fp);
        usleep(1000000 / SAMPLE_RATE); // ~1.16ms for 860Hz
    }

    fclose(fp);
    close(fd);
    printf("Saved to recorded_adc.wav\n");

    return 0;
}
