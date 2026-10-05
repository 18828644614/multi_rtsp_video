#include "pipeline/frame_queue.hpp"

#include <chrono>
#include <stdexcept>

namespace pipeline {

FrameQueue::FrameQueue(FrameQueueOptions options)
    : options_(options) {
    if (options_.max_frames == 0) {
        throw std::invalid_argument("frame queue max_frames must be greater than zero");
    }
    if (options_.max_age_ms < 0) {
        throw std::invalid_argument("frame queue max_age_ms must not be negative");
    }
}

PushResult FrameQueue::push(media::FramePacket frame) {
    std::unique_lock<std::mutex> lock(mutex_);
    if (closed_) {
        return PushResult::Closed;
    }

    purgeExpiredLocked(monotonicNowMs());
    PushResult result = PushResult::Enqueued;

    while (frames_.size() >= options_.max_frames) {
        if (options_.drop_policy == DropPolicy::DropOldest) {
            frames_.pop_front();
            ++stats_.dropped_oldest;
            result = PushResult::DroppedOldest;
            break;
        }

        if (options_.drop_policy == DropPolicy::DropNewest) {
            ++stats_.dropped_newest;
            return PushResult::DroppedNewest;
        }

        if (options_.max_age_ms > 0) {
            not_full_.wait_for(
                lock,
                std::chrono::milliseconds(50),
                [this] { return closed_ || frames_.size() < options_.max_frames; });
            if (closed_) {
                return PushResult::Closed;
            }
            purgeExpiredLocked(monotonicNowMs());
        } else {
            not_full_.wait(lock, [this] { return closed_ || frames_.size() < options_.max_frames; });
            if (closed_) {
                return PushResult::Closed;
            }
        }
    }

    frames_.push_back(std::move(frame));
    ++stats_.pushed;
    if (frames_.size() > stats_.peak_size) {
        stats_.peak_size = frames_.size();
    }

    lock.unlock();
    not_empty_.notify_one();
    return result;
}

PopResult FrameQueue::waitPop(media::FramePacket& frame, int timeout_ms) {
    std::unique_lock<std::mutex> lock(mutex_);
    const auto ready = [this] { return closed_ || !frames_.empty(); };

    if (timeout_ms < 0) {
        not_empty_.wait(lock, ready);
    } else if (!not_empty_.wait_for(lock, std::chrono::milliseconds(timeout_ms), ready)) {
        return PopResult::Timeout;
    }

    purgeExpiredLocked(monotonicNowMs());
    if (frames_.empty()) {
        return closed_ ? PopResult::Closed : PopResult::Timeout;
    }

    frame = std::move(frames_.front());
    frames_.pop_front();
    ++stats_.popped;

    lock.unlock();
    not_full_.notify_one();
    return PopResult::Item;
}

void FrameQueue::close() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        closed_ = true;
    }
    not_empty_.notify_all();
    not_full_.notify_all();
}

void FrameQueue::abort() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        closed_ = true;
        frames_.clear();
    }
    not_empty_.notify_all();
    not_full_.notify_all();
}

std::size_t FrameQueue::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return frames_.size();
}

FrameQueueStats FrameQueue::stats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    FrameQueueStats result = stats_;
    result.current_size = frames_.size();
    return result;
}

int64_t FrameQueue::monotonicNowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

bool FrameQueue::isExpired(const media::FramePacket& frame, int64_t now_ms) const {
    if (options_.max_age_ms <= 0 || frame.monotonic_time_ms <= 0 || now_ms < frame.monotonic_time_ms) {
        return false;
    }
    return now_ms - frame.monotonic_time_ms > options_.max_age_ms;
}

void FrameQueue::purgeExpiredLocked(int64_t now_ms) {
    if (options_.max_age_ms <= 0) {
        return;
    }

    for (auto iterator = frames_.begin(); iterator != frames_.end();) {
        if (!isExpired(*iterator, now_ms)) {
            ++iterator;
            continue;
        }
        iterator = frames_.erase(iterator);
        ++stats_.expired;
    }
}

}



