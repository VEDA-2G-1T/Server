#include "SerialCommunicator.h"
#include <iostream>
#include <termios.h>
#include <fcntl.h>
#include <unistd.h>
#include <chrono>
#include <stdexcept>
#include <cstring>

// 생성자
SerialCommunicator::SerialCommunicator(const std::string& port, int baudrate) {
    fd_ = open(port.c_str(), O_RDWR | O_NOCTTY);
    if (fd_ < 0) {
        std::cerr << "오류: 시리얼 포트 " << port << " 열기 실패 - " << strerror(errno) << std::endl;
        return;
    }

    termios tty{};
    if (tcgetattr(fd_, &tty) != 0) {
        std::cerr << "오류: 시리얼 포트 속성을 가져올 수 없습니다 - " << strerror(errno) << std::endl;
        return;
    }

    cfsetospeed(&tty, baudrate);
    cfsetispeed(&tty, baudrate);

    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;
    tty.c_iflag &= ~IGNBRK;
    tty.c_lflag = 0;
    tty.c_oflag = 0;
    tty.c_cc[VMIN]  = 0;
    tty.c_cc[VTIME] = 1; // 0.1 seconds read timeout

    tty.c_iflag &= ~(IXON | IXOFF | IXANY);
    tty.c_cflag |= (CLOCAL | CREAD);
    tty.c_cflag &= ~(PARENB | PARODD);
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CRTSCTS;

    if (tcsetattr(fd_, TCSANOW, &tty) != 0) {
        std::cerr << "오류: 시리얼 포트 속성을 설정할 수 없습니다 - " << strerror(errno) << std::endl;
        return;
    }

    is_open_ = true;
    std::cout << "시리얼 포트 " << port << " 초기화 성공." << std::endl;
}

// 소멸자
SerialCommunicator::~SerialCommunicator() {
    if (fd_ >= 0) {
        close(fd_);
    }
}

bool SerialCommunicator::isOpen() const {
    return is_open_;
}

// 응답을 기다리지 않고 데이터 프레임을 보내기만 합니다.
bool SerialCommunicator::send(const std::vector<uint8_t>& frame) {
    if (!isOpen()) return false;
    ssize_t bytes_written = write(fd_, frame.data(), frame.size());
    return bytes_written == static_cast<ssize_t>(frame.size());
}

// 요청 전송, 로그 출력, 응답 수신, 파싱, 로그 출력을 모두 처리하는 메인 함수
std::optional<ParsedFrame> SerialCommunicator::sendAndReceive(const std::vector<uint8_t>& frame_to_send, const std::string& log_description) {
    if (!isOpen()) return std::nullopt;

    std::cout << log_description << std::endl;
    std::cout << "[TX RAW] " << STM32Protocol::frameToString(frame_to_send) << std::endl;
    ssize_t bytes_written = write(fd_, frame_to_send.data(), frame_to_send.size());
    if (bytes_written != static_cast<ssize_t>(frame_to_send.size())) {
        std::cerr << "[TX] Error: Failed to write all data to serial port." << std::endl;
        return std::nullopt;
    }

    try {
        auto raw_response = read_raw_frame(2000); // 2초 타임아웃
        if (raw_response.empty()) {
            std::cerr << "[RX] Timeout or no response received." << std::endl;
            return std::nullopt;
        }
        std::cout << "[RX RAW] " << STM32Protocol::frameToString(raw_response) << std::endl;
        ParsedFrame parsed = STM32Protocol::parseFrame(raw_response);
        return parsed;

    } catch (const std::exception& e) {
        std::cerr << "[RX] Error: " << e.what() << std::endl;
        return std::nullopt;
    }
}

// --- 내부 헬퍼 함수 구현 ---
int SerialCommunicator::read_byte(uint8_t& out, int timeout_ms) {
    if (!isOpen()) return -1;
    fd_set set;
    FD_ZERO(&set);
    FD_SET(fd_, &set);
    timeval timeout{};
    timeout.tv_sec = timeout_ms / 1000;
    timeout.tv_usec = (timeout_ms % 1000) * 1000;
    int rv = select(fd_ + 1, &set, nullptr, nullptr, &timeout);
    if (rv > 0) return read(fd_, &out, 1);
    return 0;
}

std::vector<uint8_t> SerialCommunicator::read_raw_frame(int timeout_ms) {
    using clock = std::chrono::steady_clock;
    auto t0 = clock::now();
    std::vector<uint8_t> buf;
    uint8_t b;
    bool frame_started = false;

    while (std::chrono::duration_cast<std::chrono::milliseconds>(clock::now() - t0).count() < timeout_ms) {
        if (read_byte(b, 100) == 1) {
            if (!frame_started) {
                if (b == STM32Protocol::SOF) {
                    frame_started = true;
                    buf.clear();
                    buf.push_back(b);
                }
            } else {
                buf.push_back(b);
                if (buf.size() >= 2) {
                    size_t expected_length = buf[1] + 2;
                    if (buf.size() >= expected_length) {
                        return buf;
                    }
                }
            }
        }
    }
    return {};
}
