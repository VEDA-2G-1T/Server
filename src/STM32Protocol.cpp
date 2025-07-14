#include "STM32Protocol.h"
#include <stdexcept>
#include <iomanip>
#include <sstream>
#include <cstring> // memcpy

namespace { 
    uint16_t calc_crc16(const std::vector<uint8_t>& buf) {
        uint16_t crc = 0xFFFF;
        for (uint8_t b : buf) {
            crc ^= b;
            for (int i = 0; i < 8; ++i) {
                if (crc & 1) crc = (crc >> 1) ^ 0xA001;
                else crc >>= 1;
            }
        }
        return crc;
    }

    std::vector<uint8_t> build_frame(uint8_t cmd, uint8_t type, const std::vector<uint8_t>& data = {}) {
        static uint8_t sequence = 0;
        uint8_t length = 5 + data.size(); // CMD(1)+SEQ(1)+TYPE(1)+DATA(...)+CRC(2)

        std::vector<uint8_t> payload;
        payload.push_back(cmd);
        payload.push_back(sequence);
        payload.push_back(type);
        payload.insert(payload.end(), data.begin(), data.end());
        
        uint16_t crc = calc_crc16(payload);

        std::vector<uint8_t> frame;
        frame.push_back(STM32Protocol::SOF);
        frame.push_back(length);
        frame.insert(frame.end(), payload.begin(), payload.end());
        frame.push_back(static_cast<uint8_t>(crc & 0xFF));
        frame.push_back(static_cast<uint8_t>((crc >> 8) & 0xFF));
        
        sequence++;
        return frame;
    }
}

// --- 외부 호출 함수 구현 ---
std::vector<uint8_t> STM32Protocol::buildToggleFrame() {
    return build_frame(CMD_TOGGLE, TYPE_REQ);
}
std::vector<uint8_t> STM32Protocol::buildCheckFrame() {
    return build_frame(CMD_CHK, TYPE_REQ);
}
std::vector<uint8_t> STM32Protocol::buildResetFrame() {
    return build_frame(CMD_RESET, TYPE_REQ);
}
std::vector<uint8_t> STM32Protocol::buildAnomalyFrame(bool anomaly_detected) {
    std::vector<uint8_t> data = {static_cast<uint8_t>(anomaly_detected ? 1 : 0)};
    return build_frame(CMD_ANOMALY, TYPE_REQ, data);
}

ParsedFrame STM32Protocol::parseFrame(const std::vector<uint8_t>& frame) {
    if (frame.size() < 7 || frame[0] != SOF) 
        throw std::runtime_error("Bad frame: SOF or too short");

    uint8_t length = frame[1];

    if (frame.size() != length + 2) 
        throw std::runtime_error("Bad LEN");

    std::vector<uint8_t> payload(frame.begin() + 2, frame.end() - 2);
    uint16_t expected_crc = calc_crc16(payload);
    uint16_t recv_crc = frame[frame.size() - 2] | (frame[frame.size() - 1] << 8);

    if (recv_crc != expected_crc) 
        throw std::runtime_error("CRC error");

    return ParsedFrame{
        frame[2], // cmd
        frame[3], // seq
        frame[4], // type
        std::vector<uint8_t>(frame.begin() + 5, frame.end() - 2) // data
    };
}

std::string STM32Protocol::frameToString(const std::vector<uint8_t>& frame) {
    std::stringstream ss;
    for(const auto& byte : frame) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte) << " ";
    }
    return ss.str();
}
