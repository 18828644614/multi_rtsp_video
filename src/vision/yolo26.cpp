#include "vision/detail/yolo26.hpp"

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

struct Candidate {
    std::uint32_t class_id = 0;
    double confidence = 0.0;
    vision::BoundingBox box;
};

void validateInputSpec(const vision::ModelInputSpec& input) {
    if (input.width <= 0 || input.height <= 0) {
        throw std::invalid_argument("YOLO26 input dimensions must be positive");
    }
    if (input.layout != vision::TensorLayout::Nchw ||
        input.color != vision::ColorOrder::Rgb ||
        input.dtype != vision::TensorDataType::Float32 ||
        input.normalization != vision::Normalization::DivideBy255 ||
        input.resize != vision::ResizeMode::Letterbox) {
        throw std::invalid_argument(
            "YOLO26 supports only NCHW RGB float32, divide_by_255, letterbox input");
    }
}

void validateFrame(const media::FramePacket& frame) {
    if (frame.pixel_format != media::PixelFormat::Bgr24) {
        throw std::invalid_argument("YOLO26 detector requires BGR24 frames");
    }
    if (frame.metadata.width <= 0 || frame.metadata.height <= 0) {
        throw std::invalid_argument("frame dimensions must be positive");
    }
    if (frame.metadata.width > std::numeric_limits<int>::max() / 3 ||
        frame.stride < frame.metadata.width * 3) {
        throw std::invalid_argument("frame stride is smaller than its BGR24 row size");
    }

    const std::size_t stride = static_cast<std::size_t>(frame.stride);
    const std::size_t height = static_cast<std::size_t>(frame.metadata.height);
    if (stride > std::numeric_limits<std::size_t>::max() / height ||
        frame.image.size() < stride * height) {
        throw std::invalid_argument("frame image buffer is smaller than stride * height");
    }
}

int roundLikeUltralytics(double value) {
    return static_cast<int>(std::nearbyint(value));
}

double intersectionOverUnion(const vision::BoundingBox& lhs, const vision::BoundingBox& rhs) {
    const double left = std::max(lhs.x, rhs.x);
    const double top = std::max(lhs.y, rhs.y);
    const double right = std::min(lhs.x + lhs.width, rhs.x + rhs.width);
    const double bottom = std::min(lhs.y + lhs.height, rhs.y + rhs.height);
    const double intersectionWidth = std::max(0.0, right - left);
    const double intersectionHeight = std::max(0.0, bottom - top);
    const double intersection = intersectionWidth * intersectionHeight;
    const double unionArea = lhs.width * lhs.height + rhs.width * rhs.height - intersection;
    return unionArea > 0.0 ? intersection / unionArea : 0.0;
}

}

