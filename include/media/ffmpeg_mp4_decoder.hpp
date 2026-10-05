#pragma once

#include "app/filesystem.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace media {

struct Rational {
    int numerator = 0;
    int denominator = 1;
};

struct VideoInfo {
    app_fs::path path;
    std::string stream_id;
    std::string codec_name;
    int width = 0;
    int height = 0;
    Rational time_base;
    int64_t duration_pts = 0;
    double duration_seconds = 0.0;
    double average_frame_rate = 0.0;
};

struct DecodedFrame {
    std::string stream_id;
    uint64_t sequence = 0;
    int64_t pts = 0;
    Rational time_base;
    int width = 0;
    int height = 0;
    int stride = 0;
    int64_t capture_time_ms = 0;
    int64_t monotonic_time_ms = 0;
    std::vector<uint8_t> image;
};

class FfmpegMp4Decoder {
public:
    explicit FfmpegMp4Decoder(app_fs::path path, std::string streamId = "mp4");
    ~FfmpegMp4Decoder();

    FfmpegMp4Decoder(const FfmpegMp4Decoder&) = delete;
    FfmpegMp4Decoder& operator=(const FfmpegMp4Decoder&) = delete;
    FfmpegMp4Decoder(FfmpegMp4Decoder&&) noexcept;
    FfmpegMp4Decoder& operator=(FfmpegMp4Decoder&&) noexcept;

    void open();
    bool read(DecodedFrame& frame);
    void close() noexcept;

    bool isOpen() const noexcept;
    const VideoInfo& info() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}
