#pragma once

#include "app/filesystem.hpp"

#include <stdexcept>
#include <string>
#include <vector>

namespace vision {

enum class TensorLayout {
    Nchw,
    Nhwc
};

enum class ColorOrder {
    Rgb,
    Bgr
};

enum class TensorDataType {
    Float32,
    Float16,
    UInt8
};

enum class Normalization {
    None,
    DivideBy255
};

enum class ResizeMode {
    Stretch,
    Letterbox
};

struct ModelInputSpec {
    int width = 0;
    int height = 0;
    TensorLayout layout = TensorLayout::Nchw;
    ColorOrder color = ColorOrder::Rgb;
    TensorDataType dtype = TensorDataType::Float32;
    Normalization normalization = Normalization::DivideBy255;
    ResizeMode resize = ResizeMode::Letterbox;
};

struct ModelOutputSpec {
    std::string format;
    std::string confidence;
    std::string nms;
    double nms_threshold = 0.0;
};

struct ModelManifest {
    std::string name;
    int version = 0;
    std::string format;
    std::string source;
    std::string license;
    std::string sha256;
    ModelInputSpec input;
    std::vector<std::string> classes;
    ModelOutputSpec output;
};

class ModelManifestError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

ModelManifest loadModelManifest(const app_fs::path& path);
void validateModelManifest(const ModelManifest& manifest);

std::string toString(TensorLayout value);
std::string toString(ColorOrder value);
std::string toString(TensorDataType value);
std::string toString(Normalization value);
std::string toString(ResizeMode value);

}