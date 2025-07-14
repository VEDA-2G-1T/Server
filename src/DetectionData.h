#pragma once
#include <string>

// detections 테이블의 한 행을 나타내는 구조체
struct DetectionData {
    int camera_id;
    std::string timestamp;
    std::string all_objects;
    int person_count;
    int helmet_count;
    int safety_vest_count;
    double avg_confidence; 
    std::string image_path;
};

// person_counts 테이블의 한 행을 나타내는 구조체 
struct PersonCountData {
    int camera_id;
    std::string timestamp;
    int count;
};
