#pragma once
#include "types.h"
#include <opencv2/opencv.hpp>
#include <vector>
#include <string>
#include <memory>
// #include <set> // 더 이상 필요 없음

#include <tensorflow/lite/interpreter.h>
#include <tensorflow/lite/kernels/register.h>
#include <tensorflow/lite/model.h>
#include <tensorflow/lite/interpreter_builder.h>
#include <tensorflow/lite/delegates/xnnpack/xnnpack_delegate.h>


class Fall {
public:
    // ✅ 기본 모델 경로 수정 및 불필요한 input_size 제거
    Fall(const std::string& model_path = "../fall_192.tflite"); 
    
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
    
    std::vector<std::string> class_names;
};