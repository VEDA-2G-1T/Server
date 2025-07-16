#!/bin/bash

# --- 스크립트 설정 ---
# 스크립트가 있는 위치(scripts)의 한 단계 상위 폴더를 프로젝트 루트로 설정합니다.
# 이렇게 하면 어느 위치에서 스크립트를 실행하든 항상 올바르게 동작합니다.
PROJECT_ROOT="$( cd "$( dirname "${BASH_SOURCE[0]}" )/.." &> /dev/null && pwd )"
BUILD_DIR="${PROJECT_ROOT}/build"

# --- 빌드 시작 ---
echo "========================================"
echo "Project Root: ${PROJECT_ROOT}"
echo "Build Directory: ${BUILD_DIR}"
echo "========================================"

# 1. build 폴더가 없으면 새로 생성합니다.
mkdir -p "${BUILD_DIR}"

# 2. CMake를 실행하여 빌드 파일을 생성합니다.
#    -S : 소스 코드 디렉토리 (프로젝트 루트)
#    -B : 빌드 파일이 생성될 디렉토리 (build 폴더)
cmake -S "${PROJECT_ROOT}" -B "${BUILD_DIR}"

# 3. make를 실행하여 실제 컴파일을 진행합니다.
#    -C : make를 실행할 디렉토리를 지정합니다.
make -C "${BUILD_DIR}" -j$(nproc)

# 4. 빌드 결과 확인
# $? 변수는 바로 직전에 실행된 명령어의 성공(0) 또는 실패(0이 아닌 값) 코드를 담고 있습니다.
if [ $? -eq 0 ]; then
    echo ""
    echo "========================================"
    echo "✅ Build successful!"
    echo "   Executable is at: ${BUILD_DIR}/pi_server"
    echo "   Run from project root: ./build/pi_server"
    echo "========================================"
else
    echo ""
    echo "========================================"
    echo "❌ Build failed."
    echo "========================================"
fi
