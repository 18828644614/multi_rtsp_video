#include "app/filesystem.hpp"
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

#include "app/version.hpp"
#include "config/config.hpp"
#include "media/ffmpeg_mp4_decoder.hpp"

namespace {

int decodeMp4(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "usage: " << argv[0] << " --decode-mp4 <input.mp4> [max-frames]\n";
        return 2;
    }

    int maxFrames = 0;
    if (argc >= 4) {
        maxFrames = std::stoi(argv[3]);
        if (maxFrames < 0) {
            throw std::invalid_argument("max-frames must not be negative");
        }
    }

    media::FfmpegMp4Decoder decoder(argv[2], "mp4");
    decoder.open();
    const media::VideoInfo& info = decoder.info();
    std::cout << "input: " << info.path.string() << "\n";
    std::cout << "codec: " << info.codec_name << "\n";
    std::cout << "size: " << info.width << "x" << info.height << "\n";
    std::cout << "time_base: " << info.time_base.numerator << "/" << info.time_base.denominator << "\n";
    std::cout << "fps: " << info.average_frame_rate << "\n";

    media::DecodedFrame frame;
    std::size_t frameCount = 0;
    while (decoder.read(frame)) {
        ++frameCount;
        if (maxFrames > 0 && frameCount >= static_cast<std::size_t>(maxFrames)) {
            break;
        }
    }

    std::cout << "decoded_frames: " << frameCount << "\n";
    if (frameCount > 0) {
        std::cout << "last_sequence: " << frame.sequence << "\n";
        std::cout << "last_pts: " << frame.pts << "\n";
        std::cout << "image_bytes: " << frame.image.size() << "\n";
    }
    return 0;
}

}

int main(int argc, char* argv[]) {
    std::cout << app::kName << "\n";
    std::cout << "version: " << app::kVersion << "\n";

    if (argc > 1 && std::string(argv[1]) == "--decode-mp4") {
        try {
            return decodeMp4(argc, argv);
        } catch (const std::exception& error) {
            std::cerr << "decode error: " << error.what() << "\n";
            return 1;
        }
    }

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

