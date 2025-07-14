// detector.h (완성본)
#pragma once
#include <onnxruntime_cxx_api.h>
#include <opencv2/opencv.hpp>
#include <vector>
#include <string>
#include <memory>

struct DetectionResult {
    cv::Rect box;
    float confidence;
    int class_id;
};

class Detector {
public:
    Detector(const std::string& model_path = "best_test.onnx", const cv::Size& input_size = {320, 320});
    std::vector<DetectionResult> detect(const cv::Mat& image, float conf_threshold, float nms_threshold);
    const std::vector<std::string>& get_class_names() const;

private:
    Ort::Env env;
    Ort::SessionOptions session_options;
    std::unique_ptr<Ort::Session> session;

    cv::Size input_size;
    // [수정] const char* 대신 std::string으로 이름 저장
    std::vector<std::string> input_names_str;
    std::vector<std::string> output_names_str;
    Ort::AllocatorWithDefaultOptions allocator;

    std::vector<std::string> class_names;
    std::vector<std::string> target_class_names = {"person", "helmet", "safety-vest"};
};
