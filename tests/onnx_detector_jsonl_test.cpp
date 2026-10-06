#include "output/detection_jsonl_sink.hpp"
#include "vision/onnx_detector.hpp"

#include <yaml-cpp/yaml.h>

#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

media::FramePacket makeFrame() {
    media::FramePacket frame;
    frame.stream_id = "onnx-smoke";
    frame.sequence = 7;
    frame.pts = 6300;
    frame.time_base = {1, 90000};
    frame.width = 640;
    frame.height = 480;
    frame.stride = frame.width * 3;
    frame.capture_time_ms = 1730000000123;
    frame.monotonic_time_ms = 987654321;
    frame.image.assign(static_cast<std::size_t>(frame.stride * frame.height), 114);
    return frame;
}

void testInferenceAndJsonl(const app_fs::path& modelPath, const app_fs::path& manifestPath) {
    vision::OnnxDetector detector(modelPath, manifestPath);
    const media::FramePacket frame = makeFrame();
    const vision::DetectionResult result = detector.detect(frame);

    require(result.stream_id == frame.stream_id, "stream ID was not preserved");
    require(result.sequence == frame.sequence, "sequence was not preserved");
    require(result.pts == frame.pts, "PTS was not preserved");
    require(result.capture_time_ms == frame.capture_time_ms, "capture timestamp was not preserved");
    require(result.monotonic_time_ms == frame.monotonic_time_ms, "monotonic timestamp was not preserved");
    require(result.width == frame.width && result.height == frame.height,
            "original frame dimensions were not preserved");
    for (const auto& detection : result.detections) {
        require(detection.class_id < 80, "class ID exceeded the COCO class list");
        require(!detection.label.empty(), "inference returned an empty class label");
        require(detection.confidence >= 0.25 && detection.confidence <= 1.0,
                "inference returned an invalid confidence");
        require(detection.bbox.isWithinFrame(frame.width, frame.height),
                "inference produced a box outside the original frame");
    }

    std::ostringstream jsonl;
    output::DetectionJsonlSink sink(jsonl);
    sink.write(result);
    const std::string line = jsonl.str();
    require(!line.empty() && line.back() == '\n', "JSONL sink did not write a newline");
    require(line.find('\n') == line.size() - 1, "JSONL sink wrote more than one line");

    const YAML::Node document = YAML::Load(line.substr(0, line.size() - 1));
    require(document["type"].as<std::string>() == "detection", "JSONL record type was incorrect");
    require(document["stream_id"].as<std::string>() == frame.stream_id,
            "JSONL stream ID was incorrect");
    require(document["sequence"].as<std::uint64_t>() == frame.sequence,
            "JSONL sequence was incorrect");
    require(document["timestamp_ms"].as<std::int64_t>() == frame.capture_time_ms,
            "JSONL timestamp was incorrect");
    require(document["objects"].size() == result.detections.size(),
            "JSONL detection count did not match inference");
}

}

int main(int argc, char** argv) {
    try {
        if (argc != 3) {
            throw std::invalid_argument("usage: onnx_detector_jsonl_test <model.onnx> <manifest.yaml>");
        }
        testInferenceAndJsonl(argv[1], argv[2]);
        std::cout << "onnx_detector_jsonl_test passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "onnx_detector_jsonl_test failed: " << error.what() << '\n';
        return 1;
    }
}
