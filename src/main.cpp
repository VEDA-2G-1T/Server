#include "crow.h"
#include "DatabaseManager.h"
#include "ApiService.h"
#include "SharedState.h"
#include "StreamProcessor.h" // 새로 만든 클래스 포함

#include <thread>
#include <csignal>
#include <iostream>

// 전역 변수 선언
std::string g_current_mode = "blur";
std::mutex g_mode_mutex;
std::atomic<bool> g_keep_running(true);

// 시그널 핸들러
void signal_handler(int signum) {
    std::cout << "종료 신호 (" << signum << ") 수신." << std::endl;
    g_keep_running = false;
}

int main() {
    // 1. 시그널 핸들링 설정
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // 2. 핵심 컴포넌트 생성
    DatabaseManager dbManager("../data/detections.db", "../data/blur.db", "../captured_images");
    StreamProcessor streamProcessor(dbManager);
    
    crow::SimpleApp app;
    ApiService apiService(app, streamProcessor, dbManager);
    apiService.setupRoutes();

    // 콜백 함수 등록 (이벤트 연결)
    streamProcessor.onAnomalyStatusChanged([&apiService](bool isAnomaly) {
        apiService.broadcastAnomalyStatus(isAnomaly);
    });

    streamProcessor.onNewDetection([&apiService](const DetectionData& data) {
        apiService.broadcastNewDetection(data);
    });

    streamProcessor.onNewBlur([&apiService](const PersonCountData& data) {
        apiService.broadcastNewBlur(data);
    });

    // 3. API 서버를 백그라운드 스레드에서 실행
    std::thread server_thread([&app](){
        std::cout << "C++ 백엔드 서버가 9000번 포트에서 시작됩니다..." << std::endl;
        app.port(9000).multithreaded().run();
    });

    // 4. 메인 스레드에서 영상 처리 루프 실행
    streamProcessor.run(); // g_keep_running이 false가 될 때까지 여기서 대기

    // 5. 정리
    std::cout << "서버와 스트리밍 프로세스를 중지하고 리소스를 정리합니다..." << std::endl;
    app.stop();
    if (server_thread.joinable()) {
        server_thread.join();
    }
    
    std::cout << "프로그램을 종료합니다." << std::endl;
    return 0;
}
