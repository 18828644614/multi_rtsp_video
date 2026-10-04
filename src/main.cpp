#include "app/filesystem.hpp"
#include <iostream>

#include "app/version.hpp"
#include "config/config.hpp"

int main(int argc, char* argv[]) {
    std::cout << app::kName << "\n";
    std::cout << "version: " << app::kVersion << "\n";

    const app_fs::path configPath = argc > 1 ? argv[1] : "configs/example.yaml";
    std::cout << "loading config: " << configPath.string() << "\n";

    try {
        const config::AppConfig config = config::loadConfig(configPath);
        config::validateConfig(config);

        std::cout << "config validation passed\n";
        std::cout << "model: " << config.model.path.string() << "\n";
        std::cout << "streams: " << config.streams.size() << "\n";
        for (const auto& stream : config.streams) {
            std::cout << "stream: " << stream.id << ", type=" << config::toString(stream.type) << "\n";
        }
        return 0;
    } catch (const config::ConfigError& error) {
        std::cerr << "configuration error: " << error.what() << "\n";
        return 2;
    } catch (const std::exception& error) {
        std::cerr << "startup error: " << error.what() << "\n";
        return 1;
    }
}

