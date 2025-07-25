#include <stdio.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <stdint.h>
#include <unistd.h>

#define LEDPWM_IOC_MAGIC 'L'
#define LEDPWM_IOC_FADE _IOW(LEDPWM_IOC_MAGIC, 0x02, struct ledpwm_fade_req)

struct ledpwm_fade_req {
    uint64_t period_ns;
    uint64_t duration_ms;
    uint8_t  steps;
    uint8_t  polarity;
    uint8_t  reserved[6];
};

int main() {
    int fd = open("/dev/ledpwm0", O_RDWR);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    struct ledpwm_fade_req req = {
        .period_ns = 1000000,  
        .duration_ms = 300,   
        .steps = 50,
        .polarity = 0
    };

    if (ioctl(fd, LEDPWM_IOC_FADE, &req) < 0) {
        perror("ioctl");
        close(fd);
        return 1;
    }

    printf("✅ PWM fade requested.\n");
    close(fd);
    return 0;
}
