#pragma once

#include "app/filesystem.hpp"
#include "vision/detector.hpp"

#include <cstddef>
#include <memory>

namespace vision {

struct OnnxDetectorOptions {
    double confidence_threshold = 0.25;
    int intra_op_threads = 0;
    std::size_t max_detections = 300;
};

class OnnxDetector final : public Detector {
public:
    OnnxDetector(
        app_fs::path model_path,
        app_fs::path manifest_path,
        OnnxDetectorOptions options = {});
    ~OnnxDetector() override;

    OnnxDetector(const OnnxDetector&) = delete;
    OnnxDetector& operator=(const OnnxDetector&) = delete;
    OnnxDetector(OnnxDetector&&) noexcept;
    OnnxDetector& operator=(OnnxDetector&&) noexcept;

    DetectionResult detect(const media::FramePacket& frame) override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}
