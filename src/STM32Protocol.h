#pragma once

#include <vector>
#include <cstdint>
#include <string>

// 응답 프레임을 파싱한 결과를 담을 구조체
struct ParsedFrame {
    uint8_t cmd;
    uint8_t seq;
    uint8_t type;
    std::vector<uint8_t> data;
};

namespace STM32Protocol {
    // 통신 프레임에 사용될 상수 정의
    constexpr uint8_t SOF = 0x7E;
    constexpr uint8_t CMD_TOGGLE = 0x10;
    constexpr uint8_t CMD_CHK    = 0x11;
    constexpr uint8_t CMD_RESET  = 0x12;
    constexpr uint8_t CMD_ANOMALY = 0x13;  // 이상탐지 알림 명령 추가
    constexpr uint8_t TYPE_REQ   = 0x00; 
    constexpr uint8_t TYPE_RSP   = 0x01; 

    // 프레임 생성 함수들
    std::vector<uint8_t> buildToggleFrame();
    std::vector<uint8_t> buildCheckFrame();
    std::vector<uint8_t> buildResetFrame();
    std::vector<uint8_t> buildAnomalyFrame(bool anomaly_detected);  // 이상탐지 프레임 추가

    // 프레임 파싱 및 로깅 함수
    ParsedFrame parseFrame(const std::vector<uint8_t>& frame);
    std::string frameToString(const std::vector<uint8_t>& frame);
}
