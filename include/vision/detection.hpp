#pragma once

#include "media/frame.hpp"

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace vision {

struct BoundingBox {
    double x = 0.0;
    double y = 0.0;
    double width = 0.0;
    double height = 0.0;

    bool isWithinFrame(int frame_width, int frame_height) const noexcept {
        return frame_width > 0 && frame_height > 0 &&
               std::isfinite(x) && std::isfinite(y) &&
               std::isfinite(width) && std::isfinite(height) &&
               x >= 0.0 && y >= 0.0 && width > 0.0 && height > 0.0 &&
               x + width <= static_cast<double>(frame_width) &&
               y + height <= static_cast<double>(frame_height);
    }
};

struct Detection {
    std::uint32_t class_id = 0;
    std::string label;
    double confidence = 0.0;
    BoundingBox bbox;
};

struct DetectionResult {
    media::FrameMetadata metadata;
    std::vector<Detection> detections;
};

}
