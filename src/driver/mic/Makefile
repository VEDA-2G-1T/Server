# === 사용자 설정 ===
KO          := ads1115_driver.ko
DRV_NAME    := ads1115_driver
OVERLAY     := veda_mic
DTS         := $(OVERLAY).dts
DTBO        := $(OVERLAY).dtbo
KDIR        := /lib/modules/$(shell uname -r)/build
EXTRA_DIR   := /lib/modules/$(shell uname -r)/extra
OVERLAY_DIR := /boot/overlays
CONFIG_TXT  := /boot/config.txt

# 테스트 앱
C_SRC := ads1115_test.cpp
C_OUT := ads1115_test
CPPFLAGS := -Wall -O2

obj-m += ads1115_driver.o

# === 기본 타겟 ===
all: build-dtbo copy-dtbo build-ko install-ko persist-dtoverlay $(C_OUT)

# === 0. DTBO 빌드 ===
build-dtbo: $(DTBO)

$(DTBO): $(DTS)
	@echo "========= [0] Building DTBO from DTS ========="
	dtc -@ -I dts -O dtb -o $@ $<

# === 1. DTBO 복사 ===
copy-dtbo: $(DTBO)
	@echo "========= [1] Copying DTBO to /boot/overlays ========="
	sudo cp $(DTBO) $(OVERLAY_DIR)/
	@echo "[OK] $(DTBO) copied to $(OVERLAY_DIR)"


# === 1. 커널 모듈 빌드 ===
build-ko:
	@echo "========= [1] Building kernel module ========="
	@if [ ! -d "$(KDIR)" ]; then \
		echo "[ERROR] Kernel headers not found at $(KDIR)"; \
		exit 1; \
	fi
	$(MAKE) -C $(KDIR) M=$(PWD) modules

# === 2. 커널 모듈 설치 ===
install-ko: build-ko
	@echo "========= [2] Installing kernel module ========="
	sudo mkdir -p $(EXTRA_DIR)
	sudo cp $(KO) $(EXTRA_DIR)/
	sudo depmod
	@echo "[OK] Kernel module installed to $(EXTRA_DIR)"

# === 3. /boot/config.txt dtoverlay 추가 ===
persist-dtoverlay:
	@echo "========= [3] Ensuring dtoverlay in config.txt ========="
	@if grep -q "^dtoverlay=$(OVERLAY)" $(CONFIG_TXT); then \
		echo "[OK] dtoverlay already present"; \
	else \
		echo "dtoverlay=$(OVERLAY)" | sudo tee -a $(CONFIG_TXT); \
		echo "[OK] dtoverlay added to config.txt"; \
	fi

# === 4. 테스트 앱 빌드 ===
$(C_OUT): $(C_SRC)
	$(CXX) $(CPPFLAGS) -o $@ $^

# === 정리 ===
clean:
	@echo "========= Cleaning build artifacts ========="
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	$(RM) $(C_OUT)

.PHONY: all build-ko install-ko persist-dtoverlay clean
