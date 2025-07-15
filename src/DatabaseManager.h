#pragma once
#include <string>
#include <vector>
#include <opencv2/opencv.hpp>
#include <sqlite3.h>
#include <optional> 

#include "DetectionData.h"
#include "detector.h"

class DatabaseManager {
public:
    // ★ 수정: 생성자에 이미지 저장 경로 추가
    DatabaseManager(const std::string& detection_db_path, const std::string& blur_db_path, const std::string& image_save_dir);
    ~DatabaseManager();

    void initDatabases();
    
    bool getAllDetections(std::vector<DetectionData>& detections);
    bool getPersonCounts(std::vector<PersonCountData>& counts);
    
    std::optional<DetectionData> saveDetectionLog(int camera_id, const std::vector<DetectionResult>& results, const cv::Mat& frame, Detector& detector);
    std::optional<PersonCountData> saveBlurLog(int camera_id, int person_count);

private:
    sqlite3* db_detection_;
    sqlite3* db_blur_;
    std::string detection_db_path_;
    std::string blur_db_path_;
    std::string image_save_dir_; // ★ 추가: 이미지 저장 경로 멤버 변수
    std::string get_current_timestamp();
};
