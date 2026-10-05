#pragma once

#include "app/filesystem.hpp"
#include "media/frame.hpp"

#include <memory>
#include <string>

namespace media {

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

class FfmpegMp4Decoder {
public:
    explicit FfmpegMp4Decoder(app_fs::path path, std::string streamId = "mp4");
    ~FfmpegMp4Decoder();

    FfmpegMp4Decoder(const FfmpegMp4Decoder&) = delete;
    FfmpegMp4Decoder& operator=(const FfmpegMp4Decoder&) = delete;
    FfmpegMp4Decoder(FfmpegMp4Decoder&&) noexcept;
    FfmpegMp4Decoder& operator=(FfmpegMp4Decoder&&) noexcept;

    void open();
    bool read(FramePacket& frame);
    void close() noexcept;

    bool isOpen() const noexcept;
    const VideoInfo& info() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}