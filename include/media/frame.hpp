#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace media {

struct Rational {
    int numerator = 0;
    int denominator = 1;
};

enum class PixelFormat {
    Bgr24
};

struct FramePacket {
    std::string stream_id;
    uint64_t sequence = 0;
    int64_t pts = 0;
    Rational time_base;
    int width = 0;
    int height = 0;
    int stride = 0;
    PixelFormat pixel_format = PixelFormat::Bgr24;
    int64_t capture_time_ms = 0;
    int64_t monotonic_time_ms = 0;
    std::vector<uint8_t> image;
};

using DecodedFrame = FramePacket;

}
