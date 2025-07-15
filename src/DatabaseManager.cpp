#include "DatabaseManager.h"
#include <iostream>
#include <set>
#include <algorithm>
#include <sys/stat.h>

// ★ 수정: 생성자에서 모든 경로를 받아 멤버 변수에 저장
DatabaseManager::DatabaseManager(const std::string& detection_db_path, const std::string& blur_db_path, const std::string& image_save_dir)
    : detection_db_path_(detection_db_path), blur_db_path_(blur_db_path), image_save_dir_(image_save_dir), db_detection_(nullptr), db_blur_(nullptr) {
    initDatabases();
}

DatabaseManager::~DatabaseManager() {
    // 소멸자에서는 DB 연결을 닫을 필요가 없습니다.
    // 각 함수에서 열고 닫는 것이 더 안전합니다.
}

void DatabaseManager::initDatabases() {
    // 이제 멤버 변수를 사용
    mkdir(image_save_dir_.c_str(), 0777);

    sqlite3* db;
    if (sqlite3_open(detection_db_path_.c_str(), &db) == SQLITE_OK) {
        const char* sql = "CREATE TABLE IF NOT EXISTS detections ("
                          "camera_id INTEGER NOT NULL, "
                          "timestamp TEXT NOT NULL, all_objects TEXT, person_count INTEGER NOT NULL, "
                          "helmet_count INTEGER NOT NULL, safety_vest_count INTEGER NOT NULL, "
                          "avg_confidence REAL, image_path TEXT);";
        sqlite3_exec(db, sql, 0, 0, 0);
        sqlite3_close(db);
    }
    if (sqlite3_open(blur_db_path_.c_str(), &db) == SQLITE_OK) {
        const char* sql = "CREATE TABLE IF NOT EXISTS person_counts ("
                          "camera_id INTEGER NOT NULL, "
                          "timestamp TEXT NOT NULL, count INTEGER NOT NULL);";
        sqlite3_exec(db, sql, 0, 0, 0);
        sqlite3_close(db);
    }
    std::cout << "데이터베이스 초기화 완료." << std::endl;
}

std::string DatabaseManager::get_current_timestamp() {
    time_t now = time(0);
    char buf[80];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", localtime(&now));
    return buf;
}

// ★ 수정: 전역 변수 대신 멤버 변수를 사용하도록 전체 로직 구현
std::optional<DetectionData> DatabaseManager::saveDetectionLog(int camera_id, const std::vector<DetectionResult>& results, const cv::Mat& frame, Detector& detector) {
    if (results.empty()) return std::nullopt;

    int person_count = 0, helmet_count = 0, safety_vest_count = 0;
    double total_confidence = 0;
    std::set<std::string> unique_objects;

    const auto& class_names = detector.get_class_names();
    for(const auto& res : results) {
        std::string class_name = class_names[res.class_id];
        if (class_name == "person") person_count++;
        else if (class_name == "helmet") helmet_count++;
        else if (class_name == "safety-vest") safety_vest_count++;
        unique_objects.insert(class_name);
        total_confidence += res.confidence;
    }

    std::string timestamp_str = get_current_timestamp();
    std::string image_path = "";
    bool is_normal_state = (helmet_count == safety_vest_count) && (person_count <= helmet_count);
    if (!is_normal_state) {
        std::string timestamp_file = timestamp_str;
        std::replace(timestamp_file.begin(), timestamp_file.end(), ':', '-');
        std::replace(timestamp_file.begin(), timestamp_file.end(), ' ', '_');
        image_path = image_save_dir_ + "/" + timestamp_file + ".jpg";
        cv::imwrite(image_path, frame);
    }

    std::string all_objects_str;
    for(const auto& obj : unique_objects) { all_objects_str += obj + ", "; }
    if (!all_objects_str.empty()) all_objects_str.resize(all_objects_str.length() - 2);

    double avg_confidence = results.empty() ? 0.0 : total_confidence / results.size();

    sqlite3* db;

    if (sqlite3_open(detection_db_path_.c_str(), &db) != SQLITE_OK) {
        return std::nullopt; // DB 열기 실패 시 빈 optional 반환
    }

    sqlite3_exec(db, "PRAGMA journal_mode=WAL;", NULL, NULL, NULL);
    const char* sql = "INSERT INTO detections (camera_id, timestamp, all_objects, person_count, helmet_count, safety_vest_count, avg_confidence, image_path) VALUES(?,?,?,?,?,?,?,?);";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, camera_id); 
        sqlite3_bind_text(stmt, 2, timestamp_str.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, all_objects_str.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 4, person_count);
        sqlite3_bind_int(stmt, 5, helmet_count);
        sqlite3_bind_int(stmt, 6, safety_vest_count);
        sqlite3_bind_double(stmt, 7, avg_confidence);
        sqlite3_bind_text(stmt, 8, image_path.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_step(stmt);
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);

    DetectionData data;
    data.camera_id = camera_id;
    data.timestamp = timestamp_str;
    data.all_objects = all_objects_str;
    data.person_count = person_count;
    data.helmet_count = helmet_count;
    data.safety_vest_count = safety_vest_count;
    data.avg_confidence = avg_confidence;
    data.image_path = image_path;

    return data;
}

