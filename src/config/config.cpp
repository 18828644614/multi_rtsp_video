#include "config/config.hpp"

#include <algorithm>
#include <fstream>
#include <set>
#include <sstream>
#include <string_view>

#include <yaml-cpp/yaml.h>

namespace config {
namespace {

using KeySet = std::set<std::string, std::less<>>;

DropPolicy parseDropPolicy(const std::string& value, const std::string& field);

[[noreturn]] void fail(const std::string& message) {
    throw ConfigError(message);
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
        const std::string key = entry.first.as<std::string>();
        if (allowed.find(key) == allowed.end()) {
            fail(field + "." + key + " is not supported");
        }
    }
}

template <typename T>
T readValue(const YAML::Node& node, const std::string& field) {
    try {
        return node.as<T>();
    } catch (const YAML::Exception& error) {
        fail(field + " has an invalid value: " + std::string(error.what()));
    }
}

template <typename T>
T readOptional(const YAML::Node& parent, const std::string& key, const T& defaultValue, const std::string& field) {
    const YAML::Node node = parent[key];
    if (!node || node.IsNull()) {
        return defaultValue;
    }
    return readValue<T>(node, field + "." + key);
}

app_fs::path resolvePath(const app_fs::path& baseDirectory, const std::string& rawPath, const std::string& field) {
    if (rawPath.empty()) {
        fail(field + " must not be empty");
    }

    app_fs::path path(rawPath);
    if (path.is_relative()) {
        path = baseDirectory / path;
    }
    return app_fs::absolute(path);
}

QueueSettings parseQueueSettings(
    const YAML::Node& node,
    const std::string& field,
    const QueueSettings* inherited = nullptr) {
    requireMap(node, field);
    validateKeys(node, field, {"max_frames", "max_age_ms", "drop_policy"});

    QueueSettings settings = inherited == nullptr ? QueueSettings{} : *inherited;
    if (inherited == nullptr) {
        settings.max_frames = readValue<int>(requireNode(node, "max_frames", field + ".max_frames"), field + ".max_frames");
        settings.max_age_ms = readValue<int>(requireNode(node, "max_age_ms", field + ".max_age_ms"), field + ".max_age_ms");
        settings.drop_policy = parseDropPolicy(
            readValue<std::string>(requireNode(node, "drop_policy", field + ".drop_policy"), field + ".drop_policy"),
            field + ".drop_policy");
    } else {
        settings.max_frames = readOptional<int>(node, "max_frames", settings.max_frames, field);
        settings.max_age_ms = readOptional<int>(node, "max_age_ms", settings.max_age_ms, field);
        const YAML::Node policy = node["drop_policy"];
        if (policy && !policy.IsNull()) {
            settings.drop_policy = parseDropPolicy(readValue<std::string>(policy, field + ".drop_policy"), field + ".drop_policy");
        }
    }

    if (settings.max_frames <= 0) {
        fail(field + ".max_frames must be greater than 0");
    }
    if (settings.max_age_ms < 0) {
        fail(field + ".max_age_ms must be greater than or equal to 0");
    }
    if (settings.drop_policy == DropPolicy::Block && settings.max_age_ms > 0) {
        fail(field + ".max_age_ms must be 0 when drop_policy is block");
    }
    return settings;
}

void validateRange(double value, double minimum, double maximum, const std::string& field) {
    if (value < minimum || value > maximum) {
        std::ostringstream message;
        message << field << " must be between " << minimum << " and " << maximum;
        fail(message.str());
    }
}

void validatePositive(int value, const std::string& field) {
    if (value <= 0) {
        fail(field + " must be greater than 0");
    }
}

StreamType parseStreamType(const std::string& value, const std::string& field) {
    if (value == "mp4") {
        return StreamType::Mp4;
    }
    if (value == "rtsp") {
        return StreamType::Rtsp;
    }
    fail(field + " must be either mp4 or rtsp");
}

DropPolicy parseDropPolicy(const std::string& value, const std::string& field) {
    if (value == "drop_oldest") {
        return DropPolicy::DropOldest;
    }
    if (value == "drop_newest") {
        return DropPolicy::DropNewest;
    }
    if (value == "block") {
        return DropPolicy::Block;
    }
    fail(field + " must be one of drop_oldest, drop_newest, block");
}

std::string parseLogLevel(const YAML::Node& node) {
    const std::string value = readValue<std::string>(node, "app.log_level");
    if (value != "trace" && value != "debug" && value != "info" && value != "warn" && value != "error") {
        fail("app.log_level must be one of trace, debug, info, warn, error");
    }
    return value;
}

ReconnectSettings parseReconnect(const YAML::Node& node, const std::string& field) {
    ReconnectSettings settings;
    if (!node || node.IsNull()) {
        return settings;
    }

    requireMap(node, field);
    validateKeys(node, field, {"enabled", "connect_timeout_ms", "read_timeout_ms", "initial_backoff_ms", "max_backoff_ms", "max_retries"});
    settings.enabled = readOptional<bool>(node, "enabled", false, field);
    settings.connect_timeout_ms = readOptional<int>(node, "connect_timeout_ms", 5000, field);
    settings.read_timeout_ms = readOptional<int>(node, "read_timeout_ms", 5000, field);
    settings.initial_backoff_ms = readOptional<int>(node, "initial_backoff_ms", 500, field);
    settings.max_backoff_ms = readOptional<int>(node, "max_backoff_ms", 10000, field);
    settings.max_retries = readOptional<int>(node, "max_retries", 0, field);

    validatePositive(settings.connect_timeout_ms, field + ".connect_timeout_ms");
    validatePositive(settings.read_timeout_ms, field + ".read_timeout_ms");
    validatePositive(settings.initial_backoff_ms, field + ".initial_backoff_ms");
    validatePositive(settings.max_backoff_ms, field + ".max_backoff_ms");
    if (settings.max_backoff_ms < settings.initial_backoff_ms) {
        fail(field + ".max_backoff_ms must be greater than or equal to initial_backoff_ms");
    }
    if (settings.max_retries < 0) {
        fail(field + ".max_retries must be greater than or equal to 0");
    }
    return settings;
}

RulesSettings parseRules(const YAML::Node& node, const std::string& field) {
    RulesSettings settings;
    if (!node || node.IsNull()) {
        return settings;
    }

    requireMap(node, field);
    validateKeys(node, field, {"tracker_enabled", "cooldown_ms", "rois", "lines"});
    settings.tracker_enabled = readOptional<bool>(node, "tracker_enabled", false, field);
    settings.cooldown_ms = readOptional<int>(node, "cooldown_ms", 0, field);
    if (settings.cooldown_ms < 0) {
        fail(field + ".cooldown_ms must be greater than or equal to 0");
    }

    const YAML::Node rois = node["rois"];
    if (rois && !rois.IsNull()) {
        requireSequence(rois, field + ".rois");
        if (rois.size() != 0) {
            fail(field + ".rois are not supported yet");
        }
    }

    const YAML::Node lines = node["lines"];
    if (lines && !lines.IsNull()) {
        requireSequence(lines, field + ".lines");
        if (lines.size() != 0) {
            fail(field + ".lines are not supported yet");
        }
    }
    return settings;
}

StreamSettings parseStream(
    const YAML::Node& node,
    std::size_t index,
    const app_fs::path& baseDirectory,
    const QueueSettings& defaultQueue) {
    const std::string field = "streams[" + std::to_string(index) + "]";
    requireMap(node, field);
    validateKeys(node, field, {"id", "type", "path", "url", "transport", "realtime", "loop", "reconnect", "rules", "queue"});

    StreamSettings stream;
    stream.id = readValue<std::string>(requireNode(node, "id", field + ".id"), field + ".id");
    if (stream.id.empty()) {
        fail(field + ".id must not be empty");
    }

    const std::string type = readValue<std::string>(requireNode(node, "type", field + ".type"), field + ".type");
    stream.type = parseStreamType(type, field + ".type");
    stream.transport = readOptional<std::string>(node, "transport", "tcp", field);
    if (stream.transport != "tcp" && stream.transport != "udp") {
        fail(field + ".transport must be tcp or udp");
    }
    stream.realtime = readOptional<bool>(node, "realtime", true, field);
    stream.loop = readOptional<bool>(node, "loop", false, field);
    stream.reconnect = parseReconnect(node["reconnect"], field + ".reconnect");
    stream.rules = parseRules(node["rules"], field + ".rules");
    const YAML::Node queue = node["queue"];
    stream.queue = queue && !queue.IsNull()
                       ? parseQueueSettings(queue, field + ".queue", &defaultQueue)
                       : defaultQueue;

    const YAML::Node path = node["path"];
    const YAML::Node url = node["url"];
    if (stream.type == StreamType::Mp4) {
        stream.path = resolvePath(baseDirectory, readValue<std::string>(requireNode(node, "path", field + ".path"), field + ".path"), field + ".path");
        if (url && !url.IsNull()) {
            fail(field + ".url must not be set for an mp4 stream");
        }
    } else {
        stream.url = readValue<std::string>(requireNode(node, "url", field + ".url"), field + ".url");
        if (stream.url.rfind("rtsp://", 0) != 0 && stream.url.rfind("rtsps://", 0) != 0) {
            fail(field + ".url must start with rtsp:// or rtsps://");
        }
        if (path && !path.IsNull()) {
            fail(field + ".path must not be set for an rtsp stream");
        }
    }
    return stream;
}

}

