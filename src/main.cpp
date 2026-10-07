#include "app/filesystem.hpp"
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <system_error>

#include "app/version.hpp"
#include "config/config.hpp"
#include "media/ffmpeg_mp4_decoder.hpp"
#include "output/detection_jsonl_sink.hpp"
#include "pipeline/frame_pipeline.hpp"
#include "vision/onnx_detector.hpp"
#include <cstddef>

namespace {

pipeline::DropPolicy toPipelineDropPolicy(config::DropPolicy policy) {
    switch (policy) {
    case config::DropPolicy::DropOldest:
        return pipeline::DropPolicy::DropOldest;
    case config::DropPolicy::DropNewest:
        return pipeline::DropPolicy::DropNewest;
    case config::DropPolicy::Block:
        return pipeline::DropPolicy::Block;
    }
    throw std::invalid_argument("unsupported queue drop policy");
}

const config::StreamSettings& selectMp4Stream(
    const config::AppConfig& appConfig,
    const std::string& requestedStreamId) {
    if (!requestedStreamId.empty()) {
        for (const auto& stream : appConfig.streams) {
            if (stream.id == requestedStreamId) {
                if (stream.type != config::StreamType::Mp4) {
                    throw std::invalid_argument("stream is not an MP4 stream: " + requestedStreamId);
                }
                return stream;
            }
        }
        throw std::invalid_argument("MP4 stream was not found: " + requestedStreamId);
    }

    for (const auto& stream : appConfig.streams) {
        if (stream.type == config::StreamType::Mp4) {
            return stream;
        }
    }
    throw std::invalid_argument("configuration does not contain an MP4 stream");
}

int parseMaxFrames(const char* value) {
    const int maxFrames = std::stoi(value);
    if (maxFrames < 0) {
        throw std::invalid_argument("max-frames must not be negative");
    }
    return maxFrames;
}

int decodeMp4(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "usage: " << argv[0] << " --decode-mp4 <input.mp4> [max-frames]\n";
        return 2;
    }

    int maxFrames = 0;
    if (argc >= 4) {
        maxFrames = parseMaxFrames(argv[3]);
    }

    media::FfmpegMp4Decoder decoder(argv[2], "mp4");
    decoder.open();
    const media::VideoInfo& info = decoder.info();
    std::cout << "input: " << info.path.string() << "\n";
    std::cout << "codec: " << info.codec_name << "\n";
    std::cout << "size: " << info.width << "x" << info.height << "\n";
    std::cout << "time_base: " << info.time_base.numerator << "/" << info.time_base.denominator << "\n";
    std::cout << "fps: " << info.average_frame_rate << "\n";

    media::FramePacket frame;
    std::size_t frameCount = 0;
    while (decoder.read(frame).status == media::FrameReadStatus::Frame) {
        ++frameCount;
        if (maxFrames > 0 && frameCount >= static_cast<std::size_t>(maxFrames)) {
            break;
        }
    }

    std::cout << "decoded_frames: " << frameCount << "\n";
    if (frameCount > 0) {
        std::cout << "last_sequence: " << frame.metadata.sequence << "\n";
        std::cout << "last_pts: " << (frame.metadata.pts.has_value() ? std::to_string(*frame.metadata.pts) : "none") << "\n";
        std::cout << "image_bytes: " << frame.image.size() << "\n";
    }
    return 0;
}

