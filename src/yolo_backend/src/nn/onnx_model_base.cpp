#include "nn/onnx_model_base.h"

#include <iostream>
#include <onnxruntime_cxx_api.h>
#include <onnxruntime_c_api.h>

#include "constants.h"
#include "utils/common.h"


/**
 * @brief Base class for any onnx model regarding the target.
 *
 * Wraps OrtApi.
 *
 * The caller provides a model path, logid, and provider.
 *
 * See the output logs for more information on warnings/errors that occur while processing the model.
 *
 * @param[in] modelPath Path to the model file.
 * @param[in] logid Log identifier.
 * @param[in] provider Provider (e.g., "CPU" or "CUDA"). (NOTE: for now only CPU is supported)
 */

OnnxModelBase::OnnxModelBase(const char* modelPath, const char* logid, const char* provider)
//: modelPath_(modelPath), env(std::move(env)), session(std::move(session))
    : modelPath_(modelPath)
{

    // TODO: too bad passing `ORT_LOGGING_LEVEL_WARNING` by default - for some cases
    //       info level would make sense too
    env = Ort::Env(ORT_LOGGING_LEVEL_WARNING, logid);
    Ort::SessionOptions sessionOptions; // 세션 옵션 객체 생성

    // provider 문자열을 비교하여 실행 장치를 설정합니다.
    std::string providerStr(provider); 

    if (providerStr == "cuda") {
        std::vector<std::string> availableProviders = Ort::GetAvailableProviders();
        auto cudaAvailable = std::find(availableProviders.begin(), availableProviders.end(), "CUDAExecutionProvider");
        if (cudaAvailable == availableProviders.end()) {
            std::cout << "CUDA is not supported. Fallback to CPU." << std::endl;
            std::cout << "Inference device: CPU (Fallback)" << std::endl;
            // CPU는 기본값이므로 아무것도 하지 않습니다.
        }
        else {
            std::cout << "Inference device: GPU" << std::endl;
            OrtCUDAProviderOptions cudaOption;
            sessionOptions.AppendExecutionProvider_CUDA(cudaOption);
        }
    }
    else if (providerStr == "cpu") {
        // 이 버전에서는 CPU Provider를 명시적으로 추가하는 함수가 없습니다.
        // 이 블록을 비워두면 ONNX Runtime이 기본값으로 CPU를 사용합니다.
        std::cout << "Inference device: CPU" << std::endl;
    }
    else {
        // 지원하지 않는 프로바이더에 대한 에러 처리
        throw std::runtime_error("NotImplemented provider=" + providerStr);
    }
    
    #ifdef _WIN32
        auto modelPathW = get_win_path(modelPath);
        session = Ort::Session(env, modelPathW.c_str(), sessionOptions);
    #else
        // 설정된 sessionOptions를 사용하여 세션을 생성합니다.
        session = Ort::Session(env, modelPath, sessionOptions);
    #endif
    //session = Ort::Session(env)
    // https://github.com/microsoft/onnxruntime/issues/14157
    //std::vector<const char*> inputNodeNames; //
    // ----------------
    // init input names
    inputNodeNames;
    std::vector<Ort::AllocatedStringPtr> inputNodeNameAllocatedStrings; // <-- newly added
    Ort::AllocatorWithDefaultOptions allocator;
    auto inputNodesNum = session.GetInputCount();
    for (int i = 0; i < inputNodesNum; i++) {
        auto input_name = session.GetInputNameAllocated(i, allocator);
        inputNodeNameAllocatedStrings.push_back(std::move(input_name));
        inputNodeNames.push_back(inputNodeNameAllocatedStrings.back().get());
    }
    // -----------------
    // init output names
    outputNodeNames;
    auto outputNodesNum = session.GetOutputCount();
    std::vector<Ort::AllocatedStringPtr> outputNodeNameAllocatedStrings; // <-- newly added
    Ort::AllocatorWithDefaultOptions output_names_allocator;
    for (int i = 0; i < outputNodesNum; i++)
    {
        auto output_name = session.GetOutputNameAllocated(i, output_names_allocator);
        outputNodeNameAllocatedStrings.push_back(std::move(output_name));
        outputNodeNames.push_back(outputNodeNameAllocatedStrings.back().get());
    }
    // -------------------------
    // initialize model metadata
    model_metadata = session.GetModelMetadata();
    Ort::AllocatorWithDefaultOptions metadata_allocator;

    std::vector<Ort::AllocatedStringPtr> metadataAllocatedKeys = model_metadata.GetCustomMetadataMapKeysAllocated(metadata_allocator);
    std::vector<std::string> metadata_keys;
    metadata_keys.reserve(metadataAllocatedKeys.size());

    for (const Ort::AllocatedStringPtr& allocatedString : metadataAllocatedKeys) {
        metadata_keys.emplace_back(allocatedString.get());
    }

    // -------------------------
    // initialize metadata as the dict
    // even though we know exactly what metadata we intend to use
    // base onnx class should not have any ultralytics yolo-specific attributes like stride, task etc, so keep it clean as much as possible
    for (const std::string& key : metadata_keys) {
        Ort::AllocatedStringPtr metadata_value = model_metadata.LookupCustomMetadataMapAllocated(key.c_str(), metadata_allocator);
        if (metadata_value != nullptr) {
            auto raw_metadata_value = metadata_value.get();
            metadata[key] = std::string(raw_metadata_value);
        }
    }

    // initialize cstr
    for (const std::string& name : outputNodeNames) {
        outputNamesCStr.push_back(name.c_str());
    }

    for (const std::string& name : inputNodeNames)
    {
        inputNamesCStr.push_back(name.c_str());
    }

}

const std::vector<std::string>& OnnxModelBase::getInputNames() {
    return inputNodeNames;
}

const std::vector<std::string>& OnnxModelBase::getOutputNames() {
    return outputNodeNames;
}

const Ort::ModelMetadata& OnnxModelBase::getModelMetadata()
{
    return model_metadata;
}

const std::unordered_map<std::string, std::string>& OnnxModelBase::getMetadata()
{
    return metadata;
}


const Ort::Session& OnnxModelBase::getSession()
{
    return session;
}

const char* OnnxModelBase::getModelPath()
{
    return modelPath_;
}

const std::vector<const char*> OnnxModelBase::getOutputNamesCStr()
{
    return outputNamesCStr;
}

const std::vector<const char*> OnnxModelBase::getInputNamesCStr()
{
    return inputNamesCStr;
}

std::vector<Ort::Value> OnnxModelBase::forward(std::vector<Ort::Value>& inputTensors)
{
    // todo: make runOptions parameter here

    return session.Run(Ort::RunOptions{ nullptr },
        inputNamesCStr.data(),
        inputTensors.data(),
        inputNamesCStr.size(),
        outputNamesCStr.data(),
        outputNamesCStr.size());
}

//OnnxModelBase::~OnnxModelBase() {
//    // empty body
//}