// ★ 수정: 전역 변수 대신 멤버 변수를 사용하도록 전체 로직 구현
std::optional<PersonCountData> DatabaseManager::saveBlurLog(int camera_id, int person_count) {
    sqlite3* db;
    if (sqlite3_open(blur_db_path_.c_str(), &db) != SQLITE_OK) {
        return std::nullopt; // DB 열기 실패 시 빈 optional 반환
    }

    std::string timestamp_str = get_current_timestamp(); 

    sqlite3_exec(db, "PRAGMA journal_mode=WAL;", NULL, NULL, NULL);
    const char* sql = "INSERT INTO person_counts (camera_id, timestamp, count) VALUES(?,?,?);";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, camera_id);
        sqlite3_bind_text(stmt, 2, timestamp_str.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 3, person_count);
        sqlite3_step(stmt);
    }
    sqlite3_finalize(stmt);
    sqlite3_close(db);

    PersonCountData data;
    data.camera_id = camera_id;
    data.timestamp = timestamp_str;
    data.count = person_count;

    return data;
}

bool DatabaseManager::getAllDetections(std::vector<DetectionData>& detections) {
    sqlite3* db;
    if (sqlite3_open(detection_db_path_.c_str(), &db) != SQLITE_OK) {
        std::cerr << "Error opening detection DB: " << sqlite3_errmsg(db) << std::endl;
        return false;
    }

    const char* sql = "SELECT camera_id, timestamp, all_objects, person_count, helmet_count, safety_vest_count, avg_confidence, image_path FROM detections ORDER BY timestamp DESC;";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(db) << std::endl;
        sqlite3_close(db);
        return false;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        DetectionData data;
        auto get_safe_string = [&](int col_index) {
            const char* text = reinterpret_cast<const char*>(sqlite3_column_text(stmt, col_index));
            return text ? std::string(text) : "";
        };

        data.camera_id = sqlite3_column_int(stmt, 0);
        data.timestamp = get_safe_string(1);
        data.all_objects = get_safe_string(2);
        data.person_count = sqlite3_column_int(stmt, 3);
        data.helmet_count = sqlite3_column_int(stmt, 4);
        data.safety_vest_count = sqlite3_column_int(stmt, 5);
        data.avg_confidence = sqlite3_column_double(stmt, 6);
        data.image_path = get_safe_string(7);
        
        detections.push_back(data);
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return true;
}

bool DatabaseManager::getPersonCounts(std::vector<PersonCountData>& counts) {
    sqlite3* db;
    if (sqlite3_open(blur_db_path_.c_str(), &db) != SQLITE_OK) {
        std::cerr << "Error opening blur DB: " << sqlite3_errmsg(db) << std::endl;
        return false;
    }

    const char* sql = "SELECT camera_id, timestamp, count FROM person_counts ORDER BY timestamp ASC;";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(db) << std::endl;
        sqlite3_close(db);
        return false;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        PersonCountData data;
        auto get_safe_string = [&](int col_index) {
            const char* text = reinterpret_cast<const char*>(sqlite3_column_text(stmt, col_index));
            return text ? std::string(text) : "";
        };
        data.camera_id = sqlite3_column_int(stmt, 0);
        data.timestamp = get_safe_string(1);
        data.count = sqlite3_column_int(stmt, 2);
        
        counts.push_back(data);
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return true;
}