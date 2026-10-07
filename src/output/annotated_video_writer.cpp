#include "output/annotated_video_writer.hpp"

#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace {

int clampCoordinate(double value, int lower, int upper) {
    if (!std::isfinite(value)) {
        throw std::invalid_argument("detection bounding box coordinate must be finite");
    }

    const double clamped = std::clamp(value, static_cast<double>(lower), static_cast<double>(upper));
    if (clamped <= static_cast<double>(std::numeric_limits<int>::min())) {
        return std::numeric_limits<int>::min();
    }
    if (clamped >= static_cast<double>(std::numeric_limits<int>::max())) {
        return std::numeric_limits<int>::max();
    }
    return static_cast<int>(std::lround(clamped));
}

std::string makeLabel(const vision::Detection& detection) {
    std::ostringstream label;
    label << detection.label << " "
          << std::fixed << std::setprecision(2) << detection.confidence;
    return label.str();
}

}

namespace output {

AnnotatedVideoWriter::AnnotatedVideoWriter(
    const app_fs::path& path,
    double framesPerSecond,
    int width,
    int height)
    : width_(width),
      height_(height) {
    if (width_ <= 0 || height_ <= 0) {
        throw std::invalid_argument("annotated video dimensions must be positive");
    }
    if (!std::isfinite(framesPerSecond) || framesPerSecond <= 0.0) {
        throw std::invalid_argument("annotated video frame rate must be positive and finite");
    }

    const int codec = cv::VideoWriter::fourcc('m', 'p', '4', 'v');
    writer_.open(
        path.string(),
        codec,
        framesPerSecond,
        cv::Size(width_, height_),
        true);
    if (!writer_.isOpened()) {
        throw std::runtime_error("failed to open annotated video output: " + path.string());
    }
}

AnnotatedVideoWriter::~AnnotatedVideoWriter() {
    close();
}

void AnnotatedVideoWriter::write(
    const media::FramePacket& frame,
    const vision::DetectionResult& result) {
    if (!writer_.isOpened()) {
        throw std::logic_error("annotated video writer is not open");
    }
    if (frame.pixel_format != media::PixelFormat::Bgr24) {
        throw std::invalid_argument("annotated video writer requires BGR24 frames");
    }
    if (frame.metadata.width != width_ || frame.metadata.height != height_) {
        throw std::invalid_argument("frame dimensions do not match annotated video dimensions");
    }
    if (frame.stride < width_ * 3) {
        throw std::invalid_argument("frame stride is smaller than the BGR row width");
    }

    const std::size_t requiredBytes =
        static_cast<std::size_t>(frame.stride) * static_cast<std::size_t>(height_);
    if (frame.image.empty() || frame.image.size() < requiredBytes) {
        throw std::invalid_argument("frame image buffer is smaller than its dimensions");
    }

    const cv::Mat source(
        height_,
        width_,
        CV_8UC3,
        const_cast<std::uint8_t*>(frame.image.data()),
        static_cast<std::size_t>(frame.stride));
    cv::Mat annotated = source.clone();

    for (const vision::Detection& detection : result.detections) {
        const double rightValue = detection.bbox.x + detection.bbox.width;
        const double bottomValue = detection.bbox.y + detection.bbox.height;
        if (!std::isfinite(rightValue) || !std::isfinite(bottomValue) ||
            detection.bbox.width <= 0.0 || detection.bbox.height <= 0.0) {
            throw std::invalid_argument("detection bounding box must have positive finite dimensions");
        }

        const int left = clampCoordinate(detection.bbox.x, 0, width_ - 1);
        const int top = clampCoordinate(detection.bbox.y, 0, height_ - 1);
        const int right = clampCoordinate(rightValue, left + 1, width_);
        const int bottom = clampCoordinate(bottomValue, top + 1, height_);
        const cv::Rect rectangle(left, top, right - left, bottom - top);
        const cv::Scalar color(0, 255, 0);

        cv::rectangle(annotated, rectangle, color, 2, cv::LINE_AA);

        const std::string label = makeLabel(detection);
        int baseline = 0;
        const cv::Size textSize = cv::getTextSize(
            label,
            cv::FONT_HERSHEY_SIMPLEX,
            0.55,
            1,
            &baseline);
        const int labelTop = std::max(0, top - textSize.height - baseline - 4);
        const int labelBottom = std::min(height_, labelTop + textSize.height + baseline + 4);
        const int labelRight = std::min(width_, left + textSize.width + 6);
        cv::rectangle(
            annotated,
            cv::Point(left, labelTop),
            cv::Point(labelRight, labelBottom),
            color,
            cv::FILLED);
        cv::putText(
            annotated,
            label,
            cv::Point(left + 3, labelBottom - baseline - 2),
            cv::FONT_HERSHEY_SIMPLEX,
            0.55,
            cv::Scalar(0, 0, 0),
            1,
            cv::LINE_AA);
    }

    writer_.write(annotated);
}

void AnnotatedVideoWriter::close() noexcept {
    if (writer_.isOpened()) {
        writer_.release();
    }
}

bool AnnotatedVideoWriter::isOpen() const noexcept {
    return writer_.isOpened();
}

}