#pragma once

#include "app/filesystem.hpp"
#include "media/frame.hpp"
#include "vision/detection.hpp"

#include <opencv2/videoio.hpp>

namespace output {

class AnnotatedVideoWriter final {
public:
    AnnotatedVideoWriter(
        const app_fs::path& path,
        double framesPerSecond,
        int width,
        int height);
    ~AnnotatedVideoWriter();

    AnnotatedVideoWriter(const AnnotatedVideoWriter&) = delete;
    AnnotatedVideoWriter& operator=(const AnnotatedVideoWriter&) = delete;
    AnnotatedVideoWriter(AnnotatedVideoWriter&&) = delete;
    AnnotatedVideoWriter& operator=(AnnotatedVideoWriter&&) = delete;

    void write(const media::FramePacket& frame, const vision::DetectionResult& result);
    void close() noexcept;
    bool isOpen() const noexcept;

private:
    cv::VideoWriter writer_;
    int width_ = 0;
    int height_ = 0;
};

}