#pragma once

#include "media/frame.hpp"
#include "vision/model_manifest.hpp"
#include "vision/detection.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace vision::detail {

struct Yolo26Transform {
    int original_width = 0;
    int original_height = 0;
    double scale = 0.0;
    int pad_left = 0;
    int pad_top = 0;
};

struct Yolo26Input {
    std::vector<float> values;
    Yolo26Transform transform;
};

Yolo26Input preprocessYolo26Frame(
    const media::FramePacket& frame,
    const ModelInputSpec& input_spec);

std::vector<Detection> decodeYolo26Output(
    const float* output,
    std::size_t output_size,
    std::size_t candidate_count,
    const ModelManifest& manifest,
    const Yolo26Transform& transform,
    double confidence_threshold,
    std::size_t max_detections,
    const std::vector<std::uint32_t>& class_filter);

}
