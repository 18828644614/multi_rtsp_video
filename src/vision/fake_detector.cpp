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
    result.metadata.stream_id = frame.metadata.stream_id;
    result.metadata.sequence = frame.metadata.sequence;
    result.metadata.source_epoch = frame.metadata.source_epoch;
    result.metadata.pts = frame.metadata.pts;
    result.metadata.time_base = frame.metadata.time_base;
    result.metadata.received_at_unix_ms = frame.metadata.received_at_unix_ms;
    result.metadata.received_at_steady_ms = frame.metadata.received_at_steady_ms;
    result.metadata.width = frame.metadata.width;
    result.metadata.height = frame.metadata.height;

    if (frame.metadata.width > 0 && frame.metadata.height > 0) {
        const double width = static_cast<double>(frame.metadata.width);
        const double height = static_cast<double>(frame.metadata.height);
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
