#pragma once

#include "media/frame.hpp"

#include <cstddef>
#include <cstdint>
#include <condition_variable>
#include <deque>
#include <mutex>

namespace pipeline {

enum class DropPolicy {
    DropOldest,
    DropNewest,
    Block
};

enum class PushResult {
    Enqueued,
    DroppedOldest,
    DroppedNewest,
    Closed
};

enum class PopResult {
    Item,
    Timeout,
    Closed
};

struct FrameQueueOptions {
    std::size_t max_frames = 1;
    int64_t max_age_ms = 0;
    DropPolicy drop_policy = DropPolicy::DropOldest;
};

struct FrameQueueStats {
    std::size_t current_size = 0;
    std::size_t peak_size = 0;
    uint64_t pushed = 0;
    uint64_t popped = 0;
    uint64_t dropped_oldest = 0;
    uint64_t dropped_newest = 0;
    uint64_t expired = 0;
};

class FrameQueue {
public:
    explicit FrameQueue(FrameQueueOptions options);

    PushResult push(media::FramePacket frame);
    PopResult waitPop(media::FramePacket& frame, int timeout_ms);

    void close();
    void abort();

    std::size_t size() const;
    FrameQueueStats stats() const;

private:
    static int64_t monotonicNowMs();

    bool isExpired(const media::FramePacket& frame, int64_t now_ms) const;
    void purgeExpiredLocked(int64_t now_ms);

    FrameQueueOptions options_;
    mutable std::mutex mutex_;
    std::condition_variable not_empty_;
    std::condition_variable not_full_;
    std::deque<media::FramePacket> frames_;
    FrameQueueStats stats_;
    bool closed_ = false;
};

}



