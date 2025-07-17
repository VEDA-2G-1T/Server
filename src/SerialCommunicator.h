#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <optional> // optional을 위해 포함
#include "STM32Protocol.h" // ParsedFrame을 위해 포함

class SerialCommunicator {
public:
    SerialCommunicator(const std::string& port, int baudrate);
    ~SerialCommunicator();

    std::optional<ParsedFrame> sendAndReceive(const std::vector<uint8_t>& frame_to_send, const std::string& log_description);
    
    bool send(const std::vector<uint8_t>& frame);
    bool isOpen() const;
    uint8_t getNextSeq();

private:
    int fd_ = -1;
    bool is_open_ = false;
    uint8_t seq_ = 0;

    // 내부 헬퍼 함수
    int read_byte(uint8_t& out, int timeout_ms);
    std::vector<uint8_t> read_raw_frame(int timeout_ms);
};
