#pragma once

#include "vision/detector.hpp"

#include <cstdint>
#include <string>

namespace vision {

class FakeDetector final : public Detector {
public:
    explicit FakeDetector(
        std::uint32_t class_id = 0,
        std::string label = "person",
        double confidence = 0.9);

    DetectionResult detect(const media::FramePacket& frame) override;

private:
    std::uint32_t class_id_;
    std::string label_;
    double confidence_;
};

}
