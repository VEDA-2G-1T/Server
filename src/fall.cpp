#include "fall.h"
#include <algorithm>
#include <iostream>
#include <cmath>

Fall::Fall(const std::string& model_path)
    : xnnpack_delegate(nullptr, &TfLiteXNNPackDelegateDelete)
{
    // 클래스 이름 초기화
    class_names = { "fall", "stand" };

    // TFLite 모델 로드 및 초기화 (이전과 동일)
    model = tflite::FlatBufferModel::BuildFromFile(model_path.c_str());
    if (!model) {
        throw std::runtime_error("모델 로드 실패: " + model_path);
    }
    tflite::ops::builtin::BuiltinOpResolver resolver;
    tflite::InterpreterBuilder(*model, resolver)(&interpreter);
    if (!interpreter) {
        throw std::runtime_error("인터프리터 생성 실패");
    }
    TfLiteXNNPackDelegateOptions xnnpack_options = TfLiteXNNPackDelegateOptionsDefault();
    xnnpack_options.num_threads = 4;
    xnnpack_delegate.reset(TfLiteXNNPackDelegateCreate(&xnnpack_options));
    if (interpreter->ModifyGraphWithDelegate(xnnpack_delegate.get()) != kTfLiteOk) {
        std::cerr << "XNNPACK 델리게이트 추가 실패!" << std::endl;
    }
    interpreter->AllocateTensors();
    input_idx = interpreter->inputs()[0];
    TfLiteIntArray* in_dims = interpreter->tensor(input_idx)->dims;
    in_h = in_dims->data[1];
    in_w = in_dims->data[2];
    in_c = in_dims->data[3];
    input_scale = interpreter->tensor(input_idx)->params.scale;
    input_zero_point = interpreter->tensor(input_idx)->params.zero_point;
    output_idx = interpreter->outputs()[0];
    TfLiteIntArray* out_dims = interpreter->tensor(output_idx)->dims;
    out_num_attr = out_dims->data[1];
    out_num_det = out_dims->data[2];
    output_scale = interpreter->tensor(output_idx)->params.scale;
    output_zero_point = interpreter->tensor(output_idx)->params.zero_point;
}

const std::vector<std::string>& Fall::get_class_names() const {
    return class_names;
}

std::vector<DetectionResult> Fall::detect(const cv::Mat& image, float conf_threshold, float nms_threshold) {
    // --- 입력 전처리 ---
    cv::Mat resized, rgb;
    cv::resize(image, resized, cv::Size(in_w, in_h));
    cv::cvtColor(resized, rgb, cv::COLOR_BGR2RGB);
    int8_t* input_ptr = interpreter->typed_tensor<int8_t>(input_idx);
    for (int i = 0; i < in_h * in_w * in_c; ++i) {
        float normalized_float = static_cast<float>(rgb.data[i]) / 255.0f;
        int32_t quant_val = static_cast<int32_t>(std::round(normalized_float / input_scale + input_zero_point));
        input_ptr[i] = static_cast<int8_t>(std::max(-128, std::min(quant_val, 127)));
    }
    
    // --- 추론 ---
    interpreter->Invoke();
    
    // --- 후처리 ---
    int8_t* out_data_int8 = interpreter->typed_tensor<int8_t>(output_idx);
    std::vector<cv::Rect> boxes;
    std::vector<float> scores;
    std::vector<int> classes;
    int W0 = image.cols, H0 = image.rows;

    for (int i = 0; i < out_num_det; ++i) {
        float best_conf = -1.0f;
        int class_id = -1;
        for (size_t k = 0; k < class_names.size(); ++k) {
            float current_conf = (static_cast<float>(out_data_int8[(4 + k) * out_num_det + i]) - output_zero_point) * output_scale;
            if (current_conf > best_conf) {
                best_conf = current_conf;
                class_id = k;
            }
        }
        if (best_conf < conf_threshold) continue;
        
        float cx = (static_cast<float>(out_data_int8[0 * out_num_det + i]) - output_zero_point) * output_scale;
        float cy = (static_cast<float>(out_data_int8[1 * out_num_det + i]) - output_zero_point) * output_scale;
        float w  = (static_cast<float>(out_data_int8[2 * out_num_det + i]) - output_zero_point) * output_scale;
        float h  = (static_cast<float>(out_data_int8[3 * out_num_det + i]) - output_zero_point) * output_scale;
        
        int x1 = static_cast<int>((cx - w/2) * W0);
        int y1 = static_cast<int>((cy - h/2) * H0);
        int w_box = static_cast<int>(w * W0);
        int h_box = static_cast<int>(h * H0);
        
        boxes.emplace_back(x1, y1, w_box, h_box);
        scores.push_back(best_conf);
        classes.push_back(class_id);
    }

    std::vector<int> nms_idx;
    if (!boxes.empty()) {
        cv::dnn::NMSBoxes(boxes, scores, conf_threshold, nms_threshold, nms_idx);
    }

    std::vector<DetectionResult> results;
    for (int idx : nms_idx) {
        results.push_back({boxes[idx], scores[idx], classes[idx]});
    }
    return results; 
}