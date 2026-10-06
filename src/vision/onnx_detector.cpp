#include "vision/onnx_detector.hpp"

#include "vision/detail/yolo26.hpp"
#include "vision/model_manifest.hpp"

#include <onnxruntime_cxx_api.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

Ort::Env& onnxEnvironment() {
    static Ort::Env environment(ORT_LOGGING_LEVEL_WARNING, "multi_rtsp_video_analysis");
    return environment;
}

Ort::Session createSession(
    Ort::Env& environment,
    const app_fs::path& model_path,
    const Ort::SessionOptions& options) {
#ifdef _WIN32
    const std::wstring path = model_path.wstring();
    return Ort::Session(environment, path.c_str(), options);
#else
    const std::string path = model_path.string();
    return Ort::Session(environment, path.c_str(), options);
#endif
}

void requireTensor(
    const Ort::TypeInfo& type_info,
    ONNXTensorElementDataType expected_type,
    const std::vector<std::int64_t>& expected_shape,
    const std::string& tensor_name) {
    if (type_info.GetONNXType() != ONNX_TYPE_TENSOR) {
        throw std::runtime_error(tensor_name + " must be a tensor");
    }
    const auto tensor_info = type_info.GetTensorTypeAndShapeInfo();
    if (tensor_info.GetElementType() != expected_type) {
        throw std::runtime_error(tensor_name + " has an unsupported element type");
    }
    if (tensor_info.GetShape() != expected_shape) {
        throw std::runtime_error(tensor_name + " shape does not match the YOLO26 manifest contract");
    }
}

}

namespace vision {

struct OnnxDetector::Impl {
    ModelManifest manifest;
    OnnxDetectorOptions options;
    Ort::SessionOptions session_options;
    std::unique_ptr<Ort::Session> session;
    std::string input_name;
    std::string output_name;
    std::size_t candidate_count = 8400;

    Impl(const app_fs::path& model_path, const app_fs::path& manifest_path, OnnxDetectorOptions detector_options)
        : manifest(loadModelManifest(manifest_path)), options(detector_options) {
        validateConfiguration();

        if (options.intra_op_threads > 0) {
            session_options.SetIntraOpNumThreads(options.intra_op_threads);
        }
        session_options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
        session = std::make_unique<Ort::Session>(createSession(onnxEnvironment(), model_path, session_options));
        validateModelSignature();
    }

    void validateConfiguration() const {
        const ModelInputSpec& input = manifest.input;
        if (manifest.format != "onnx" ||
            manifest.output.format != "ultralytics_yolo26_raw" ||
            manifest.output.confidence != "max_class_score") {
            throw std::invalid_argument("OnnxDetector only supports the ultralytics_yolo26_raw format");
        }
        if (input.width != 640 || input.height != 640 ||
            input.layout != TensorLayout::Nchw ||
            input.color != ColorOrder::Rgb ||
            input.dtype != TensorDataType::Float32 ||
            input.normalization != Normalization::DivideBy255 ||
            input.resize != ResizeMode::Letterbox) {
            throw std::invalid_argument(
                "OnnxDetector supports only 640x640 NCHW RGB float32, divide_by_255, letterbox input");
        }
        if (manifest.classes.size() != 80) {
            throw std::invalid_argument("YOLO26n COCO manifest must contain exactly 80 classes");
        }
        if (options.intra_op_threads < 0) {
            throw std::invalid_argument("intra_op_threads must not be negative");
        }
        if (options.max_detections == 0) {
            throw std::invalid_argument("max_detections must be positive");
        }
        if (!std::isfinite(options.confidence_threshold) ||
            options.confidence_threshold < 0.0 || options.confidence_threshold > 1.0) {
            throw std::invalid_argument("confidence_threshold must be in [0, 1]");
        }
    }

    void validateModelSignature() {
        if (session->GetInputCount() != 1 || session->GetOutputCount() != 1) {
            throw std::runtime_error("YOLO26 model must have exactly one input and one output");
        }

        Ort::AllocatorWithDefaultOptions allocator;
        auto inputNameAllocated = session->GetInputNameAllocated(0, allocator);
        auto outputNameAllocated = session->GetOutputNameAllocated(0, allocator);
        input_name = inputNameAllocated.get();
        output_name = outputNameAllocated.get();

        requireTensor(
            session->GetInputTypeInfo(0),
            ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT,
            {1, 3, 640, 640},
            "model input");
        requireTensor(
            session->GetOutputTypeInfo(0),
            ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT,
            {1, 84, 8400},
            "model output");
    }

    DetectionResult infer(const media::FramePacket& frame) {
        detail::Yolo26Input prepared = detail::preprocessYolo26Frame(frame, manifest.input);
        const std::array<std::int64_t, 4> inputShape = {
            1, 3, manifest.input.height, manifest.input.width};
        Ort::MemoryInfo memoryInfo = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
        Ort::Value inputTensor = Ort::Value::CreateTensor<float>(
            memoryInfo,
            prepared.values.data(),
            prepared.values.size(),
            inputShape.data(),
            inputShape.size());

        const char* inputNames[] = {input_name.c_str()};
        const char* outputNames[] = {output_name.c_str()};
        std::vector<Ort::Value> outputs = session->Run(
            Ort::RunOptions{nullptr},
            inputNames,
            &inputTensor,
            1,
            outputNames,
            1);
        if (outputs.size() != 1 || !outputs.front().IsTensor()) {
            throw std::runtime_error("YOLO26 inference did not return a tensor output");
        }

        const auto outputInfo = outputs.front().GetTensorTypeAndShapeInfo();
        if (outputInfo.GetElementType() != ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT ||
            outputInfo.GetShape() != std::vector<std::int64_t>{1, 84, 8400}) {
            throw std::runtime_error("YOLO26 inference output shape or type changed at runtime");
        }

        DetectionResult result;
        result.stream_id = frame.stream_id;
        result.sequence = frame.sequence;
        result.pts = frame.pts;
        result.time_base = frame.time_base;
        result.capture_time_ms = frame.capture_time_ms;
        result.monotonic_time_ms = frame.monotonic_time_ms;
        result.width = frame.width;
        result.height = frame.height;
        result.detections = detail::decodeYolo26Output(
            outputs.front().GetTensorData<float>(),
            outputInfo.GetElementCount(),
            candidate_count,
            manifest,
            prepared.transform,
            options.confidence_threshold,
            options.max_detections);
        return result;
    }
};

OnnxDetector::OnnxDetector(
    app_fs::path model_path,
    app_fs::path manifest_path,
    OnnxDetectorOptions options)
    : impl_(std::make_unique<Impl>(model_path, manifest_path, options)) {}

OnnxDetector::~OnnxDetector() = default;
OnnxDetector::OnnxDetector(OnnxDetector&&) noexcept = default;
OnnxDetector& OnnxDetector::operator=(OnnxDetector&&) noexcept = default;

DetectionResult OnnxDetector::detect(const media::FramePacket& frame) {
    return impl_->infer(frame);
}

}
