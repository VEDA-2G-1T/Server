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
    DatabaseManager(const std::string& detection_db_path, const std::string& blur_db_path, const std::string& fall_db_path, const std::string& trespass_db_path, const std::string& image_save_dir);
    ~DatabaseManager();

    void initDatabases();
    
    bool getAllDetections(std::vector<DetectionData>& detections);
    bool getPersonCounts(std::vector<PersonCountData>& counts);
    bool getFallLogs(std::vector<FallCountData>& logs);
    bool getTrespassLogs(std::vector<TrespassLogData>& logs);

    std::optional<DetectionData> saveDetectionLog(int camera_id, const std::vector<DetectionResult>& results, const cv::Mat& frame, Detector& detector);
    std::optional<PersonCountData> saveBlurLog(int camera_id, int person_count);
    std::optional<FallCountData> saveFallLog(int camera_id, int fall_count, const cv::Mat& frame);
    std::optional<TrespassLogData> saveTrespassLog(int camera_id, int person_count, const cv::Mat& frame);

private:
    std::string detection_db_path_;
    std::string blur_db_path_;
    std::string fall_db_path_;
    std::string trespass_db_path_;

    std::string image_save_dir_; 

    std::string get_current_timestamp();
};
