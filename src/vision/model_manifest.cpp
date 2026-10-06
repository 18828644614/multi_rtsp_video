#include "vision/model_manifest.hpp"

#include <yaml-cpp/yaml.h>

#include <cctype>
#include <cmath>
#include <set>
#include <sstream>

namespace {

using KeySet = std::set<std::string, std::less<>>;

[[noreturn]] void fail(const std::string& message) {
    throw vision::ModelManifestError(message);
}

void requireMap(const YAML::Node& node, const std::string& field) {
    if (!node || !node.IsMap()) {
        fail(field + " must be a mapping");
    }
}

void requireSequence(const YAML::Node& node, const std::string& field) {
    if (!node || !node.IsSequence()) {
        fail(field + " must be a sequence");
    }
}

YAML::Node requireNode(const YAML::Node& parent, const std::string& key, const std::string& field) {
    const YAML::Node node = parent[key];
    if (!node || node.IsNull()) {
        fail(field + " is required");
    }
    return node;
}

void validateKeys(const YAML::Node& node, const std::string& field, const KeySet& allowed) {
    for (const auto& entry : node) {
        if (!entry.first.IsScalar()) {
            fail(field + " contains a non-scalar key");
        }
        const std::string key = entry.first.as<std::string>();
        if (allowed.find(key) == allowed.end()) {
            fail(field + " contains unknown key: " + key);
        }
    }
}

template <typename T>
T readValue(const YAML::Node& node, const std::string& field) {
    try {
        return node.as<T>();
    } catch (const YAML::Exception& error) {
        fail(field + " has invalid value: " + error.what());
    }
}

bool isPlaceholder(const std::string& value) {
    return value.empty() || value.find("replace-with-") != std::string::npos;
}

vision::TensorLayout parseLayout(const std::string& value, const std::string& field) {
    if (value == "NCHW") {
        return vision::TensorLayout::Nchw;
    }
    if (value == "NHWC") {
        return vision::TensorLayout::Nhwc;
    }
    fail(field + " must be one of NCHW, NHWC");
}

vision::ColorOrder parseColor(const std::string& value, const std::string& field) {
    if (value == "RGB") {
        return vision::ColorOrder::Rgb;
    }
    if (value == "BGR") {
        return vision::ColorOrder::Bgr;
    }
    fail(field + " must be one of RGB, BGR");
}

vision::TensorDataType parseDtype(const std::string& value, const std::string& field) {
    if (value == "float32") {
        return vision::TensorDataType::Float32;
    }
    if (value == "float16") {
        return vision::TensorDataType::Float16;
    }
    if (value == "uint8") {
        return vision::TensorDataType::UInt8;
    }
    fail(field + " must be one of float32, float16, uint8");
}

vision::Normalization parseNormalization(const std::string& value, const std::string& field) {
    if (value == "none") {
        return vision::Normalization::None;
    }
    if (value == "divide_by_255") {
        return vision::Normalization::DivideBy255;
    }
    fail(field + " must be one of none, divide_by_255");
}

vision::ResizeMode parseResize(const std::string& value, const std::string& field) {
    if (value == "stretch") {
        return vision::ResizeMode::Stretch;
    }
    if (value == "letterbox") {
        return vision::ResizeMode::Letterbox;
    }
    fail(field + " must be one of stretch, letterbox");
}

bool isHexSha256(const std::string& value) {
    if (value.size() != 64) {
        return false;
    }
    for (const char character : value) {
        if (!std::isxdigit(static_cast<unsigned char>(character))) {
            return false;
        }
    }
    return true;
}

void validatePositive(int value, const std::string& field) {
    if (value <= 0) {
        fail(field + " must be positive");
    }
}

void validateRange(double value, double minimum, double maximum, const std::string& field) {
    if (!std::isfinite(value) || value < minimum || value > maximum) {
        std::ostringstream message;
        message << field << " must be in [" << minimum << ", " << maximum << "]";
        fail(message.str());
    }
}

void validateStringField(const std::string& value, const std::string& field) {
    if (isPlaceholder(value)) {
        fail(field + " must not be empty or a placeholder");
    }
}

}

