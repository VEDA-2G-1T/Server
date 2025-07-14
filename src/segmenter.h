// crtsp/src/segmenter.h

#pragma once

#include <opencv2/opencv.hpp>
#include <memory>
#include "nn/autobackend.h"

// 반환값으로 사용할 구조체 정의
struct SegmentationResult {
    int person_count = 0;
};

class Segmenter {
public:
    Segmenter(const std::string& model_path);
    ~Segmenter();

    // 함수 이름을 바꾸고, 사람 수를 담은 구조체를 반환하도록 수정
    SegmentationResult process_frame(cv::Mat& frame);

private:
    std::unique_ptr<AutoBackendOnnx> model;
    int person_class_id;
    float conf_threshold;
    float iou_threshold;
    float mask_threshold;
    std::vector<cv::Scalar> colors;

    void apply_blur(cv::Mat& img, std::vector<YoloResults>& results);
};