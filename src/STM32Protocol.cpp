#include "STM32Protocol.h"
#include <stdexcept>
#include <iomanip>
#include <sstream>
#include <cstring> // for memcpy

// Private static method for CRC calculation
uint16_t STM32Protocol::calc_crc16(const std::vector<uint8_t>& buf) {
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

std::vector<uint8_t> STM32Protocol::buildFrame(uint8_t cmd, uint8_t seq, uint8_t type, const std::vector<uint8_t>& data) {
    // Length = CMD(1) + SEQ(1) + TYPE(1) + DATA_SIZE + CRC(2)
    uint8_t length = 3 + data.size() + 2;

    // Construct the full frame without CRC first
    std::vector<uint8_t> frame = { SOF, length, cmd, seq, type };
    frame.insert(frame.end(), data.begin(), data.end());

    // Calculate CRC on the part of the frame from CMD to the end of DATA
    std::vector<uint8_t> crc_input(frame.begin() + 2, frame.end());
    uint16_t crc = calc_crc16(crc_input);

    // Append CRC (Little Endian)
    frame.push_back(static_cast<uint8_t>(crc & 0xFF));
    frame.push_back(static_cast<uint8_t>((crc >> 8) & 0xFF));
    
    return frame;
}

// --- Public static helper methods ---
std::vector<uint8_t> STM32Protocol::buildToggleFrame(uint8_t seq) {
    return buildFrame(CMD_TOGGLE, seq, TYPE_REQ);
}

std::vector<uint8_t> STM32Protocol::buildCheckFrame(uint8_t seq) {
    return buildFrame(CMD_CHK, seq, TYPE_REQ);
}

std::vector<uint8_t> STM32Protocol::buildResetFrame(uint8_t seq) {
    return buildFrame(CMD_RESET, seq, TYPE_REQ);
}

std::vector<uint8_t> STM32Protocol::buildAnomalyFrame(uint8_t seq, bool anomaly_detected) {
    std::vector<uint8_t> data = {static_cast<uint8_t>(anomaly_detected ? 1 : 0)};
    return buildFrame(CMD_ANOMALY, seq, TYPE_REQ, data);
}

// --- Parsing and Utility ---
std::optional<ParsedFrame> STM32Protocol::parseFrame(const std::vector<uint8_t>& frame) {
    if (frame.size() < 7 || frame[0] != SOF)
        return std::nullopt;

    uint8_t length = frame[1];
    if (frame.size() != length + 2) // Total size = SOF(1) + LEN_FIELD(1) + length
        return std::nullopt;

    // CRC is calculated on the part from CMD to the end of DATA
    std::vector<uint8_t> crc_input(frame.begin() + 2, frame.end() - 2);
    uint16_t expected_crc = calc_crc16(crc_input);
    uint16_t recv_crc = frame[frame.size() - 2] | (frame[frame.size() - 1] << 8);

    if (recv_crc != expected_crc)
        return std::nullopt;

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

std::optional<STM32Status> STM32Protocol::parseStatusData(const ParsedFrame& frame) {
    if (frame.cmd != CMD_CHK || frame.type != TYPE_RSP || frame.data.size() < 8) {
        return std::nullopt;
    }

    STM32Status status;
    status.is_led_on = (frame.data[0] == 0x01);
    status.is_buzzer_on = (frame.data[1] == 0x01);
    status.light_value = frame.data[2] | (frame.data[3] << 8);
    std::memcpy(&status.temperature, &frame.data[4], sizeof(float));

    return status;
}