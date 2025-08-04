# CMake 최소 버전 및 프로젝트 이름/언어 설정
cmake_minimum_required(VERSION 3.10)
project(pi_server CXX)

# C++17 표준을 사용하도록 설정
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- 컴파일러 옵션 ---
# 최적화 옵션 (-O3) 및 디버그 코드 제외(-DNDEBUG) 플래그를 설정합니다.
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -DNDEBUG")

# --- 시스템 라이브러리 찾기 ---
# PkgConfig는 .pc 파일을 통해 라이브러리를 찾는 데 사용됩니다.
find_package(PkgConfig REQUIRED)

# pkg-config를 사용하여 시스템에 설치된 라이브러리들의 경로와 링크 정보를 찾습니다.
pkg_check_modules(OpenCV REQUIRED opencv4)
pkg_check_modules(SQLite3 REQUIRED sqlite3)
pkg_check_modules(FFTW3 REQUIRED fftw3)
pkg_check_modules(ALSA REQUIRED alsa)

# CMake 내장 기능을 사용하여 Threads와 OpenSSL 라이브러리를 찾습니다.
find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)

# --- 외부(Bundled) 라이브러리 경로 설정 ---
# 프로젝트에 포함된 라이브러리들의 경로를 변수로 정의합니다.
set(ONNXRUNTIME_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/onnxruntime")
set(TFLITE_DIR      "${CMAKE_CURRENT_SOURCE_DIR}/external/tensorflow_lite")
set(CROW_DIR        "${CMAKE_CURRENT_SOURCE_DIR}/external/Crow")
set(JSON_DIR        "${CMAKE_CURRENT_SOURCE_DIR}/external/json/single_include/nlohmann")

# --- 소스 파일 목록 ---
# yolo_backend 폴더 안의 모든 .cpp 파일을 자동으로 찾아 YOLO_SOURCES 변수에 담습니다.
file(GLOB_RECURSE YOLO_SOURCES "src/yolo_backend/src/nn/*.cpp" "src/yolo_backend/src/utils/*.cpp")

# 컴파일할 모든 소스 파일들을 지정합니다.
set(SOURCES
    src/main.cpp
    src/ApiService.cpp
    src/DatabaseManager.cpp
    src/detector.cpp
    src/segmenter.cpp
    src/StreamProcessor.cpp
    src/SerialCommunicator.cpp
    src/STM32Protocol.cpp
    src/AnomalyDetector.cpp
    src/AudioNotifier.cpp
    src/fall.cpp
    src/SystemMonitor.cpp
    src/driver/led_pwm/led_controller/led_pwm_controller.cpp
    src/driver/led_pwm/led_controller/led_fade_manager.cpp
    src/driver/mic/mic_cotroller/mic_controller.cpp
    ${YOLO_SOURCES}
)

# --- 실행 파일 정의 ---
# 위 소스 파일들을 바탕으로 'pi_server'라는 이름의 실행 파일을 생성합니다.
add_executable(pi_server ${SOURCES})

# Crow 라이브러리에서 SSL 기능을 활성화하기 위한 컴파일 정의를 추가합니다.
target_compile_definitions(pi_server PRIVATE CROW_ENABLE_SSL)
target_compile_options(pi_server PRIVATE -march=native)

# --- 헤더 파일 인클루드 경로 연결 ---
# pi_server를 컴파일할 때 필요한 헤더 파일들이 있는 폴더들을 지정합니다.
target_include_directories(pi_server PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/yolo_backend/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src/driver/led_pwm/led_controller
    ${CMAKE_CURRENT_SOURCE_DIR}/src/driver/mic/mic_cotroller
    ${OpenCV_INCLUDE_DIRS}
    ${SQLite3_INCLUDE_DIRS}
    ${ONNXRUNTIME_DIR}/include
    ${TFLITE_DIR}/include
    ${CROW_DIR}/include
    ${JSON_DIR}
    ${FFTW3_INCLUDE_DIRS}
    ${ALSA_INCLUDE_DIRS}
)

# --- 라이브러리 링크 ---
# 최종 실행 파일에 필요한 라이브러리들을 연결합니다.
target_link_libraries(pi_server PRIVATE
    # 시스템 라이브러리
    Threads::Threads
    ${OpenCV_LIBRARIES}
    ${SQLite3_LIBRARIES}
    ${FFTW3_LIBRARIES}
    ${ALSA_LIBRARIES}
    OpenSSL::SSL
    OpenSSL::Crypto

    # 외부(Bundled) 라이브러리
    ${ONNXRUNTIME_DIR}/lib/libonnxruntime.so
    ${TFLITE_DIR}/lib/libtensorflowlite.so
)