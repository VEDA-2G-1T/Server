#!/bin/bash

# 스크립트 실행 중 오류가 발생하면 즉시 중단
set -e

# 1. 패키지 목록 업데이트
echo "--- 패키지 목록을 업데이트합니다... ---"
sudo apt update

# 2. 필요한 모든 패키지 설치
echo "--- 필수 패키지를 설치합니다... ---"
sudo apt install -y \
    build-essential \
    cmake \
    git \
    pkg-config \
    libopencv-dev \
    libsqlite3-dev \
    gstreamer1.0-plugins-bad \
    libgstreamer1.0-dev \
    libgstreamer-plugins-base1.0-dev \
    libcamera-dev \
    libcamera-apps \
    libfftw3-dev \
    libgpiod-dev \
    libcurl4-openssl-dev \
    meson \
    ninja-build \
    libasio-dev \
    portaudio19-dev \
    nginx \
    vim \
    ufw

echo "🎉 모든 패키지 설치가 완료되었습니다."
