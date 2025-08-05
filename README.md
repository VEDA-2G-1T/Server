# Server 🛰️

라즈베리파이에서 동작하는 백엔드 서버 시스템입니다. AI 모델을 통해 카메라 영상을 실시간으로 분석하고, 그 결과를 보안 채널을 통해 스트리밍하며, 원격 제어 및 데이터 조회를 위한 API를 제공합니다.

---
## 🚀 주요 기능

-   **실시간 AI 영상 분석:** 안전모/안전조끼 착용 여부(PPE), 넘어짐, 무단침입 등 다양한 이벤트를 실시간으로 탐지합니다.
-   **보안 스트리밍 및 통신:** RTSPS (RTSP over TLS) 및 HTTPS, WSS를 적용하여 모든 영상과 데이터를 암호화하여 전송합니다.
-   **실시간 양방향 제어:** WebSocket을 통해 클라이언트가 서버의 동작 모드를 즉시 변경하고, 서버는 탐지 이벤트를 실시간으로 클라이언트에 전송합니다.
-   **이벤트 데이터 관리:** 탐지된 모든 이벤트 내역을 기능별로 분리된 SQLite 데이터베이스에 저장하며, REST API를 통해 조회할 수 있습니다.
-   **하드웨어 연동:** STM32 마이크로컨트롤러와 연동하여 센서 값을 모니터링하고 LED, 부저 등 외부 장치를 제어합니다.
-   **시스템 모니터링:** 라즈베리파이의 CPU 및 메모리 사용률을 실시간으로 클라이언트에 전송하여 안정적인 운영을 지원합니다.

---
## 🛠️ 기술 스택 

### 하드웨어 
-   **메인 프로세서:** Raspberry Pi 4
-   **카메라:** Raspberry Pi Camera Module
-   **센서/액추에이터:** Custom board with STM32, Microphone (ADS1115 ADC), LED, Buzzer

### 소프트웨어 & 언어 
-   **언어:** C++, Shell Script
-   **운영체제:** Raspberry Pi OS (Linux)
-   **핵심 라이브러리:**
    -   **웹 서비스:** Crow (C++ HTTP/WebSocket Framework)
    -   **영상 처리:** OpenCV
    -   **신호 처리:** FFTW
    -   **데이터베이스:** SQLite3
    -   **하드웨어 통신:** libi2c-dev, termios (Serial)

### 인프라 & 프로토콜
-   **웹 서버:** Nginx (리버스 프록시)
-   **영상 스트리밍:** MediaMTX (RTSP/RTSPS Server), FFmpeg, GStreamer
-   **보안:** OpenSSL (TLS/SSL for HTTPS & RTSPS)
-   **통신 프로토콜:** HTTPS, WebSocket (WSS), RTSPS, UART, I2C

---

## 🏗️ 서버 시스템 아키텍처 

<img src="https://github.com/user-attachments/assets/e4fe2159-ff17-4fe8-88a9-2cd906cac17d" alt="프로젝트 구조" width="80%">

---
## 🛠️ 설치 및 설정 가이드

### 1. 의존성 패키지 설치

프로젝트 실행에 필요한 모든 라이브러리와 도구를 설치합니다.

```bash
# setup.sh 스크립트에 실행 권한 부여
chmod +x ./scripts/setup.sh

# 스크립트 실행
./scripts/setup.sh
```
### 2. 커널 드라이버 설치

이 프로젝트는 커스텀 커널 드라이버를 사용하여 하드웨어를 직접 제어합니다. 각 드라이버 디렉터리에서 `make`를 실행하여 드라이버를 빌드하고 설치합니다.

> **중요:** 커널 드라이버와 디바이스 트리 오버레이를 처음 설치한 후에는, 변경사항을 시스템에 완전히 적용하기 위해 **반드시 재부팅(`sudo reboot`)**을 해야 합니다.

#### 2.1. LED PWM 드라이버 (led_pwm)

```bash
# led_pwm 드라이버 디렉터리로 이동
cd src/driver/led_pwm

# make 명령어로 빌드, 설치, 시스템 설정까지 한 번에 실행
sudo make

# 프로젝트 루트 디렉터리로 복귀
cd ../../..
```