AppConfig loadConfig(const app_fs::path& path) {
    const app_fs::path sourcePath = app_fs::absolute(path);
    if (!app_fs::exists(sourcePath)) {
        throw ConfigError("configuration file does not exist: " + sourcePath.string());
    }
    if (!app_fs::is_regular_file(sourcePath)) {
        throw ConfigError("configuration path is not a file: " + sourcePath.string());
    }

    YAML::Node root;
    try {
        root = YAML::LoadFile(sourcePath.string());
    } catch (const YAML::Exception& error) {
        throw ConfigError("failed to parse " + sourcePath.string() + ": " + std::string(error.what()));
    }

    requireMap(root, "root");
    validateKeys(root, "root", {"app", "model", "queue", "output", "streams"});

    AppConfig config;
    config.source_path = sourcePath;
    config.base_directory = app_fs::current_path();

    const YAML::Node app = requireNode(root, "app", "app");
    requireMap(app, "app");
    validateKeys(app, "app", {"name", "shutdown_timeout_ms", "log_level"});
    config.app.name = readValue<std::string>(requireNode(app, "name", "app.name"), "app.name");
    config.app.shutdown_timeout_ms = readValue<int>(requireNode(app, "shutdown_timeout_ms", "app.shutdown_timeout_ms"), "app.shutdown_timeout_ms");
    config.app.log_level = parseLogLevel(requireNode(app, "log_level", "app.log_level"));
    validatePositive(config.app.shutdown_timeout_ms, "app.shutdown_timeout_ms");

    const YAML::Node model = requireNode(root, "model", "model");
    requireMap(model, "model");
    validateKeys(model, "model", {"path", "manifest", "confidence_threshold", "class_filter"});
    config.model.path = resolvePath(config.base_directory, readValue<std::string>(requireNode(model, "path", "model.path"), "model.path"), "model.path");
    config.model.manifest = resolvePath(config.base_directory, readValue<std::string>(requireNode(model, "manifest", "model.manifest"), "model.manifest"), "model.manifest");
    config.model.confidence_threshold = readValue<double>(requireNode(model, "confidence_threshold", "model.confidence_threshold"), "model.confidence_threshold");
    validateRange(config.model.confidence_threshold, 0.0, 1.0, "model.confidence_threshold");

    const YAML::Node classFilter = requireNode(model, "class_filter", "model.class_filter");
    requireSequence(classFilter, "model.class_filter");
    for (const auto& item : classFilter) {
        const std::string label = readValue<std::string>(item, "model.class_filter item");
        if (label.empty()) {
            fail("model.class_filter must not contain empty labels");
        }
        if (std::find(config.model.class_filter.begin(), config.model.class_filter.end(), label) != config.model.class_filter.end()) {
            fail("model.class_filter contains duplicate label: " + label);
        }
        config.model.class_filter.push_back(label);
    }
    const YAML::Node queue = requireNode(root, "queue", "queue");
    config.queue = parseQueueSettings(queue, "queue");

    const YAML::Node output = requireNode(root, "output", "output");
    requireMap(output, "output");
    validateKeys(output, "output", {"directory", "save_annotated_video", "save_events", "save_snapshots", "metrics_interval_ms"});
    config.output.directory = resolvePath(config.base_directory, readValue<std::string>(requireNode(output, "directory", "output.directory"), "output.directory"), "output.directory");
    config.output.save_annotated_video = readValue<bool>(requireNode(output, "save_annotated_video", "output.save_annotated_video"), "output.save_annotated_video");
    config.output.save_events = readValue<bool>(requireNode(output, "save_events", "output.save_events"), "output.save_events");
    config.output.save_snapshots = readValue<bool>(requireNode(output, "save_snapshots", "output.save_snapshots"), "output.save_snapshots");
    config.output.metrics_interval_ms = readValue<int>(requireNode(output, "metrics_interval_ms", "output.metrics_interval_ms"), "output.metrics_interval_ms");
    validatePositive(config.output.metrics_interval_ms, "output.metrics_interval_ms");

    const YAML::Node streams = requireNode(root, "streams", "streams");
    requireSequence(streams, "streams");
    if (streams.size() == 0) {
        fail("streams must contain at least one stream");
    }
    for (std::size_t index = 0; index < streams.size(); ++index) {
        config.streams.push_back(parseStream(streams[index], index, config.base_directory, config.queue));
    }

    return config;
}

