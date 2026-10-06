#include "output/detection_jsonl_sink.hpp"
#include "vision/fake_detector.hpp"

#include <yaml-cpp/yaml.h>

#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

media::FramePacket makeFrame() {
    media::FramePacket frame;
    frame.stream_id = "cam-01";
    frame.sequence = 1024;
    frame.pts = 9000;
    frame.time_base = {1, 90000};
    frame.width = 640;
    frame.height = 480;
    frame.stride = 640 * 3;
    frame.capture_time_ms = 1730000000123;
    frame.monotonic_time_ms = 987654321;
    frame.image.assign(static_cast<std::size_t>(frame.stride * frame.height), 0);
    return frame;
}

void testFakeDetectorAndJsonlSink() {
    vision::FakeDetector detector(0, "person", 0.9);
    const vision::DetectionResult result = detector.detect(makeFrame());

    require(result.stream_id == "cam-01", "fake detector did not preserve stream ID");
    require(result.sequence == 1024, "fake detector did not preserve sequence");
    require(result.capture_time_ms == 1730000000123, "fake detector did not preserve timestamp");
    require(result.width == 640 && result.height == 480, "fake detector did not preserve dimensions");
    require(result.detections.size() == 1, "fake detector did not emit its deterministic detection");
    require(result.detections.front().bbox.isWithinFrame(result.width, result.height),
            "fake detector emitted an invalid bounding box");

    std::ostringstream serialized;
    output::DetectionJsonlSink sink(serialized);
    sink.write(result);

    const std::string jsonl = serialized.str();
    require(!jsonl.empty() && jsonl.back() == '\n', "JSONL output did not end with a newline");
    require(jsonl.find('\n') == jsonl.size() - 1, "JSONL output contained more than one line");

    const YAML::Node document = YAML::Load(jsonl.substr(0, jsonl.size() - 1));
    require(document["schema_version"].as<int>() == 1, "schema version was incorrect");
    require(document["type"].as<std::string>() == "detection", "JSON type was incorrect");
    require(document["stream_id"].as<std::string>() == "cam-01", "stream ID was incorrect");
    require(document["sequence"].as<std::uint64_t>() == 1024, "sequence was incorrect");
    require(document["timestamp_ms"].as<std::int64_t>() == 1730000000123,
            "timestamp was incorrect");
    require(document["width"].as<int>() == 640 && document["height"].as<int>() == 480,
            "dimensions were incorrect");
    require(document["objects"].size() == 1, "object count was incorrect");
    require(document["objects"][0]["label"].as<std::string>() == "person",
            "detection label was incorrect");
    require(document["objects"][0]["bbox"]["width"].as<double>() == 320.0,
            "detection width was incorrect");
}

void testJsonStringEscaping() {
    vision::DetectionResult result;
    result.stream_id = "cam\"\\\n01";
    result.width = 100;
    result.height = 100;
    result.detections.push_back({0, "line\n\"label", 0.5, {1.0, 2.0, 3.0, 4.0}});

    std::ostringstream serialized;
    output::DetectionJsonlSink sink(serialized);
    sink.write(result);

    const std::string json = serialized.str();
    require(json.find("cam\\\"\\\\\\n01") != std::string::npos,
            "stream ID was not JSON escaped");
    require(json.find("line\\n\\\"label") != std::string::npos,
            "detection label was not JSON escaped");
    YAML::Load(json.substr(0, json.size() - 1));
}

void testSinkRejectsInvalidDetectionValues() {
    vision::DetectionResult result;
    result.width = 100;
    result.height = 100;
    result.detections.push_back({0, "person", 1.1, {1.0, 2.0, 3.0, 4.0}});

    std::ostringstream serialized;
    output::DetectionJsonlSink sink(serialized);
    bool rejected = false;
    try {
        sink.write(result);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    require(rejected, "sink accepted an out-of-range confidence");

    result.detections.front().confidence = 0.5;
    result.detections.front().bbox = {-1.0, 2.0, 3.0, 4.0};
    rejected = false;
    try {
        sink.write(result);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    require(rejected, "sink accepted a bounding box outside the frame");
}

}

int main() {
    try {
        testFakeDetectorAndJsonlSink();
        testJsonStringEscaping();
        testSinkRejectsInvalidDetectionValues();
        std::cout << "detection_jsonl_test passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "detection_jsonl_test failed: " << error.what() << "\n";
        return 1;
    }
}
