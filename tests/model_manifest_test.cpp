#include "app/filesystem.hpp"
#include "vision/model_manifest.hpp"

#include <fstream>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void writeFile(const app_fs::path& path, const std::string& content) {
    std::ofstream output(path.string(), std::ios::binary);
    require(output.good(), "could not open test file for writing: " + path.string());
    output << content;
    require(output.good(), "could not write test file: " + path.string());
}

void replaceOnce(std::string& content, const std::string& from, const std::string& to) {
    const std::size_t position = content.find(from);
    require(position != std::string::npos, "test fixture text was not found: " + from);
    content.replace(position, from.size(), to);
}

void expectManifestError(
    const std::function<void()>& action,
    const std::string& testName) {
    try {
        action();
    } catch (const vision::ModelManifestError&) {
        return;
    }
    throw std::runtime_error(testName + " expected ModelManifestError");
}

std::string validManifest() {
    return "name: test-detector\n"
           "version: 1\n"
           "format: onnx\n"
           "source: https://example.invalid/test-detector.onnx\n"
           "license: Apache-2.0\n"
           "sha256: aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\n"
           "\n"
           "input:\n"
           "  width: 640\n"
           "  height: 640\n"
           "  layout: NCHW\n"
           "  color: RGB\n"
           "  dtype: float32\n"
           "  normalization: divide_by_255\n"
           "  resize: letterbox\n"
           "\n"
           "classes:\n"
           "  - person\n"
           "  - car\n"
           "\n"
           "output:\n"
           "  format: ultralytics_yolo26_raw\n"
           "  confidence: max_class_score\n"
           "  nms: class_aware\n"
           "  nms_threshold: 0.45\n";
}

void testLoadsValidManifest(const app_fs::path& root) {
    const auto path = root / "valid.yaml";
    writeFile(path, validManifest());

    const vision::ModelManifest manifest = vision::loadModelManifest(path);
    require(manifest.name == "test-detector", "manifest name was not loaded");
    require(manifest.version == 1, "manifest version was not loaded");
    require(manifest.format == "onnx", "manifest format was not loaded");
    require(manifest.input.width == 640 && manifest.input.height == 640, "input dimensions were not loaded");
    require(manifest.input.layout == vision::TensorLayout::Nchw, "input layout was not loaded");
    require(manifest.input.color == vision::ColorOrder::Rgb, "input color was not loaded");
    require(manifest.input.dtype == vision::TensorDataType::Float32, "input dtype was not loaded");
    require(manifest.input.normalization == vision::Normalization::DivideBy255, "normalization was not loaded");
    require(manifest.input.resize == vision::ResizeMode::Letterbox, "resize mode was not loaded");
    require(manifest.classes.size() == 2, "class list size was not loaded");
    require(manifest.classes[0] == "person" && manifest.classes[1] == "car", "class order was not preserved");
    require(manifest.output.format == "ultralytics_yolo26_raw", "output format was not loaded");
    require(manifest.output.confidence == "max_class_score", "confidence rule was not loaded");
    require(manifest.output.nms == "class_aware", "NMS mode was not loaded");
    require(manifest.output.nms_threshold == 0.45, "NMS threshold was not loaded");
}

void testRejectsInvalidInputFields(const app_fs::path& root) {
    const std::vector<std::pair<std::string, std::string>> cases = {
        {"width", "  width: 0\n"},
        {"height", "  height: -1\n"},
        {"layout", "  layout: CHW\n"},
        {"color", "  color: BGRX\n"},
        {"dtype", "  dtype: int8\n"},
        {"normalization", "  normalization: mean_std\n"},
        {"resize", "  resize: crop\n"}
    };

    for (const auto& testCase : cases) {
        std::string content = validManifest();
        const std::string original = testCase.first == "width"
            ? "  width: 640\n"
            : testCase.first == "height"
                ? "  height: 640\n"
                : testCase.first == "layout"
                    ? "  layout: NCHW\n"
                    : testCase.first == "color"
                        ? "  color: RGB\n"
                        : testCase.first == "dtype"
                            ? "  dtype: float32\n"
                            : testCase.first == "normalization"
                                ? "  normalization: divide_by_255\n"
                                : "  resize: letterbox\n";
        replaceOnce(content, original, testCase.second);
        const auto path = root / ("invalid_" + testCase.first + ".yaml");
        writeFile(path, content);
        expectManifestError([&] { vision::loadModelManifest(path); }, "invalid " + testCase.first);
    }
}