namespace vision::detail {

Yolo26Input preprocessYolo26Frame(
    const media::FramePacket& frame,
    const ModelInputSpec& input_spec) {
    validateInputSpec(input_spec);
    validateFrame(frame);

    const double scale = std::min(
        static_cast<double>(input_spec.width) / frame.metadata.width,
        static_cast<double>(input_spec.height) / frame.metadata.height);
    const int resizedWidth = std::max(1, roundLikeUltralytics(frame.metadata.width * scale));
    const int resizedHeight = std::max(1, roundLikeUltralytics(frame.metadata.height * scale));
    const int totalPadX = input_spec.width - resizedWidth;
    const int totalPadY = input_spec.height - resizedHeight;
    const int left = roundLikeUltralytics(totalPadX / 2.0 - 0.1);
    const int top = roundLikeUltralytics(totalPadY / 2.0 - 0.1);

    const cv::Mat source(
        frame.metadata.height,
        frame.metadata.width,
        CV_8UC3,
        const_cast<std::uint8_t*>(frame.image.data()),
        static_cast<std::size_t>(frame.stride));
    cv::Mat resized;
    cv::resize(source, resized, cv::Size(resizedWidth, resizedHeight), 0.0, 0.0, cv::INTER_LINEAR);

    cv::Mat letterboxed(
        input_spec.height,
        input_spec.width,
        CV_8UC3,
        cv::Scalar(114, 114, 114));
    resized.copyTo(letterboxed(cv::Rect(left, top, resizedWidth, resizedHeight)));

    const std::size_t tensorWidth = static_cast<std::size_t>(input_spec.width);
    const std::size_t tensorHeight = static_cast<std::size_t>(input_spec.height);
    if (tensorWidth > std::numeric_limits<std::size_t>::max() / tensorHeight) {
        throw std::overflow_error("YOLO26 input tensor size overflows size_t");
    }
    const std::size_t planeSize = tensorWidth * tensorHeight;
    if (planeSize > std::numeric_limits<std::size_t>::max() / 3) {
        throw std::overflow_error("YOLO26 input tensor size overflows size_t");
    }

    Yolo26Input prepared;
    prepared.values.resize(planeSize * 3);
    for (int row = 0; row < input_spec.height; ++row) {
        const auto* pixels = letterboxed.ptr<cv::Vec3b>(row);
        for (int column = 0; column < input_spec.width; ++column) {
            const std::size_t index =
                static_cast<std::size_t>(row) * static_cast<std::size_t>(input_spec.width) +
                static_cast<std::size_t>(column);
            const cv::Vec3b& bgr = pixels[column];
            prepared.values[index] = static_cast<float>(bgr[2]) / 255.0F;
            prepared.values[planeSize + index] = static_cast<float>(bgr[1]) / 255.0F;
            prepared.values[planeSize * 2 + index] = static_cast<float>(bgr[0]) / 255.0F;
        }
    }

    prepared.transform = {frame.metadata.width, frame.metadata.height, scale, left, top};
    return prepared;
}

std::vector<Detection> decodeYolo26Output(
    const float* output,
    std::size_t output_size,
    std::size_t candidate_count,
    const ModelManifest& manifest,
    const Yolo26Transform& transform,
    double confidence_threshold,
    std::size_t max_detections,
    const std::vector<std::uint32_t>& class_filter) {
    if (output == nullptr) {
        throw std::invalid_argument("YOLO26 output data is null");
    }
    if (manifest.output.format != "ultralytics_yolo26_raw" ||
        manifest.output.confidence != "max_class_score") {
        throw std::invalid_argument("unsupported YOLO26 output manifest");
    }
    if (manifest.classes.empty() || manifest.classes.size() > std::numeric_limits<std::uint32_t>::max()) {
        throw std::invalid_argument("YOLO26 manifest must contain a valid class list");
    }
    if (candidate_count == 0 ||
        candidate_count > std::numeric_limits<std::size_t>::max() / (manifest.classes.size() + 4)) {
        throw std::invalid_argument("YOLO26 candidate count is invalid");
    }
    const std::size_t channelCount = manifest.classes.size() + 4;
    if (output_size != candidate_count * channelCount) {
        throw std::invalid_argument("YOLO26 output tensor has an unexpected number of values");
    }
    if (!std::isfinite(confidence_threshold) || confidence_threshold < 0.0 || confidence_threshold > 1.0) {
        throw std::invalid_argument("confidence threshold must be in [0, 1]");
    }
    if (!std::isfinite(manifest.output.nms_threshold) ||
        manifest.output.nms_threshold < 0.0 || manifest.output.nms_threshold > 1.0) {
        throw std::invalid_argument("NMS threshold must be in [0, 1]");
    }
    if (manifest.output.nms != "class_aware" && manifest.output.nms != "class_agnostic") {
        throw std::invalid_argument("unsupported YOLO26 NMS mode");
    }
    if (transform.original_width <= 0 || transform.original_height <= 0 ||
        !std::isfinite(transform.scale) || transform.scale <= 0.0 ||
        transform.pad_left < 0 || transform.pad_top < 0) {
        throw std::invalid_argument("YOLO26 letterbox transform is invalid");
    }
    std::vector<bool> allowedClasses(manifest.classes.size(), class_filter.empty());
    for (const std::uint32_t classId : class_filter) {
        if (classId >= manifest.classes.size()) {
            throw std::invalid_argument("YOLO26 class filter contains an invalid class ID");
        }
        allowedClasses[classId] = true;
    }
    if (max_detections == 0) {
        return {};
    }

    std::vector<Candidate> candidates;
    for (std::size_t candidateIndex = 0; candidateIndex < candidate_count; ++candidateIndex) {
        const double centerX = output[candidateIndex];
        const double centerY = output[candidate_count + candidateIndex];
        const double width = output[candidate_count * 2 + candidateIndex];
        const double height = output[candidate_count * 3 + candidateIndex];
        if (!std::isfinite(centerX) || !std::isfinite(centerY) ||
            !std::isfinite(width) || !std::isfinite(height) || width <= 0.0 || height <= 0.0) {
            continue;
        }

        std::uint32_t bestClass = 0;
        double bestScore = -1.0;
        for (std::size_t classIndex = 0; classIndex < manifest.classes.size(); ++classIndex) {
            const double score = output[(4 + classIndex) * candidate_count + candidateIndex];
            if (!std::isfinite(score)) {
                continue;
            }
            if (score < 0.0 || score > 1.0) {
                throw std::runtime_error("YOLO26 class score is outside [0, 1]");
            }
            if (score > bestScore) {
                bestScore = score;
                bestClass = static_cast<std::uint32_t>(classIndex);
            }
        }
        if (bestScore < confidence_threshold || !allowedClasses[bestClass]) {
            continue;
        }

        const double left = (centerX - width / 2.0 - transform.pad_left) / transform.scale;
        const double top = (centerY - height / 2.0 - transform.pad_top) / transform.scale;
        const double right = (centerX + width / 2.0 - transform.pad_left) / transform.scale;
        const double bottom = (centerY + height / 2.0 - transform.pad_top) / transform.scale;
        const double clippedLeft = std::clamp(left, 0.0, static_cast<double>(transform.original_width));
        const double clippedTop = std::clamp(top, 0.0, static_cast<double>(transform.original_height));
        const double clippedRight = std::clamp(right, 0.0, static_cast<double>(transform.original_width));
        const double clippedBottom = std::clamp(bottom, 0.0, static_cast<double>(transform.original_height));
        BoundingBox box{
            clippedLeft,
            clippedTop,
            clippedRight - clippedLeft,
            clippedBottom - clippedTop};
        if (!box.isWithinFrame(transform.original_width, transform.original_height)) {
            continue;
        }
        candidates.push_back({bestClass, bestScore, box});
    }

    std::stable_sort(candidates.begin(), candidates.end(), [](const Candidate& lhs, const Candidate& rhs) {
        return lhs.confidence > rhs.confidence;
    });

    std::vector<Candidate> kept;
    kept.reserve(std::min(max_detections, candidates.size()));
    for (const Candidate& candidate : candidates) {
        bool suppressed = false;
        for (const Candidate& selected : kept) {
            if (manifest.output.nms == "class_agnostic" || candidate.class_id == selected.class_id) {
                if (intersectionOverUnion(candidate.box, selected.box) > manifest.output.nms_threshold) {
                    suppressed = true;
                    break;
                }
            }
        }
        if (!suppressed) {
            kept.push_back(candidate);
            if (kept.size() == max_detections) {
                break;
            }
        }
    }

    std::vector<Detection> detections;
    detections.reserve(kept.size());
    for (const Candidate& candidate : kept) {
        detections.push_back({
            candidate.class_id,
            manifest.classes[candidate.class_id],
            candidate.confidence,
            candidate.box});
    }
    return detections;
}

}
