cmake_minimum_required(VERSION 3.10)
project(pi_server CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- 컴파일러 플래그 (선택 사항) ---
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -DNDEBUG")

# --- 필수 패키지 찾기 ---
find_package(OpenCV REQUIRED)
find_package(SQLite3 REQUIRED)
find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(FFTW3 REQUIRED fftw3)
pkg_check_modules(ALSA REQUIRED alsa)
find_package(OpenSSL REQUIRED)

# --- 외부 라이브러리 절대 경로 설정 ---
set(ONNXRUNTIME_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/onnxruntime")
set(CROW_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/Crow/include")
set(JSON_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/json/single_include/nlohmann")

file(GLOB_RECURSE YOLO_SOURCES "src/yolo_backend/src/nn/*.cpp" "src/yolo_backend/src/utils/*.cpp")

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
    ${YOLO_SOURCES}
)

# --- 실행 파일 ---
add_executable(pi_server ${SOURCES})

target_compile_definitions(pi_server PRIVATE CROW_ENABLE_SSL)

# --- 인클루드 디렉토리 ---
target_include_directories(pi_server PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/yolo_backend/include
    ${OpenCV_INCLUDE_DIRS}
    ${SQLite3_INCLUDE_DIRS}
    ${ONNXRUNTIME_DIR}/include
    ${CROW_INCLUDE_DIR}
    ${JSON_INCLUDE_DIR}
    ${FFTW3_INCLUDE_DIRS}
    ${ALSA_INCLUDE_DIRS}
)

# --- 라이브러리 링크 ---
target_link_libraries(pi_server PRIVATE
    Threads::Threads
    SQLite::SQLite3
    ${OpenCV_LIBS}
    ${ONNXRUNTIME_DIR}/lib/libonnxruntime.so
    -lpthread
    ${FFTW3_LIBRARIES}
    ${ALSA_LIBRARIES}
    OpenSSL::SSL     
    OpenSSL::Crypto   
)

# --- 컴파일러 플래그 (선택 사항) ---
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -DNDEBUG")