int processMp4(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "usage: " << argv[0]
                  << " --process-mp4 <config.yaml> [stream-id] [max-frames]\n";
        return 2;
    }
    if (argc > 5) {
        throw std::invalid_argument("too many arguments for --process-mp4");
    }

    const app_fs::path configPath = argv[2];
    const config::AppConfig appConfig = config::loadConfig(configPath);
    config::validateConfig(appConfig);

    const std::string requestedStreamId = argc >= 4 ? argv[3] : std::string();
    const config::StreamSettings& stream = selectMp4Stream(appConfig, requestedStreamId);
    const int maxFrames = argc >= 5 ? parseMaxFrames(argv[4]) : 0;
    if (stream.loop) {
        throw std::invalid_argument("MP4 loop mode is not supported by --process-mp4: " + stream.id);
    }

    std::error_code outputDirectoryError;
    if (!app_fs::exists(appConfig.output.directory)) {
        app_fs::create_directories(appConfig.output.directory, outputDirectoryError);
        if (outputDirectoryError) {
            throw std::runtime_error(
                "failed to create output directory " + appConfig.output.directory.string() + ": " +
                outputDirectoryError.message());
        }
    }
    if (!app_fs::is_directory(appConfig.output.directory)) {
        throw std::runtime_error("output path is not a directory: " + appConfig.output.directory.string());
    }

    const app_fs::path outputPath = appConfig.output.directory / (stream.id + ".detections.jsonl");
    std::ofstream detectionOutput(outputPath.string(), std::ios::out | std::ios::trunc);
    if (!detectionOutput.is_open()) {
        throw std::runtime_error("failed to open detection output: " + outputPath.string());
    }

    vision::OnnxDetectorOptions detectorOptions;
    detectorOptions.confidence_threshold = appConfig.model.confidence_threshold;
    detectorOptions.class_filter = appConfig.model.class_filter;
    vision::OnnxDetector detector(appConfig.model.path, appConfig.model.manifest, detectorOptions);
    output::DetectionJsonlSink sink(detectionOutput);

    media::FfmpegMp4Decoder decoder(stream.path, stream.id);
    decoder.open();
    const media::VideoInfo& info = decoder.info();
    std::cout << "config: " << appConfig.source_path.string() << "\n";
    std::cout << "stream: " << stream.id << "\n";
    std::cout << "input: " << info.path.string() << "\n";
    std::cout << "codec: " << info.codec_name << "\n";
    std::cout << "size: " << info.width << "x" << info.height << "\n";
    std::cout << "model: " << appConfig.model.path.string() << "\n";
    std::cout << "manifest: " << appConfig.model.manifest.string() << "\n";
    std::cout << "confidence_threshold: " << appConfig.model.confidence_threshold << "\n";
    std::cout << "class_filter: " << appConfig.model.class_filter.size() << " labels\n";
    std::cout << "detection_output: " << outputPath.string() << "\n";

    std::size_t producedFrames = 0;
    std::size_t consumedFrames = 0;
    std::size_t detectedObjects = 0;
    pipeline::FramePipeline framePipeline({
        static_cast<std::size_t>(stream.queue.max_frames),
        static_cast<int64_t>(stream.queue.max_age_ms),
        toPipelineDropPolicy(stream.queue.drop_policy)
    });

    const pipeline::FrameQueueStats stats = framePipeline.run(
        [&](media::FramePacket& frame) {
            if (maxFrames > 0 && producedFrames >= static_cast<std::size_t>(maxFrames)) {
                return false;
            }

            const media::FrameReadResult readResult = decoder.read(frame);
            if (readResult.status == media::FrameReadStatus::Frame) {
                ++producedFrames;
                return true;
            }
            if (readResult.status == media::FrameReadStatus::EndOfStream) {
                return false;
            }

            const std::string message = readResult.message.empty() ? "unknown frame source error" : readResult.message;
            throw std::runtime_error("failed to read stream " + stream.id + ": " + message);
        },
        [&](const media::FramePacket& frame) {
            if (frame.image.empty()) {
                throw std::runtime_error("consumer received a frame with no image data");
            }

            const vision::DetectionResult result = detector.detect(frame);
            detectedObjects += result.detections.size();
            sink.write(result);
            ++consumedFrames;
        });

    detectionOutput.flush();
    if (!detectionOutput) {
        throw std::runtime_error("failed to flush detection output: " + outputPath.string());
    }

    std::cout << "produced_frames: " << producedFrames << "\n";
    std::cout << "consumed_frames: " << consumedFrames << "\n";
    std::cout << "detected_objects: " << detectedObjects << "\n";
    std::cout << "queue_peak: " << stats.peak_size << " / " << stream.queue.max_frames << "\n";
    std::cout << "queue_dropped: " << stats.dropped_oldest + stats.dropped_newest + stats.expired << "\n";
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

    if (argc > 1 && std::string(argv[1]) == "--process-mp4") {
        try {
            return processMp4(argc, argv);
        } catch (const config::ConfigError& error) {
            std::cerr << "configuration error: " << error.what() << "\n";
            return 2;
        } catch (const std::exception& error) {
            std::cerr << "pipeline error: " << error.what() << "\n";
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