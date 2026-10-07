#pragma once

#include "app/filesystem.hpp"
#include "media/frame_source.hpp"

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

class FfmpegMp4Decoder final : public FrameSource {
public:
    explicit FfmpegMp4Decoder(app_fs::path path, std::string streamId = "mp4");
    ~FfmpegMp4Decoder() override;

    FfmpegMp4Decoder(const FfmpegMp4Decoder&) = delete;
    FfmpegMp4Decoder& operator=(const FfmpegMp4Decoder&) = delete;
    FfmpegMp4Decoder(FfmpegMp4Decoder&&) noexcept;
    FfmpegMp4Decoder& operator=(FfmpegMp4Decoder&&) noexcept;

    void open() override;
    FrameReadResult read(FramePacket& frame) override;
    void close() noexcept override;

    bool isOpen() const noexcept override;
    const VideoInfo& info() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}