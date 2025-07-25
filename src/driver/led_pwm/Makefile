# === 사용자 설정 ===
DTS     := led_pwm.dts
DTBO    := led_pwm.dtbo
KO      := led_pwm_dev.ko
OVERLAY := led_pwm
KDIR    := /lib/modules/$(shell uname -r)/build
EXTRA_DIR := /lib/modules/$(shell uname -r)/extra
CONFIG_TXT := /boot/config.txt
OVERLAY_PATH := /boot/overlays
obj-m += led_pwm_dev.o

# === 파일 및 타겟 설정 ===
C_SRC := test_led.c
CPP_SRC := test_led.cpp

C_OUT := test_led_c
CPP_OUT := test_led_cpp

CFLAGS := -Wall -O2
CPPFLAGS := -Wall -O2


# === 기본 타겟 ===
all: build-dtbo copy-dtbo build-ko install-ko persist-dtoverlay  $(C_OUT) $(CPP_OUT)

# === 0. led_pwm.dts → led_pwm.dtbo 빌드 ===
build-dtbo: $(DTBO)

$(DTBO): $(DTS)
	@echo "========= [0] Building DTBO from DTS ========="
	dtc -@ -I dts -O dtb -o $@ $<

# === 1. DTBO 복사 ===
copy-dtbo: $(DTBO)
	@echo "========= [1] Copying DTBO to overlays ========="
	sudo cp $(DTBO) $(OVERLAY_PATH)/
	@echo "[OK] $(DTBO) copied to $(OVERLAY_PATH)"

# === 2. 커널 모듈 빌드 ===
build-ko:
	@echo "========= [2] Building kernel module ========="
	@if [ ! -d "$(KDIR)" ]; then \
		echo "[ERROR] Kernel headers not found at $(KDIR)"; \
		exit 1; \
	fi
	$(MAKE) -C $(KDIR) M=$(PWD) modules

# === 3. 커널 모듈 설치 ===
install-ko: build-ko
	@echo "========= [3] Installing kernel module ========="
	sudo mkdir -p $(EXTRA_DIR)
	sudo cp $(KO) $(EXTRA_DIR)/
	sudo depmod
	@echo "[OK] Kernel module installed to $(EXTRA_DIR)"

# === 4. config.txt 에 dtoverlay 추가 ===
persist-dtoverlay:
	@echo "========= [4] Ensuring overlay is persistent ========="
	@if grep -q "^dtoverlay=$(OVERLAY)" $(CONFIG_TXT); then \
		echo "[OK] dtoverlay already present in config.txt"; \
	else \
		echo "dtoverlay=$(OVERLAY)" | sudo tee -a $(CONFIG_TXT); \
		echo "[OK] dtoverlay added to config.txt"; \
	fi
test-file: $(C_OUT) $(CPP_OUT)

# === C 컴파일 ===
$(C_OUT): $(C_SRC)
	$(CC) $(CFLAGS) -o $@ $^

# === C++ 컴파일 ===
$(CPP_OUT): $(CPP_SRC)
	$(CXX) $(CPPFLAGS) -o $@ $^


# === 정리 ===
clean:
	@echo "========= Cleaning build artifacts ========="
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	$(RM) $(DTBO)
	$(RM) $(C_OUT) $(CPP_OUT)
.PHONY: all build-dtbo copy-dtbo build-ko install-ko persist-dtoverlay clean
