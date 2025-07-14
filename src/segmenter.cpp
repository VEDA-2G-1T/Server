#include "segmenter.h"
#include "yolo_backend/include/constants.h"
#include "yolo_backend/include/utils/common.h" // generateRandomColors 함수를 위해

// 생성자: 모델 로딩 및 초기 설정
Segmenter::Segmenter(const std::string& model_path) {
    conf_threshold = 0.5f;
    iou_threshold = 0.45f;
    mask_threshold = 0.5f;

    const std::string onnx_provider = OnnxProviders::CPU;
    model = std::make_unique<AutoBackendOnnx>(model_path.c_str(), "segmenter_log", onnx_provider.c_str());

    auto names = model->getNames();
    colors = generateRandomColors(model->getNc(), model->getCh());

    person_class_id = 0;
    for (const auto& pair : names) {
        if (pair.second == "'person'") { // 따옴표 포함
            person_class_id = pair.first;
            break;
        }
    }

    if (person_class_id == -1) {
        throw std::runtime_error("'person' class not found in the model!");
    }
    std::cout << "Segmenter initialized. Found 'person' with ID: " << person_class_id << std::endl;
}

// 소멸자
Segmenter::~Segmenter() {}

SegmentationResult Segmenter::process_frame(cv::Mat& frame) {
    if (frame.empty()) return {0}; // 빈 프레임이면 0명 반환

    std::vector<YoloResults> all_results = model->predict_once(frame, conf_threshold, iou_threshold, mask_threshold);
    std::vector<YoloResults> person_results;
    for (const auto& result : all_results) {
        if (result.class_idx == person_class_id) {
            person_results.push_back(result);
        }
    }
    apply_blur(frame, person_results);

    // 사람 수를 담은 구조체를 반환합니다.
    return {(int)person_results.size()};
}

// 블러 처리 함수
void Segmenter::apply_blur(cv::Mat& img, std::vector<YoloResults>& results) {
    for (const auto& res : results) {
        if (res.mask.rows && res.mask.cols > 0) {
            cv::Mat roi = img(res.bbox);
            cv::Mat blurred_roi;
            cv::GaussianBlur(roi, blurred_roi, cv::Size(51, 51), 0);
            blurred_roi.copyTo(roi, res.mask);
        }
    }
}