void testRejectsInvalidCollectionsAndOutput(const app_fs::path& root) {
    {
        std::string content = validManifest();
        replaceOnce(content, "classes:\n  - person\n  - car\n", "classes: []\n");
        const auto path = root / "empty_classes.yaml";
        writeFile(path, content);
        expectManifestError([&] { vision::loadModelManifest(path); }, "empty classes");
    }

    {
        std::string content = validManifest();
        replaceOnce(content, "  - car\n", "  - person\n");
        const auto path = root / "duplicate_classes.yaml";
        writeFile(path, content);
        expectManifestError([&] { vision::loadModelManifest(path); }, "duplicate classes");
    }

    const std::vector<std::pair<std::string, std::string>> cases = {
        {"output_format_placeholder", "  format: replace-with-model-output-format\n"},
        {"unsupported_output_format", "  format: yolo_v8_xywh\n"},
        {"unsupported_confidence_rule", "  confidence: objectness_times_class_score\n"},
        {"source_placeholder", "source: replace-with-authorized-source\n"},
        {"license_placeholder", "license: replace-with-model-license\n"},
        {"sha256_placeholder", "sha256: replace-with-sha256\n"},
        {"invalid_nms", "  nms: unsupported\n"},
        {"invalid_nms_threshold", "  nms_threshold: 1.01\n"}
    };

    for (const auto& testCase : cases) {
        std::string content = validManifest();
        std::string original;
        if (testCase.first == "output_format_placeholder" || testCase.first == "unsupported_output_format") {
            original = "  format: ultralytics_yolo26_raw\n";
        } else if (testCase.first == "unsupported_confidence_rule") {
            original = "  confidence: max_class_score\n";
        } else if (testCase.first == "source_placeholder") {
            original = "source: https://example.invalid/test-detector.onnx\n";
        } else if (testCase.first == "license_placeholder") {
            original = "license: Apache-2.0\n";
        } else if (testCase.first == "sha256_placeholder") {
            original = "sha256: aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\n";
        } else if (testCase.first == "invalid_nms") {
            original = "  nms: class_aware\n";
        } else {
            original = "  nms_threshold: 0.45\n";
        }
        replaceOnce(content, original, testCase.second);
        const auto path = root / (testCase.first + ".yaml");
        writeFile(path, content);
        expectManifestError([&] { vision::loadModelManifest(path); }, testCase.first);
    }
}

void testRejectsUnknownAndMissingFields(const app_fs::path& root) {
    {
        std::string content = validManifest();
        content += "extra: true\n";
        const auto path = root / "unknown_key.yaml";
        writeFile(path, content);
        expectManifestError([&] { vision::loadModelManifest(path); }, "unknown top-level key");
    }

    {
        std::string content = validManifest();
        replaceOnce(content, "  resize: letterbox\n", "");
        const auto path = root / "missing_resize.yaml";
        writeFile(path, content);
        expectManifestError([&] { vision::loadModelManifest(path); }, "missing input.resize");
    }

    {
        std::string content = validManifest();
        replaceOnce(content, "  format: ultralytics_yolo26_raw\n", "");
        const auto path = root / "missing_output_format.yaml";
        writeFile(path, content);
        expectManifestError([&] { vision::loadModelManifest(path); }, "missing output.format");
    }
}

void testRejectsMalformedAndMissingFiles(const app_fs::path& root) {
    const auto malformedPath = root / "malformed.yaml";
    writeFile(malformedPath, "input: [\n");
    expectManifestError([&] { vision::loadModelManifest(malformedPath); }, "malformed YAML");

    const auto missingPath = root / "does-not-exist.yaml";
    expectManifestError([&] { vision::loadModelManifest(missingPath); }, "missing manifest file");
}

}

int main() {
    try {
        const app_fs::path root = app_fs::temp_directory_path() / "multi_rtsp_model_manifest_test";
        std::error_code cleanupError;
        app_fs::remove_all(root, cleanupError);
        app_fs::create_directories(root);

        testLoadsValidManifest(root);
        testRejectsInvalidInputFields(root);
        testRejectsInvalidCollectionsAndOutput(root);
        testRejectsUnknownAndMissingFields(root);
        testRejectsMalformedAndMissingFiles(root);

        app_fs::remove_all(root, cleanupError);
        std::cout << "model_manifest_test passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
