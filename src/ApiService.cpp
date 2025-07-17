#include "ApiService.h"
#include "DatabaseManager.h"
#include "json.hpp"
#include "SharedState.h" 
#include "DetectionData.h" 
#include "StreamProcessor.h" 
#include "SerialCommunicator.h" 
#include "STM32Protocol.h"     

// 생성자는 이전과 동일하게 유지합니다.
//ApiService::ApiService(crow::SimpleApp& app, StreamProcessor& processor, DatabaseManager& dbManager)
//    : app_(app), processor_(processor), dbManager_(dbManager) {}

ApiService::ApiService(crow::SimpleApp& app, StreamProcessor& processor, DatabaseManager& dbManager, SerialCommunicator& serial_comm)
    : app_(app), processor_(processor), dbManager_(dbManager), serial_comm_(serial_comm) {}

void ApiService::setupRoutes() {
    // 웹소켓 라우트
    CROW_WEBSOCKET_ROUTE(app_, "/ws")
        .onopen([this](crow::websocket::connection& conn) {
            std::lock_guard<std::mutex> _(mtx_);
            ws_users_.insert(&conn);
        })
        .onclose([this](crow::websocket::connection& conn, const std::string& reason, uint16_t code) {
        std::lock_guard<std::mutex> _(mtx_);
        ws_users_.erase(&conn);
        CROW_LOG_INFO << "Websocket connection closed: " << &conn;
        })
        .onmessage([this](crow::websocket::connection& conn, const std::string& data, bool is_binary) {
            try {
                auto json_req = nlohmann::json::parse(data);
                if (json_req.value("type", "") == "request_stm_status") {
                    handleSTM32StatusCheck();
                }
            } catch (const std::exception& e) {
                CROW_LOG_ERROR << "Invalid WebSocket message: " << e.what();
            }
        });
    
    CROW_ROUTE(app_, "/api/detections")([this] {
        std::vector<DetectionData> results;
        nlohmann::json response_json;

        if (dbManager_.getAllDetections(results)) {
            nlohmann::json detections_array = nlohmann::json::array();
            for (const auto& data : results) {
                nlohmann::json detection_obj;
                detection_obj["camera_id"] = data.camera_id;
                detection_obj["timestamp"] = data.timestamp;
                detection_obj["all_objects"] = data.all_objects;
                detection_obj["person_count"] = data.person_count;
                detection_obj["helmet_count"] = data.helmet_count;
                detection_obj["safety_vest_count"] = data.safety_vest_count;
                detection_obj["avg_confidence"] = data.avg_confidence;
                detection_obj["image_path"] = data.image_path;
                detections_array.push_back(detection_obj);
            }
            response_json["status"] = "success";
            response_json["detections"] = detections_array;
            
            crow::response res(response_json.dump());
            res.set_header("Content-Type", "application/json");
            return res;
        } else {
            response_json["status"] = "error";
            response_json["message"] = "Failed to fetch detections from database.";
            crow::response res(500, response_json.dump());
            res.set_header("Content-Type", "application/json");
            return res;
        }
    });

    // /api/mode 라우트
    CROW_ROUTE(app_, "/api/mode").methods("POST"_method)
    ([this](const crow::request& req){
        auto data = nlohmann::json::parse(req.body);
        std::string mode = data.value("mode", "detect");

        std::vector<std::string> valid_modes = {"detect", "blur", "raw", "stop"};
        bool is_valid = false;
        for (const auto& valid_mode : valid_modes) {
            if (mode == valid_mode) {
                is_valid = true;
                break;
            }
        }

        if (is_valid) {
            std::lock_guard<std::mutex> lock(g_mode_mutex);
            g_current_mode = mode;
            
            std::cout << "모드가 다음으로 변경되었습니다: " << mode << std::endl;
            return crow::response(200, "{\"status\":\"success\", \"message\":\"모드가 성공적으로 변경되었습니다.\"}");
        }
        
        return crow::response(400, "{\"status\":\"error\", \"message\":\"유효하지 않은 모드입니다.\"}");
    });

}

// 웹소켓 broadcast
void ApiService::broadcastAnomalyStatus(bool isAnomaly) {
    nlohmann::json msg;
    msg["type"] = "anomaly_status"; // 메시지 종류 식별자
    msg["data"]["status"] = isAnomaly ? "detected" : "cleared";

    time_t now = time(0);
    char buf[80];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", localtime(&now));
    msg["data"]["timestamp"] = std::string(buf);

    std::lock_guard<std::mutex> _(mtx_);
    for (auto user : ws_users_) user->send_text(msg.dump());
}

void ApiService::broadcastNewDetection(const DetectionData& data) {
    nlohmann::json msg;
    msg["type"] = "new_detection"; // 메시지 종류 식별자

    nlohmann::json obj;
    obj["camera_id"] = data.camera_id;
    obj["timestamp"] = data.timestamp;
    obj["all_objects"] = data.all_objects;
    obj["person_count"] = data.person_count;
    obj["helmet_count"] = data.helmet_count;
    obj["safety_vest_count"] = data.safety_vest_count;
    obj["avg_confidence"] = data.avg_confidence;
    obj["image_path"] = data.image_path;
    msg["data"] = obj;

    std::lock_guard<std::mutex> _(mtx_);
    for (auto user : ws_users_) user->send_text(msg.dump());
}

void ApiService::broadcastNewBlur(const PersonCountData& data) {
    nlohmann::json msg;
    msg["type"] = "new_blur"; // 메시지 종류 식별자

    nlohmann::json obj;
    obj["camera_id"] = data.camera_id;
    obj["timestamp"] = data.timestamp;
    obj["count"] = data.count;
    msg["data"] = obj;

    std::lock_guard<std::mutex> _(mtx_);
    for (auto user : ws_users_) user->send_text(msg.dump());
}

void ApiService::handleSTM32StatusCheck() {
    std::cout << "[DEBUG] Received 'request_stm_status'. Attempting to check STM32..." << std::endl;
    uint8_t seq = serial_comm_.getNextSeq();
    
    auto frame_to_send = STM32Protocol::buildCheckFrame(seq);
    
    std::string log_msg = "[TX] Sent CHK (seq=" + std::to_string(seq) + ")";
    auto response_frame = serial_comm_.sendAndReceive(frame_to_send, log_msg);

    if (response_frame) {
        auto status_data = STM32Protocol::parseStatusData(*response_frame);
        if (status_data) {
            nlohmann::json msg;
            msg["type"] = "stm_status_update";
            msg["data"]["led_on"] = status_data->is_led_on;
            msg["data"]["buzzer_on"] = status_data->is_buzzer_on;
            msg["data"]["light"] = status_data->light_value;
            msg["data"]["temperature"] = status_data->temperature;
            
            std::lock_guard<std::mutex> _(mtx_);
            for (auto user : ws_users_) {
                user->send_text(msg.dump());
            }
        }
    }
}