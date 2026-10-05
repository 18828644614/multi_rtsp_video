#include <cstdint>
#include <iostream>
#include <limits>

#include "media/ffmpeg_mp4_decoder.hpp"

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "mp4_decoder_test requires an MP4 path\n";
        return 2;
    }

    try {
        media::FfmpegMp4Decoder decoder(argv[1], "test-mp4");
        decoder.open();

        const media::VideoInfo& info = decoder.info();
        if (!decoder.isOpen() || info.width <= 0 || info.height <= 0 || info.codec_name.empty()) {
            std::cerr << "decoder metadata is incomplete\n";
            return 1;
        }

        media::FramePacket frame;
        std::size_t decodedFrames = 0;
        int64_t previousPts = std::numeric_limits<int64_t>::min();
        while (decoder.read(frame)) {
            if (frame.stream_id != "test-mp4" || frame.sequence != decodedFrames || frame.width != info.width ||
                frame.height != info.height || frame.stride != frame.width * 3 ||
                frame.image.size() != static_cast<std::size_t>(frame.stride) * frame.height) {
                std::cerr << "decoded frame metadata is invalid\n";
                return 1;
            }
            if (frame.pts != std::numeric_limits<int64_t>::min() && previousPts != std::numeric_limits<int64_t>::min() &&
                frame.pts < previousPts) {
                std::cerr << "frame PTS is not monotonic\n";
                return 1;
            }
            previousPts = frame.pts;
            ++decodedFrames;
        }

        if (decodedFrames == 0) {
            std::cerr << "MP4 produced no decoded frames\n";
            return 1;
        }

        decoder.close();
        std::cout << "mp4_decoder_test passed: " << decodedFrames << " frames\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "mp4_decoder_test failed: " << error.what() << "\n";
        return 1;
    }
}