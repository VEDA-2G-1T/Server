// detector.h (완성본)
#pragma once
#include <opencv2/opencv.hpp>
#include <vector>
#include <string>
#include <memory>
#include <set>
#include <tensorflow/lite/interpreter.h>
#include <tensorflow/lite/kernels/register.h>
#include <tensorflow/lite/model.h>
#include <tensorflow/lite/interpreter_builder.h>
#include <tensorflow/lite/delegates/xnnpack/xnnpack_delegate.h>

struct DetectionResult {
    cv::Rect box;
    float confidence;
    int class_id;
};

class Detector {
public:
    Detector(const std::string& model_path = "../detect_192.tflite", const cv::Size& input_size = {192, 192});
    std::vector<DetectionResult> detect(const cv::Mat& image, float conf_threshold, float nms_threshold);
    const std::vector<std::string>& get_class_names() const;

private:
    std::unique_ptr<tflite::FlatBufferModel> model;
    std::unique_ptr<tflite::Interpreter> interpreter;
    std::unique_ptr<TfLiteDelegate, decltype(&TfLiteXNNPackDelegateDelete)> xnnpack_delegate;
    float input_scale = 1.0f;
    int input_zero_point = 0;
    float output_scale = 1.0f;
    int output_zero_point = 0;
    int in_h = 0, in_w = 0, in_c = 0;
    int out_num_attr = 0, out_num_det = 0;
    int input_idx = 0, output_idx = 0;
    cv::Size input_size;
    std::vector<std::string> class_names;
    std::set<int> target_class_ids = {0, 10, 16}; // person, helmet, safety-vest
};
