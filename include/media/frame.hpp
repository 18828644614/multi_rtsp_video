#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace media {

struct Rational {
    int numerator = 0;
    int denominator = 1;
};

struct FrameMetadata {
    std::string stream_id;
    std::uint64_t sequence = 0;
    std::uint64_t source_epoch = 0;
    std::optional<std::int64_t> pts;
    Rational time_base;
    std::int64_t received_at_unix_ms = 0;
    std::optional<std::int64_t> received_at_steady_ms;
    int width = 0;
    int height = 0;
};

enum class PixelFormat {
    Bgr24
};

struct FramePacket {
    FrameMetadata metadata;
    int stride = 0;
    PixelFormat pixel_format = PixelFormat::Bgr24;
    std::vector<std::uint8_t> image;
};

using DecodedFrame = FramePacket;

}