void validateConfig(const AppConfig& config) {
    if (!app_fs::exists(config.model.path) || !app_fs::is_regular_file(config.model.path)) {
        throw ConfigError("model.path does not point to a regular file: " + config.model.path.string());
    }
    if (!app_fs::exists(config.model.manifest) || !app_fs::is_regular_file(config.model.manifest)) {
        throw ConfigError("model.manifest does not point to a regular file: " + config.model.manifest.string());
    }
    if (app_fs::exists(config.output.directory) && !app_fs::is_directory(config.output.directory)) {
        throw ConfigError("output.directory is not a directory: " + config.output.directory.string());
    }

    std::set<std::string> ids;
    for (const auto& stream : config.streams) {
        if (!ids.insert(stream.id).second) {
            throw ConfigError("duplicate stream id: " + stream.id);
        }
        if (stream.type == StreamType::Mp4 && (!app_fs::exists(stream.path) || !app_fs::is_regular_file(stream.path))) {
            throw ConfigError("stream path does not point to a regular file: " + stream.path.string());
        }
    }
}

std::string toString(StreamType type) {
    return type == StreamType::Mp4 ? "mp4" : "rtsp";
}

std::string toString(DropPolicy policy) {
    switch (policy) {
    case DropPolicy::DropOldest:
        return "drop_oldest";
    case DropPolicy::DropNewest:
        return "drop_newest";
    case DropPolicy::Block:
        return "block";
    }
    return "unknown";
}

}



