#include "media/ffmpeg_mp4_decoder.hpp"

#include <chrono>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <utility>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/error.h>
#include <libavutil/frame.h>
#include <libavutil/imgutils.h>
#include <libavutil/pixfmt.h>
#include <libswscale/swscale.h>
}

namespace media {
namespace {

std::string ffmpegError(const std::string& operation, int errorCode) {
    char errorBuffer[AV_ERROR_MAX_STRING_SIZE] = {};
    av_strerror(errorCode, errorBuffer, sizeof(errorBuffer));
    return operation + " failed: " + errorBuffer;
}

[[noreturn]] void fail(const std::string& operation, int errorCode) {
    throw std::runtime_error(ffmpegError(operation, errorCode));
}

int64_t wallClockMilliseconds() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

int64_t monotonicClockMilliseconds() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

Rational toRational(AVRational value) {
    return Rational{value.num, value.den};
}

double rationalToDouble(AVRational value) {
    return value.den == 0 ? 0.0 : av_q2d(value);
}

}

struct FfmpegMp4Decoder::Impl {
    explicit Impl(app_fs::path pathValue, std::string streamIdValue)
        : path(std::move(pathValue)), stream_id(std::move(streamIdValue)) {
        info.path = path;
        info.stream_id = stream_id;
    }

    void release() noexcept {
        sws_freeContext(scaler);
        scaler = nullptr;
        av_frame_free(&frame);
        av_packet_free(&packet);
        avcodec_free_context(&codec_context);
        avformat_close_input(&format_context);
        video_stream = nullptr;
        codec = nullptr;
        input_eof = false;
        flush_sent = false;
        next_sequence = 0;
        source_width = 0;
        source_height = 0;
        source_pixel_format = AV_PIX_FMT_NONE;
        opened = false;
    }

    void ensureScaler(const AVFrame* decodedFrame) {
        const AVPixelFormat pixelFormat = static_cast<AVPixelFormat>(decodedFrame->format);
        if (scaler != nullptr && source_width == decodedFrame->width && source_height == decodedFrame->height &&
            source_pixel_format == pixelFormat) {
            return;
        }

        sws_freeContext(scaler);
        scaler = sws_getContext(
            decodedFrame->width,
            decodedFrame->height,
            pixelFormat,
            decodedFrame->width,
            decodedFrame->height,
            AV_PIX_FMT_BGR24,
            SWS_BILINEAR,
            nullptr,
            nullptr,
            nullptr);
        if (scaler == nullptr) {
            throw std::runtime_error("sws_getContext failed");
        }

        source_width = decodedFrame->width;
        source_height = decodedFrame->height;
        source_pixel_format = pixelFormat;
    }

