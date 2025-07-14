// detector.cpp (Python 로직과 동일하게 수정)
#include "detector.h"
#include <algorithm>
#include <iostream>

Detector::Detector(const std::string& model_path, const cv::Size& input_size)
    : env(ORT_LOGGING_LEVEL_WARNING, "Detector"),
      input_size(input_size)
{
    try {
        session = std::make_unique<Ort::Session>(env, model_path.c_str(), session_options);
    } catch (const Ort::Exception& e) {
        std::cerr << "오류: Detector 모델 로드 실패 (" << model_path << "): " << e.what() << std::endl;
        exit(1);
    }

    Ort::AllocatedStringPtr input_name_ptr = session->GetInputNameAllocated(0, allocator);
    Ort::AllocatedStringPtr output_name_ptr = session->GetOutputNameAllocated(0, allocator);
    input_names_str.push_back(input_name_ptr.get());
    output_names_str.push_back(output_name_ptr.get());

    class_names = {
        "person", "ear", "ear-mufs", "face", "face-guard", "face-mask", 
        "foot", "tool", "glasses", "gloves", "helmet", "hands", "head", 
        "medical-suit", "shoes", "safety-suit", "safety-vest"
    };
}

const std::vector<std::string>& Detector::get_class_names() const {
    return class_names;
}

std::vector<DetectionResult> Detector::detect(const cv::Mat& image, float conf_threshold, float nms_threshold) {
    // 1. 전처리: Python 코드의 로직을 그대로 따릅니다.
    //    (리사이즈 -> BGR to RGB -> 0-1 정규화 -> HWC to CHW)
    cv::Mat resized_image, rgb_image, preprocessed_image;
    cv::resize(image, resized_image, input_size);
    cv::cvtColor(resized_image, rgb_image, cv::COLOR_BGR2RGB);
    rgb_image.convertTo(preprocessed_image, CV_32F, 1.0 / 255.0);

    cv::Mat blob = cv::dnn::blobFromImage(preprocessed_image);

    // 2. 추론 준비
    std::vector<int64_t> input_shape = {1, 3, (long)input_size.height, (long)input_size.width};
    auto memory_info = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
    Ort::Value input_tensor = Ort::Value::CreateTensor<float>(memory_info, (float*)blob.data, blob.total(), input_shape.data(), input_shape.size());

    std::vector<const char*> input_names;
    for (const auto& s : input_names_str) { input_names.push_back(s.c_str()); }
    std::vector<const char*> output_names;
    for (const auto& s : output_names_str) { output_names.push_back(s.c_str()); }

    auto output_tensors = session->Run(Ort::RunOptions{nullptr}, input_names.data(), &input_tensor, 1, output_names.data(), 1);

    // 3. 후처리: Python의 np.transpose(outputs[0][0])와 동일한 로직 구현
    const float* raw_output = output_tensors[0].GetTensorData<float>();
    auto output_shape = output_tensors[0].GetTensorTypeAndShapeInfo().GetShape();
    
    // ONNX 출력 (1, 84, 8400) -> (84, 8400) Mat으로 변환
    cv::Mat raw_mat(output_shape[1], output_shape[2], CV_32F, (void*)raw_output);
    // (84, 8400) -> (8400, 84) Mat으로 변환 (Python의 transpose와 동일)
    cv::Mat output_mat = raw_mat.t();

    std::vector<cv::Rect> boxes;
    std::vector<float> confidences;
    std::vector<int> class_ids;
    float x_factor = image.cols / (float)input_size.width;
    float y_factor = image.rows / (float)input_size.height;

    for (int i = 0; i < output_mat.rows; ++i) {
        cv::Mat row = output_mat.row(i);
        
        // ★★★★★ [핵심 수정] Python 코드와 동일한 후처리 로직 적용 ★★★★★
        // 클래스 확률은 4번 인덱스부터 시작합니다.
        cv::Mat class_scores = row.colRange(4, output_mat.cols);
        cv::Point class_id_point;
        double max_score;
        cv::minMaxLoc(class_scores, 0, &max_score, 0, &class_id_point);

        // 객체 신뢰도(Objectness)를 따로 고려하지 않고, 클래스 확률의 최대값만 사용합니다.
        if (max_score > conf_threshold) {
            int class_id = class_id_point.x;
            
            if (class_id < class_names.size()) {
                bool is_target = false;
                const auto& class_name = class_names[class_id];
                for(const auto& target : target_class_names) {
                    if (class_name == target) {
                        is_target = true;
                        break;
                    }
                }

                if (is_target) {
                    confidences.push_back(max_score);
                    class_ids.push_back(class_id);
                    float cx = row.at<float>(0, 0);
                    float cy = row.at<float>(0, 1);
                    float w = row.at<float>(0, 2);
                    float h = row.at<float>(0, 3);
                    int left = static_cast<int>((cx - w / 2) * x_factor);
                    int top = static_cast<int>((cy - h / 2) * y_factor);
                    int width = static_cast<int>(w * x_factor);
                    int height = static_cast<int>(h * y_factor);
                    boxes.push_back(cv::Rect(left, top, width, height));
                }
            }
        }
    }

    std::vector<int> nms_indices;
    cv::dnn::NMSBoxes(boxes, confidences, conf_threshold, nms_threshold, nms_indices);

    std::vector<DetectionResult> final_detections;
    for (int idx : nms_indices) {
        final_detections.push_back({boxes[idx], confidences[idx], class_ids[idx]});
    }
    return final_detections;
}