#### 2.2. 마이크 ADC 드라이버 (mic)

```bash
# mic 드라이버 디렉터리로 이동
cd src/driver/mic

# make 명령어로 빌드, 설치, 시스템 설정까지 한 번에 실행
sudo make

# 프로젝트 루트 디렉터리로 복귀
cd ../../..
```

### 3. RTSP 서버 (MediaMTX) 설정

RTSP 스트리밍 서버인 MediaMTX를 다운로드하고, 보안 스트리밍(RTSPS)을 위한 설정을 진행합니다.

```bash
# 1. MediaMTX v1.12.3 다운로드 및 압축 해제
wget https://github.com/bluenviron/mediamtx/releases/download/v1.12.3/mediamtx_v1.12.3_linux_armv7.tar.gz tar 
tar -xzf mediamtx_v1.12.3_linux_armv7.tar.gz

# 2. mediamtx.yml 파일 수정
# rtspEncryption 값을 "optional"로, rtspsAddress를 ":8555"로 설정합니다.
vim mediamtx.yml

# 3. RTSPS용 SSL 인증서 및 키 생성
openssl genrsa -out server.key 2048
openssl req -new -x509 -sha256 -key server.key -out server.crt -days 365

# 4. MediaMTX 실행 권한 부여
chmod +x ./mediamtx
```
> **mediamtx.yml 예시:**
> ```yaml
> rtspEncryption: "optional"
> rtspAddress: :8554
> rtspsAddress: :8555
> ```

```bash
# 2. RTSPS용 SSL 인증서 및 키 생성
openssl genrsa -out server.key 2048
openssl req -new -x509 -sha256 -key server.key -out server.crt -days 365

# 3. MediaMTX 실행 권한 부여
chmod +x ./mediamtx
```

### 4. Nginx 설정

리버스 프록시 역할을 할 Nginx를 설정합니다. 모든 외부 요청은 Nginx를 통해 백엔드 애플리케이션으로 전달됩니다.

```bash
# 1. Nginx 설정 파일 생성
sudo vim /etc/nginx/sites-available/my_server_app
```

> **설정 파일 내용:**
> ```nginx
> # /etc/nginx/sites-available/my_server_app
> server {
>     # 외부에서 접속할 대표 포트
>     listen 80;
>
>     # API 및 웹소켓 요청 처리
>     location /api/ {
>         proxy_pass http://127.0.0.1:9000;
>     }
>
>     # 탐지 이미지 파일 제공
>     location /captured_images/ {
>         # [수정 필요] '사용자명'을 실제 리눅스 사용자 이름으로 변경하세요.
>         alias /home/사용자명/Server/captured_images/;
>     }
> }
> ```

```bash
# 2. 설정 활성화 및 재시작
sudo ln -s /etc/nginx/sites-available/my_server_app /etc/nginx/sites-enabled/
sudo nginx -t  # 설정 파일 문법 검사
sudo systemctl restart nginx
```

### 5. 백엔드 앱 SSL 인증서 생성

C++ 백엔드 애플리케이션이 HTTPS 통신을 하는 데 사용할 자체 서명 인증서를 생성합니다.

```bash
# [수정 필요] IP 주소를 라즈베리파이의 실제 IP로 변경하세요.
openssl req -x509 -newkey rsa:2048 -sha256 -nodes -keyout key.pem -out cert.pem -days 365 -addext "subjectAltName = DNS:localhost,IP:192.168.0.xx"
```

---

## ⚙️ 빌드 및 실행

### 1. 빌드

프로젝트를 빌드하기 위해 `build.sh` 스크립트를 실행합니다.

```bash
chmod +x ./scripts/build.sh
./scripts/build.sh
```

### 2. 실행

빌드가 완료되면 `build` 디렉터리에 생성된 실행 파일을 실행합니다. 하드웨어 접근 권한을 위해 `sudo`로 실행하는 것을 권장합니다.

```bash
./build/pi_server
```