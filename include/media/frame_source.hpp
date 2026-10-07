#pragma once

#include "media/frame.hpp"

#include <string>

namespace media {

enum class FrameReadStatus {
    Frame,
    EndOfStream,
    RetryableError,
    FatalError,
    Interrupted
};

struct FrameReadResult {
    FrameReadStatus status = FrameReadStatus::EndOfStream;
    std::string message;
};

class FrameSource {
public:
    virtual ~FrameSource() = default;

    virtual void open() = 0;
    virtual FrameReadResult read(FramePacket& frame) = 0;
    virtual void close() noexcept = 0;
    virtual bool isOpen() const noexcept = 0;
};

}
