#pragma once

#include "media/frame.hpp"
#include "vision/detection.hpp"

namespace vision {

class Detector {
public:
    virtual ~Detector() = default;

    virtual DetectionResult detect(const media::FramePacket& frame) = 0;
};

}