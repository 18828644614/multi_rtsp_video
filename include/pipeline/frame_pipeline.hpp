#pragma once

#include "pipeline/frame_queue.hpp"

#include <functional>

namespace pipeline {

class FramePipeline {
public:
    using Producer = std::function<bool(media::FramePacket&)>;
    using Consumer = std::function<void(const media::FramePacket&)>;

    explicit FramePipeline(FrameQueueOptions options);

    FrameQueueStats run(const Producer& producer, const Consumer& consumer);

private:
    FrameQueueOptions options_;
};

}