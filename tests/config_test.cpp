#include "app/filesystem.hpp"

#include <fstream>
#include <functional>
#include <iostream>
#include <string>
#include <system_error>

#include "config/config.hpp"

namespace {

void writeFile(const app_fs::path& path, const std::string& content) {
    std::ofstream output(path.string(), std::ios::binary);
    output << content;
}

std::string yamlPath(const app_fs::path& path) {
    std::string value = path.string();
    for (char& character : value) {
        if (character == '\\') {
            character = '/';
        }
    }
    return value;
}

void expectConfigError(const std::function<void()>& action, const std::string& testName) {
    try {
        action();
    } catch (const config::ConfigError&) {
        return;
    }
    std::cerr << testName << " expected ConfigError\n";
    std::exit(1);
}

std::string validConfig(const app_fs::path& modelPath, const app_fs::path& manifestPath, const app_fs::path& videoPath) {
    return "app:\n"
           "  name: config-test\n"
           "  shutdown_timeout_ms: 5000\n"
           "  log_level: info\n"
           "model:\n"
           "  path: \"" + yamlPath(modelPath) + "\"\n"
           "  manifest: \"" + yamlPath(manifestPath) + "\"\n"
           "  input_width: 640\n"
           "  input_height: 640\n"
           "  confidence_threshold: 0.40\n"
           "  nms_threshold: 0.45\n"
           "  class_filter: [person, car]\n"
           "queue:\n"
           "  max_frames: 4\n"
           "  max_age_ms: 500\n"
           "  drop_policy: drop_oldest\n"
           "output:\n"
           "  directory: output\n"
           "  save_annotated_video: true\n"
           "  save_events: true\n"
           "  save_snapshots: true\n"
           "  metrics_interval_ms: 5000\n"
           "streams:\n"
           "  - id: demo-mp4\n"
           "    type: mp4\n"
           "    path: \"" + yamlPath(videoPath) + "\"\n"
           "    realtime: true\n"
           "    loop: false\n"
           "    reconnect:\n"
           "      enabled: false\n"
           "    rules:\n"
           "      tracker_enabled: true\n"
           "      cooldown_ms: 3000\n"
           "      rois: []\n"
           "      lines: []\n";
}

}

int main() {
    const app_fs::path root = app_fs::temp_directory_path() / "multi_rtsp_config_test";
    std::error_code cleanupError;
    app_fs::remove_all(root, cleanupError);
    app_fs::create_directories(root / "models");

    const auto modelPath = root / "models" / "detector.onnx";
    const auto manifestPath = root / "models" / "manifest.yaml";
    const auto videoPath = root / "demo.mp4";
    const auto validPath = root / "valid.yaml";
    writeFile(modelPath, "model");
    writeFile(manifestPath, "manifest");
    writeFile(videoPath, "video");
    writeFile(validPath, validConfig(modelPath, manifestPath, videoPath));

    const config::AppConfig config = config::loadConfig(validPath);
    config::validateConfig(config);
    if (config.streams.size() != 1 || config.streams.front().id != "demo-mp4") {
        std::cerr << "valid configuration was not loaded correctly\n";
        return 1;
    }

    const auto invalidThresholdPath = root / "invalid_threshold.yaml";
    std::string invalidThreshold = validConfig(modelPath, manifestPath, videoPath);
    invalidThreshold.replace(
        invalidThreshold.find("confidence_threshold: 0.40"),
        std::string("confidence_threshold: 0.40").size(),
        "confidence_threshold: 1.40");
    writeFile(invalidThresholdPath, invalidThreshold);
    expectConfigError([&] { config::loadConfig(invalidThresholdPath); }, "invalid threshold");

    const auto invalidPolicyPath = root / "invalid_policy.yaml";
    std::string invalidPolicy = validConfig(modelPath, manifestPath, videoPath);
    invalidPolicy.replace(
        invalidPolicy.find("drop_policy: drop_oldest"),
        std::string("drop_policy: drop_oldest").size(),
        "drop_policy: discard");
    writeFile(invalidPolicyPath, invalidPolicy);
    expectConfigError([&] { config::loadConfig(invalidPolicyPath); }, "invalid drop policy");

    const auto missingPath = root / "missing.yaml";
    writeFile(missingPath, validConfig(modelPath, manifestPath, root / "missing.mp4"));
    const config::AppConfig missingConfig = config::loadConfig(missingPath);
    expectConfigError([&] { config::validateConfig(missingConfig); }, "missing media path");

    app_fs::remove_all(root, cleanupError);
    std::cout << "config_test passed\n";
    return 0;
}
