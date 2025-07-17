#pragma once

#include <vector>
#include <cstdint>
#include <string>
#include <optional>

// 응답 프레임을 파싱한 결과를 담을 구조체
struct ParsedFrame {
    uint8_t cmd;
    uint8_t seq;
    uint8_t type;
    std::vector<uint8_t> data;
};

struct STM32Status {
    bool is_led_on;
    bool is_buzzer_on;
    uint16_t light_value;
    float temperature;
};

class STM32Protocol {
public:
    // 통신 프레임에 사용될 상수 정의
    static constexpr uint8_t SOF = 0x7E;
    static constexpr uint8_t CMD_TOGGLE = 0x10;
    static constexpr uint8_t CMD_CHK    = 0x11;
    static constexpr uint8_t CMD_RESET  = 0x12;
    static constexpr uint8_t CMD_ANOMALY = 0x13;  // 이상탐지 알림 명령 추가
    static constexpr uint8_t TYPE_REQ   = 0x00; 
    static constexpr uint8_t TYPE_RSP   = 0x01; 

    // 프레임 생성 함수들
    static std::vector<uint8_t> buildToggleFrame(uint8_t seq);
    static std::vector<uint8_t> buildCheckFrame(uint8_t seq);
    static std::vector<uint8_t> buildResetFrame(uint8_t seq);
    static std::vector<uint8_t> buildAnomalyFrame(uint8_t seq, bool is_anomaly);  // 이상탐지 프레임 추가

    // 프레임 파싱 및 로깅 함수
    static std::optional<ParsedFrame> parseFrame(const std::vector<uint8_t>& buffer);
    static std::string frameToString(const std::vector<uint8_t>& frame);

    // 시스템 상태 확인 응답 데이터를 파싱하는 함수
    static std::optional<STM32Status> parseStatusData(const ParsedFrame& frame);

private:
    // 핵심 프레임 생성 함수 (이제 private으로 만들어도 무방)
    static std::vector<uint8_t> buildFrame(uint8_t cmd, uint8_t seq, uint8_t type, const std::vector<uint8_t>& data = {});
    static uint16_t calc_crc16(const std::vector<uint8_t>& buf);

};
