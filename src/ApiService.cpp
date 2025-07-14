#include "ApiService.h"
#include "DatabaseManager.h"
#include "json.hpp"
#include "SharedState.h" 
#include "DetectionData.h" 
#include "StreamProcessor.h" 

// 생성자는 이전과 동일하게 유지합니다.
ApiService::ApiService(crow::SimpleApp& app, StreamProcessor& processor, DatabaseManager& dbManager)
    : app_(app), processor_(processor), dbManager_(dbManager) {}

void ApiService::setupRoutes() {
    
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

    CROW_ROUTE(app_, "/api/blur")([this] {
        std::vector<PersonCountData> results;
        nlohmann::ordered_json response_json;

        if (dbManager_.getPersonCounts(results)) {
            nlohmann::ordered_json counts_array = nlohmann::ordered_json::array();
            for (const auto& data : results) {
                nlohmann::ordered_json count_obj;
                count_obj["camera_id"] = data.camera_id;
                count_obj["timestamp"] = data.timestamp;
                count_obj["count"] = data.count;
                counts_array.push_back(count_obj);
            }
            response_json["status"] = "success";
            response_json["person_counts"] = counts_array;
            
            crow::response res(response_json.dump());
            res.set_header("Content-Type", "application/json");
            return res;
        } else {
            response_json["status"] = "error";
            response_json["message"] = "Failed to fetch person counts from database.";
            crow::response res(500, response_json.dump());
            res.set_header("Content-Type", "application/json");
            return res;
        }
    });

    // /api/mode 라우트는 이전과 동일
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

    CROW_ROUTE(app_, "/api/anomaly/status")([this] {
        // StreamProcessor로부터 현재 이상 탐지 상태를 가져옵니다.
        // anomaly_detected_가 std::atomic<bool>이므로 .load()로 안전하게 읽습니다.
        bool is_anomaly = processor_.isAnomalyDetected(); // 혹은 직접 접근: processor_.anomaly_detected_.load()

        nlohmann::ordered_json response_json;
        response_json["status"] = is_anomaly ? "detected" : "cleared";

        crow::response res(response_json.dump());
        res.set_header("Content-Type", "application/json");
        return res;
    });
}