namespace vision {

void validateModelManifest(const ModelManifest& manifest) {
    validateStringField(manifest.name, "name");
    if (manifest.version <= 0) {
        fail("version must be positive");
    }
    if (manifest.format != "onnx") {
        fail("format must be onnx");
    }
    validateStringField(manifest.source, "source");
    validateStringField(manifest.license, "license");
    if (!isHexSha256(manifest.sha256)) {
        fail("sha256 must contain exactly 64 hexadecimal characters");
    }

    validatePositive(manifest.input.width, "input.width");
    validatePositive(manifest.input.height, "input.height");

    if (manifest.classes.empty()) {
        fail("classes must not be empty");
    }
    std::set<std::string> classNames;
    for (std::size_t index = 0; index < manifest.classes.size(); ++index) {
        const std::string field = "classes[" + std::to_string(index) + "]";
        validateStringField(manifest.classes[index], field);
        if (!classNames.insert(manifest.classes[index]).second) {
            fail("classes contains duplicate label: " + manifest.classes[index]);
        }
    }

    validateStringField(manifest.output.format, "output.format");
    if (manifest.output.format != "ultralytics_yolo26_raw") {
        fail("output.format must be ultralytics_yolo26_raw");
    }
    validateStringField(manifest.output.confidence, "output.confidence");
    if (manifest.output.confidence != "max_class_score") {
        fail("output.confidence must be max_class_score");
    }
    validateStringField(manifest.output.nms, "output.nms");
    if (manifest.output.nms != "class_aware" && manifest.output.nms != "class_agnostic") {
        fail("output.nms must be one of class_aware, class_agnostic");
    }
    validateRange(manifest.output.nms_threshold, 0.0, 1.0, "output.nms_threshold");
}

ModelManifest loadModelManifest(const app_fs::path& path) {
    YAML::Node root;
    try {
        root = YAML::LoadFile(path.string());
    } catch (const YAML::Exception& error) {
        throw ModelManifestError(
            "failed to load model manifest '" + path.string() + "': " + error.what());
    }

    requireMap(root, "manifest");
    validateKeys(root, "manifest", {
        "name", "version", "format", "source", "license", "sha256", "input", "classes", "output"
    });

    ModelManifest manifest;
    manifest.name = readValue<std::string>(requireNode(root, "name", "name"), "name");
    manifest.version = readValue<int>(requireNode(root, "version", "version"), "version");
    manifest.format = readValue<std::string>(requireNode(root, "format", "format"), "format");
    manifest.source = readValue<std::string>(requireNode(root, "source", "source"), "source");
    manifest.license = readValue<std::string>(requireNode(root, "license", "license"), "license");
    manifest.sha256 = readValue<std::string>(requireNode(root, "sha256", "sha256"), "sha256");

    const YAML::Node input = requireNode(root, "input", "input");
    requireMap(input, "input");
    validateKeys(input, "input", {
        "width", "height", "layout", "color", "dtype", "normalization", "resize"
    });
    manifest.input.width = readValue<int>(requireNode(input, "width", "input.width"), "input.width");
    manifest.input.height = readValue<int>(requireNode(input, "height", "input.height"), "input.height");
    manifest.input.layout = parseLayout(
        readValue<std::string>(requireNode(input, "layout", "input.layout"), "input.layout"),
        "input.layout");
    manifest.input.color = parseColor(
        readValue<std::string>(requireNode(input, "color", "input.color"), "input.color"),
        "input.color");
    manifest.input.dtype = parseDtype(
        readValue<std::string>(requireNode(input, "dtype", "input.dtype"), "input.dtype"),
        "input.dtype");
    manifest.input.normalization = parseNormalization(
        readValue<std::string>(requireNode(input, "normalization", "input.normalization"), "input.normalization"),
        "input.normalization");
    manifest.input.resize = parseResize(
        readValue<std::string>(requireNode(input, "resize", "input.resize"), "input.resize"),
        "input.resize");

    const YAML::Node classes = requireNode(root, "classes", "classes");
    requireSequence(classes, "classes");
    manifest.classes.reserve(classes.size());
    for (std::size_t index = 0; index < classes.size(); ++index) {
        manifest.classes.push_back(readValue<std::string>(
            classes[index], "classes[" + std::to_string(index) + "]"));
    }

    const YAML::Node output = requireNode(root, "output", "output");
    requireMap(output, "output");
    validateKeys(output, "output", {"format", "confidence", "nms", "nms_threshold"});
    manifest.output.format = readValue<std::string>(
        requireNode(output, "format", "output.format"), "output.format");
    manifest.output.confidence = readValue<std::string>(
        requireNode(output, "confidence", "output.confidence"), "output.confidence");
    manifest.output.nms = readValue<std::string>(
        requireNode(output, "nms", "output.nms"), "output.nms");
    manifest.output.nms_threshold = readValue<double>(
        requireNode(output, "nms_threshold", "output.nms_threshold"), "output.nms_threshold");

    validateModelManifest(manifest);
    return manifest;
}

std::string toString(TensorLayout value) {
    switch (value) {
    case TensorLayout::Nchw:
        return "NCHW";
    case TensorLayout::Nhwc:
        return "NHWC";
    }
    return "unknown";
}

std::string toString(ColorOrder value) {
    switch (value) {
    case ColorOrder::Rgb:
        return "RGB";
    case ColorOrder::Bgr:
        return "BGR";
    }
    return "unknown";
}

std::string toString(TensorDataType value) {
    switch (value) {
    case TensorDataType::Float32:
        return "float32";
    case TensorDataType::Float16:
        return "float16";
    case TensorDataType::UInt8:
        return "uint8";
    }
    return "unknown";
}

std::string toString(Normalization value) {
    switch (value) {
    case Normalization::None:
        return "none";
    case Normalization::DivideBy255:
        return "divide_by_255";
    }
    return "unknown";
}

std::string toString(ResizeMode value) {
    switch (value) {
    case ResizeMode::Stretch:
        return "stretch";
    case ResizeMode::Letterbox:
        return "letterbox";
    }
    return "unknown";
}

}
