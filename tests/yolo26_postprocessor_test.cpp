#include "vision/detail/yolo26.hpp"

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

vision::ModelInputSpec inputSpec(int width, int height) {
    vision::ModelInputSpec spec;
    spec.width = width;
    spec.height = height;
    spec.layout = vision::TensorLayout::Nchw;
    spec.color = vision::ColorOrder::Rgb;
    spec.dtype = vision::TensorDataType::Float32;
    spec.normalization = vision::Normalization::DivideBy255;
    spec.resize = vision::ResizeMode::Letterbox;
    return spec;
}

vision::ModelManifest outputManifest() {
    vision::ModelManifest manifest;
    manifest.classes = {"person", "car"};
    manifest.output.format = "ultralytics_yolo26_raw";
    manifest.output.confidence = "max_class_score";
    manifest.output.nms = "class_aware";
    manifest.output.nms_threshold = 0.45;
    return manifest;
}

void setValue(
    std::vector<float>& output,
    std::size_t candidateCount,
    std::size_t channel,
    std::size_t candidate,
    float value) {
    output[channel * candidateCount + candidate] = value;
}

void setBox(
    std::vector<float>& output,
    std::size_t candidateCount,
    std::size_t candidate,
    float centerX,
    float centerY,
    float width,
    float height) {
    setValue(output, candidateCount, 0, candidate, centerX);
    setValue(output, candidateCount, 1, candidate, centerY);
    setValue(output, candidateCount, 2, candidate, width);
    setValue(output, candidateCount, 3, candidate, height);
}

void testPreprocessBgrLetterboxToRgbNchw() {
    media::FramePacket frame;
    frame.width = 2;
    frame.height = 4;
    frame.stride = 2 * 3;
    frame.image.resize(static_cast<std::size_t>(frame.stride * frame.height));
    for (std::size_t index = 0; index < frame.image.size(); index += 3) {
        frame.image[index] = 10;
        frame.image[index + 1] = 20;
        frame.image[index + 2] = 30;
    }

    const vision::detail::Yolo26Input prepared =
        vision::detail::preprocessYolo26Frame(frame, inputSpec(4, 4));
    const std::size_t planeSize = 4 * 4;
    require(prepared.values.size() == planeSize * 3, "tensor size was incorrect");
    require(prepared.transform.scale == 1.0, "letterbox scale was incorrect");
    require(prepared.transform.pad_left == 1 && prepared.transform.pad_top == 0,
            "letterbox padding was incorrect");
    require(std::abs(prepared.values[0] - 114.0F / 255.0F) < 1e-6F,
            "letterbox padding value was incorrect");
    require(std::abs(prepared.values[1] - 30.0F / 255.0F) < 1e-6F,
            "BGR frame was not converted to the RGB tensor order");
    require(std::abs(prepared.values[planeSize + 1] - 20.0F / 255.0F) < 1e-6F,
            "green tensor plane was incorrect");
    require(std::abs(prepared.values[planeSize * 2 + 1] - 10.0F / 255.0F) < 1e-6F,
            "blue tensor plane was incorrect");
}

void testRejectsInvalidFramesAndUnsupportedInput() {
    media::FramePacket frame;
    frame.width = 4;
    frame.height = 4;
    frame.stride = 4 * 3;
    frame.image.resize(1);

    bool rejected = false;
    try {
        vision::detail::preprocessYolo26Frame(frame, inputSpec(4, 4));
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    require(rejected, "short frame buffer was accepted");

    frame.image.resize(static_cast<std::size_t>(frame.stride * frame.height));
    auto unsupported = inputSpec(4, 4);
    unsupported.color = vision::ColorOrder::Bgr;
    rejected = false;
    try {
        vision::detail::preprocessYolo26Frame(frame, unsupported);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    require(rejected, "unsupported color order was accepted");
}

std::vector<float> makeOutput(std::size_t candidateCount) {
    return std::vector<float>(candidateCount * 6, 0.0F);
}

void testYolo26RawDecodeAndClassAwareNms() {
    constexpr std::size_t candidateCount = 4;
    std::vector<float> output = makeOutput(candidateCount);
    setBox(output, candidateCount, 0, 50.0F, 50.0F, 40.0F, 40.0F);
    setBox(output, candidateCount, 1, 50.0F, 50.0F, 40.0F, 40.0F);
    setBox(output, candidateCount, 2, 50.0F, 50.0F, 40.0F, 40.0F);
    setBox(output, candidateCount, 3, 10.0F, 10.0F, 10.0F, 10.0F);
    setValue(output, candidateCount, 4, 0, 0.90F);
    setValue(output, candidateCount, 4, 1, 0.80F);
    setValue(output, candidateCount, 5, 2, 0.85F);
    setValue(output, candidateCount, 4, 3, 0.20F);

    const vision::detail::Yolo26Transform transform{100, 100, 1.0, 0, 0};
    vision::ModelManifest manifest = outputManifest();
    const auto detections = vision::detail::decodeYolo26Output(
        output.data(), output.size(), candidateCount, manifest, transform, 0.25, 10);

    require(detections.size() == 2, "class-aware NMS kept an incorrect number of detections");
    require(detections[0].class_id == 0 && detections[0].label == "person",
            "highest-scoring class was decoded incorrectly");
    require(std::abs(detections[0].confidence - 0.90) < 1e-6,
            "raw class score was decoded incorrectly");
    require(detections[0].bbox.isWithinFrame(100, 100),
            "decoded box is outside the original frame");
    require(detections[1].class_id == 1,
            "class-aware NMS suppressed an overlapping box from another class");

    manifest.output.nms = "class_agnostic";
    const auto classAgnostic = vision::detail::decodeYolo26Output(
        output.data(), output.size(), candidateCount, manifest, transform, 0.25, 10);
    require(classAgnostic.size() == 1, "class-agnostic NMS did not suppress cross-class overlap");
}

void testUndoLetterboxAndLimitDetections() {
    constexpr std::size_t candidateCount = 2;
    std::vector<float> output = makeOutput(candidateCount);
    setBox(output, candidateCount, 0, 100.0F, 65.0F, 40.0F, 20.0F);
    setValue(output, candidateCount, 4, 0, 0.95F);
    setBox(output, candidateCount, 1, 300.0F, 300.0F, 20.0F, 20.0F);
    setValue(output, candidateCount, 5, 1, 0.90F);

    const vision::detail::Yolo26Transform transform{100, 50, 2.0, 0, 15};
    const auto detections = vision::detail::decodeYolo26Output(
        output.data(), output.size(), candidateCount, outputManifest(), transform, 0.25, 1);
    require(detections.size() == 1, "maximum detection limit was not applied");
    require(std::abs(detections[0].bbox.x - 40.0) < 1e-6 &&
            std::abs(detections[0].bbox.y - 20.0) < 1e-6 &&
            std::abs(detections[0].bbox.width - 20.0) < 1e-6 &&
            std::abs(detections[0].bbox.height - 10.0) < 1e-6,
            "letterbox coordinates were not mapped back to the original frame");
}

}

int main() {
    try {
        testPreprocessBgrLetterboxToRgbNchw();
        testRejectsInvalidFramesAndUnsupportedInput();
        testYolo26RawDecodeAndClassAwareNms();
        testUndoLetterboxAndLimitDetections();
        std::cout << "yolo26_postprocessor_test passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "yolo26_postprocessor_test failed: " << error.what() << '\n';
        return 1;
    }
}