    app_fs::path path;
    std::string stream_id;
    VideoInfo info;
    AVFormatContext* format_context = nullptr;
    AVCodecContext* codec_context = nullptr;
    AVStream* video_stream = nullptr;
    const AVCodec* codec = nullptr;
    AVPacket* packet = nullptr;
    AVFrame* frame = nullptr;
    SwsContext* scaler = nullptr;
    AVPixelFormat source_pixel_format = AV_PIX_FMT_NONE;
    int source_width = 0;
    int source_height = 0;
    int video_stream_index = -1;
    bool input_eof = false;
    bool flush_sent = false;
    bool opened = false;
    uint64_t next_sequence = 0;
};

FfmpegMp4Decoder::FfmpegMp4Decoder(app_fs::path path, std::string streamId)
    : impl_(new Impl(std::move(path), std::move(streamId))) {
    if (impl_->path.empty()) {
        throw std::invalid_argument("MP4 path must not be empty");
    }
    if (impl_->stream_id.empty()) {
        throw std::invalid_argument("stream id must not be empty");
    }
}

FfmpegMp4Decoder::~FfmpegMp4Decoder() {
    close();
}

FfmpegMp4Decoder::FfmpegMp4Decoder(FfmpegMp4Decoder&& other) noexcept = default;

FfmpegMp4Decoder& FfmpegMp4Decoder::operator=(FfmpegMp4Decoder&& other) noexcept = default;

void FfmpegMp4Decoder::open() {
    close();

    const std::string inputPath = impl_->path.string();
    int result = avformat_open_input(&impl_->format_context, inputPath.c_str(), nullptr, nullptr);
    if (result < 0) {
        fail("avformat_open_input(" + inputPath + ")", result);
    }

    try {
        result = avformat_find_stream_info(impl_->format_context, nullptr);
        if (result < 0) {
            fail("avformat_find_stream_info", result);
        }

        result = av_find_best_stream(impl_->format_context, AVMEDIA_TYPE_VIDEO, -1, -1, &impl_->codec, 0);
        if (result < 0) {
            fail("av_find_best_stream", result);
        }
        impl_->video_stream_index = result;
        impl_->video_stream = impl_->format_context->streams[impl_->video_stream_index];

        if (impl_->codec == nullptr) {
            throw std::runtime_error("video decoder was not found");
        }

        impl_->codec_context = avcodec_alloc_context3(impl_->codec);
        if (impl_->codec_context == nullptr) {
            throw std::runtime_error("avcodec_alloc_context3 failed");
        }

        result = avcodec_parameters_to_context(
            impl_->codec_context,
            impl_->video_stream->codecpar);
        if (result < 0) {
            fail("avcodec_parameters_to_context", result);
        }

        result = avcodec_open2(impl_->codec_context, impl_->codec, nullptr);
        if (result < 0) {
            fail("avcodec_open2", result);
        }

        impl_->packet = av_packet_alloc();
        impl_->frame = av_frame_alloc();
        if (impl_->packet == nullptr || impl_->frame == nullptr) {
            throw std::runtime_error("FFmpeg packet/frame allocation failed");
        }

        impl_->info.codec_name = impl_->codec->name == nullptr ? "" : impl_->codec->name;
        impl_->info.width = impl_->codec_context->width;
        impl_->info.height = impl_->codec_context->height;
        impl_->info.time_base = toRational(impl_->video_stream->time_base);
        impl_->info.duration_pts = impl_->video_stream->duration;
        impl_->info.duration_seconds = impl_->video_stream->duration == AV_NOPTS_VALUE
                                           ? 0.0
                                           : impl_->video_stream->duration * rationalToDouble(impl_->video_stream->time_base);
        const AVRational frameRate = impl_->video_stream->avg_frame_rate.num != 0
                                         ? impl_->video_stream->avg_frame_rate
                                         : impl_->video_stream->r_frame_rate;
        impl_->info.average_frame_rate = rationalToDouble(frameRate);
        impl_->opened = true;
    } catch (...) {
        impl_->release();
        throw;
    }
}

bool FfmpegMp4Decoder::read(FramePacket& output) {
    if (!impl_->opened) {
        throw std::logic_error("MP4 decoder is not open");
    }

    while (true) {
        int result = avcodec_receive_frame(impl_->codec_context, impl_->frame);
        if (result == 0) {
            const int width = impl_->frame->width;
            const int height = impl_->frame->height;
            if (width <= 0 || height <= 0 || width > std::numeric_limits<int>::max() / 3) {
                throw std::runtime_error("decoded frame has an invalid size");
            }

            impl_->ensureScaler(impl_->frame);
            const std::size_t stride = static_cast<std::size_t>(width) * 3;
            const std::size_t imageSize = stride * static_cast<std::size_t>(height);
            output.image.resize(imageSize);

            uint8_t* destinationData[4] = {output.image.data(), nullptr, nullptr, nullptr};
            int destinationLinesize[4] = {static_cast<int>(stride), 0, 0, 0};
            const int scaledHeight = sws_scale(
                impl_->scaler,
                impl_->frame->data,
                impl_->frame->linesize,
                0,
                height,
                destinationData,
                destinationLinesize);
            if (scaledHeight != height) {
                throw std::runtime_error("sws_scale returned an incomplete frame");
            }

            output.stream_id = impl_->stream_id;
            output.sequence = impl_->next_sequence++;
            output.pts = impl_->frame->best_effort_timestamp != AV_NOPTS_VALUE
                             ? impl_->frame->best_effort_timestamp
                             : impl_->frame->pts;
            output.time_base = impl_->info.time_base;
            output.width = width;
            output.height = height;
            output.stride = static_cast<int>(stride);
            output.pixel_format = PixelFormat::Bgr24;
            output.capture_time_ms = wallClockMilliseconds();
            output.monotonic_time_ms = monotonicClockMilliseconds();
            av_frame_unref(impl_->frame);
            return true;
        }

        if (result == AVERROR_EOF) {
            return false;
        }
        if (result != AVERROR(EAGAIN)) {
            fail("avcodec_receive_frame", result);
        }

        if (impl_->input_eof) {
            if (!impl_->flush_sent) {
                result = avcodec_send_packet(impl_->codec_context, nullptr);
                if (result < 0 && result != AVERROR_EOF) {
                    fail("avcodec_send_packet(flush)", result);
                }
                impl_->flush_sent = true;
                continue;
            }
            return false;
        }

        result = av_read_frame(impl_->format_context, impl_->packet);
        if (result == AVERROR_EOF) {
            impl_->input_eof = true;
            continue;
        }
        if (result < 0) {
            fail("av_read_frame", result);
        }

        if (impl_->packet->stream_index == impl_->video_stream_index) {
            result = avcodec_send_packet(impl_->codec_context, impl_->packet);
            av_packet_unref(impl_->packet);
            if (result < 0) {
                fail("avcodec_send_packet", result);
            }
        } else {
            av_packet_unref(impl_->packet);
        }
    }
}

void FfmpegMp4Decoder::close() noexcept {
    if (impl_ != nullptr) {
        impl_->release();
    }
}

bool FfmpegMp4Decoder::isOpen() const noexcept {
    return impl_ != nullptr && impl_->opened;
}

const VideoInfo& FfmpegMp4Decoder::info() const {
    if (impl_ == nullptr) {
        throw std::logic_error("MP4 decoder has no implementation");
    }
    return impl_->info;
}

}