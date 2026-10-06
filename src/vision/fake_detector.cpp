#include "vision/fake_detector.hpp"

#include <cmath>
#include <stdexcept>
#include <utility>

namespace vision {

FakeDetector::FakeDetector(std::uint32_t class_id, std::string label, double confidence)
    : class_id_(class_id),
      label_(std::move(label)),
      confidence_(confidence) {
    if (label_.empty()) {
        throw std::invalid_argument("fake detector label must not be empty");
    }
    if (!std::isfinite(confidence_) || confidence_ < 0.0 || confidence_ > 1.0) {
        throw std::invalid_argument("fake detector confidence must be in [0, 1]");
    }
}

DetectionResult FakeDetector::detect(const media::FramePacket& frame) {
    DetectionResult result;
    result.stream_id = frame.stream_id;
    result.sequence = frame.sequence;
    result.pts = frame.pts;
    result.time_base = frame.time_base;
    result.capture_time_ms = frame.capture_time_ms;
    result.monotonic_time_ms = frame.monotonic_time_ms;
    result.width = frame.width;
    result.height = frame.height;

    if (frame.width > 0 && frame.height > 0) {
        const double width = static_cast<double>(frame.width);
        const double height = static_cast<double>(frame.height);
        result.detections.push_back({
            class_id_,
            label_,
            confidence_,
            {width * 0.25, height * 0.25, width * 0.5, height * 0.5}
        });
    }

    return result;
}

}
