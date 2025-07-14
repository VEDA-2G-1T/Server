#!/bin/bash

# 스크립트 파일의 위치를 기준으로 프로젝트 루트 디렉토리 경로를 찾음
PROJECT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )/.." && pwd )"

echo "Project Directory: $PROJECT_DIR"
echo "Starting build..."

# g++ 컴파일 명령어 (경로가 변경됨)
g++ \
    -std=c++17 \
    -I "$PROJECT_DIR/external/Crow/include" \
    -I "$PROJECT_DIR/external/json/single_include/nlohmann" \
    "$PROJECT_DIR/src/photo_backend.cpp" \
    -o "$PROJECT_DIR/build/pi_server" \
    -lpthread \
    -lstdc++fs

# $? 변수는 바로 직전에 실행된 명령어의 종료 코드를 담고 있음 (0이면 성공)
if [ $? -eq 0 ]; then
    echo "Build successful! Executable is at build/pi_server"
else
    echo "Build failed."
fi