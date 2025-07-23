#pragma once

#include <opencv2/opencv.hpp>

// Detector와 Fall 클래스가 공통으로 사용하는 결과 구조체
struct DetectionResult {
    cv::Rect box;
    float confidence;
    int class_id;
};