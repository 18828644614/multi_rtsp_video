#include "vision/detection.hpp"
#include "vision/detector.hpp"

#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void testBoundingBoxCreationAndBounds() {
    const vision::BoundingBox box{12.5, 20.0, 100.0, 80.0};

    require(box.x == 12.5 && box.y == 20.0, "bounding box position was not stored");
    require(box.width == 100.0 && box.height == 80.0, "bounding box size was not stored");
    require(box.isWithinFrame(640, 480), "valid bounding box was rejected");

    const vision::BoundingBox edgeBox{540.0, 400.0, 100.0, 80.0};
    require(edgeBox.isWithinFrame(640, 480), "bounding box on the right and bottom edge was rejected");
}

void testBoundingBoxRejectsInvalidCoordinates() {
    require(!vision::BoundingBox{-1.0, 0.0, 10.0, 10.0}.isWithinFrame(100, 100), "negative x coordinate was accepted");
    require(!vision::BoundingBox{0.0, -1.0, 10.0, 10.0}.isWithinFrame(100, 100), "negative y coordinate was accepted");
    require(!vision::BoundingBox{95.0, 0.0, 10.0, 10.0}.isWithinFrame(100, 100), "box extending past the right edge was accepted");
    require(!vision::BoundingBox{0.0, 95.0, 10.0, 10.0}.isWithinFrame(100, 100), "box extending past the bottom edge was accepted");
    require(!vision::BoundingBox{0.0, 0.0, 0.0, 10.0}.isWithinFrame(100, 100), "zero-width box was accepted");
    require(!vision::BoundingBox{0.0, 0.0, 10.0, 10.0}.isWithinFrame(0, 100), "zero-width frame was accepted");
    require(!vision::BoundingBox{std::numeric_limits<double>::infinity(), 0.0, 1.0, 1.0}.isWithinFrame(100, 100), "infinite coordinate was accepted");
    require(!vision::BoundingBox{0.0, std::numeric_limits<double>::quiet_NaN(), 1.0, 1.0}.isWithinFrame(100, 100), "NaN coordinate was accepted");
}

void testDetectionResultStoresFrameMetadataAndDetections() {
    vision::DetectionResult result;
    result.stream_id = "camera-01";
    result.sequence = 42;
    result.pts = 9000;
    result.time_base = {1, 90000};
    result.capture_time_ms = 1730000000123;
    result.monotonic_time_ms = 987654321;
    result.width = 1920;
    result.height = 1080;
    result.detections.push_back({
        0,
        "person",
        0.95,
        {100.0, 120.0, 80.0, 240.0}
    });

    require(result.stream_id == "camera-01", "stream ID was not stored");
    require(result.sequence == 42, "frame sequence was not stored");
    require(result.pts == 9000, "media PTS was not stored");
    require(result.time_base.numerator == 1 && result.time_base.denominator == 90000, "PTS time base was not stored");
    require(result.capture_time_ms == 1730000000123, "capture wall-clock time was not stored");
    require(result.monotonic_time_ms == 987654321, "capture monotonic time was not stored");
    require(result.width == 1920 && result.height == 1080, "frame dimensions were not stored");
    require(result.detections.size() == 1, "detection was not stored");
    require(result.detections.front().class_id == 0, "class ID was not stored");
    require(result.detections.front().label == "person", "class label was not stored");
    require(result.detections.front().confidence == 0.95, "confidence was not stored");
    require(result.detections.front().bbox.isWithinFrame(result.width, result.height), "stored detection box is outside the frame");
}

class TestDetector final : public vision::Detector {
public:
    vision::DetectionResult detect(const media::FramePacket& frame) override {
        vision::DetectionResult result;
        result.stream_id = frame.stream_id;
        result.sequence = frame.sequence;
        result.pts = frame.pts;
        result.time_base = frame.time_base;
        result.capture_time_ms = frame.capture_time_ms;
        result.monotonic_time_ms = frame.monotonic_time_ms;
        result.width = frame.width;
        result.height = frame.height;
        return result;
    }
};

void testDetectorInterfaceUsesFrameAndReturnsResult() {
    TestDetector detector;
    media::FramePacket frame;
    frame.stream_id = "camera-02";
    frame.sequence = 7;
    frame.width = 640;
    frame.height = 360;

    const vision::DetectionResult result = detector.detect(frame);
    require(result.stream_id == frame.stream_id && result.sequence == frame.sequence, "detector interface did not preserve frame identity");
    require(result.width == frame.width && result.height == frame.height, "detector interface did not preserve frame dimensions");
}

}

int main() {
    try {
        testBoundingBoxCreationAndBounds();
        testBoundingBoxRejectsInvalidCoordinates();
        testDetectionResultStoresFrameMetadataAndDetections();
        testDetectorInterfaceUsesFrameAndReturnsResult();
        std::cout << "detection_test passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "detection_test failed: " << error.what() << "\n";
        return 1;
    }
}