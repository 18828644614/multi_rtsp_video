#pragma once

#include "app/filesystem.hpp"

#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

namespace config {

enum class StreamType {
    Mp4,
    Rtsp
};

enum class DropPolicy {
    DropOldest,
    DropNewest,
    Block
};

class ConfigError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

struct AppSettings {
    std::string name;
    int shutdown_timeout_ms = 0;
    std::string log_level;
};

struct ModelSettings {
    app_fs::path path;
    app_fs::path manifest;
    double confidence_threshold = 0.0;
    std::vector<std::string> class_filter;
};

struct QueueSettings {
    int max_frames = 0;
    int max_age_ms = 0;
    DropPolicy drop_policy = DropPolicy::DropOldest;
};

struct OutputSettings {
    app_fs::path directory;
    bool save_annotated_video = false;
    bool save_events = false;
    bool save_snapshots = false;
    int metrics_interval_ms = 0;
};

struct ReconnectSettings {
    bool enabled = false;
    int connect_timeout_ms = 0;
    int read_timeout_ms = 0;
    int initial_backoff_ms = 0;
    int max_backoff_ms = 0;
    int max_retries = 0;
};

struct RulesSettings {
    bool tracker_enabled = false;
    int cooldown_ms = 0;
};

struct StreamSettings {
    std::string id;
    StreamType type = StreamType::Mp4;
    app_fs::path path;
    std::string url;
    std::string transport;
    bool realtime = false;
    bool loop = false;
    ReconnectSettings reconnect;
    RulesSettings rules;
    QueueSettings queue;
};

struct AppConfig {
    app_fs::path source_path;
    app_fs::path base_directory;
    AppSettings app;
    ModelSettings model;
    QueueSettings queue;
    OutputSettings output;
    std::vector<StreamSettings> streams;
};

AppConfig loadConfig(const app_fs::path& path);
void validateConfig(const AppConfig& config);
std::string toString(StreamType type);
std::string toString(DropPolicy policy);

}
