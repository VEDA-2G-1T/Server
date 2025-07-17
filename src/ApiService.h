#pragma once
#include "crow.h"
#include <set>
#include <mutex>

// 전방 선언
class StreamProcessor;
class DatabaseManager;
class SerialCommunicator;
struct DetectionData;     
struct PersonCountData;  

class ApiService {
public:
    //ApiService(crow::SimpleApp& app, StreamProcessor& processor, DatabaseManager& dbManager);
    ApiService(crow::SimpleApp& app, StreamProcessor& processor, DatabaseManager& dbManager, SerialCommunicator& serial_comm);
    void setupRoutes();

    // ▼▼▼ StreamProcessor가 호출할 공개 함수 추가 ▼▼▼
    // 이상 상태가 변경되면 이 함수를 통해 모든 클라이언트에게 알립니다.
    void broadcastAnomalyStatus(bool isAnomaly);
    void broadcastNewDetection(const DetectionData& data);
    void broadcastNewBlur(const PersonCountData& data);
    void handleSTM32StatusCheck();

private:
    crow::SimpleApp& app_;
    StreamProcessor& processor_;
    DatabaseManager& dbManager_;
    SerialCommunicator& serial_comm_;

    // 웹소켓 관련 변수
    std::mutex mtx_; // 연결 목록을 보호하기 위한 뮤텍스
    std::set<crow::websocket::connection*> ws_users_; // 접속한 클라이언트 목록
};
