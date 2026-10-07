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
    result.metadata.stream_id = "camera-01";
    result.metadata.sequence = 42;
    result.metadata.source_epoch = 2;
    result.metadata.pts = 9000;
    result.metadata.time_base = {1, 90000};
    result.metadata.received_at_unix_ms = 1730000000123;
    result.metadata.received_at_steady_ms = 987654321;
    result.metadata.width = 1920;
    result.metadata.height = 1080;
    result.detections.push_back({
        0,
        "person",
        0.95,
        {100.0, 120.0, 80.0, 240.0}
    });

    require(result.metadata.stream_id == "camera-01", "stream ID was not stored");
    require(result.metadata.sequence == 42, "frame sequence was not stored");
    require(result.metadata.source_epoch == 2, "source epoch was not stored");
    require(result.metadata.pts == 9000, "media PTS was not stored");
    require(result.metadata.time_base.numerator == 1 && result.metadata.time_base.denominator == 90000, "PTS time base was not stored");
    require(result.metadata.received_at_unix_ms == 1730000000123, "capture wall-clock time was not stored");
    require(result.metadata.received_at_steady_ms == 987654321, "capture monotonic time was not stored");
    require(result.metadata.width == 1920 && result.metadata.height == 1080, "frame dimensions were not stored");
    require(result.detections.size() == 1, "detection was not stored");
    require(result.detections.front().class_id == 0, "class ID was not stored");
    require(result.detections.front().label == "person", "class label was not stored");
    require(result.detections.front().confidence == 0.95, "confidence was not stored");
    require(result.detections.front().bbox.isWithinFrame(result.metadata.width, result.metadata.height), "stored detection box is outside the frame");
}

class TestDetector final : public vision::Detector {
public:
    vision::DetectionResult detect(const media::FramePacket& frame) override {
        vision::DetectionResult result;
        result.metadata.stream_id = frame.metadata.stream_id;
        result.metadata.sequence = frame.metadata.sequence;
        result.metadata.source_epoch = frame.metadata.source_epoch;
        result.metadata.pts = frame.metadata.pts;
        result.metadata.time_base = frame.metadata.time_base;
        result.metadata.received_at_unix_ms = frame.metadata.received_at_unix_ms;
        result.metadata.received_at_steady_ms = frame.metadata.received_at_steady_ms;
        result.metadata.width = frame.metadata.width;
        result.metadata.height = frame.metadata.height;
        return result;
    }
};

void testDetectorInterfaceUsesFrameAndReturnsResult() {
    TestDetector detector;
    media::FramePacket frame;
    frame.metadata.stream_id = "camera-02";
    frame.metadata.sequence = 7;
    frame.metadata.source_epoch = 1;
    frame.metadata.width = 640;
    frame.metadata.height = 360;

    const vision::DetectionResult result = detector.detect(frame);
    require(result.metadata.stream_id == frame.metadata.stream_id && result.metadata.sequence == frame.metadata.sequence &&
        result.metadata.source_epoch == frame.metadata.source_epoch, "detector interface did not preserve frame identity");
    require(result.metadata.width == frame.metadata.width && result.metadata.height == frame.metadata.height, "detector interface did not preserve frame dimensions");
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