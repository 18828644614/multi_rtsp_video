#include "pipeline/frame_queue.hpp"

#include <atomic>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace {

int64_t monotonicNowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

media::FramePacket makeFrame(uint64_t sequence, int64_t monotonicTimeMs = 0) {
    media::FramePacket frame;
    frame.metadata.stream_id = "test";
    frame.metadata.sequence = sequence;
    frame.metadata.width = 1;
    frame.metadata.height = 1;
    frame.stride = 3;
    frame.image = {static_cast<uint8_t>(sequence), 2, 3};
    frame.metadata.received_at_steady_ms = monotonicTimeMs;
    return frame;
}

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void testDropOldest() {
    pipeline::FrameQueue queue({3, 0, pipeline::DropPolicy::DropOldest});
    require(queue.push(makeFrame(1)) == pipeline::PushResult::Enqueued, "first frame was not enqueued");
    require(queue.push(makeFrame(2)) == pipeline::PushResult::Enqueued, "second frame was not enqueued");
    require(queue.push(makeFrame(3)) == pipeline::PushResult::Enqueued, "third frame was not enqueued");
    require(queue.push(makeFrame(4)) == pipeline::PushResult::DroppedOldest, "oldest frame was not dropped");

    media::FramePacket frame;
    require(queue.waitPop(frame, 0) == pipeline::PopResult::Item && frame.metadata.sequence == 2, "unexpected first FIFO frame");
    require(queue.waitPop(frame, 0) == pipeline::PopResult::Item && frame.metadata.sequence == 3, "unexpected second FIFO frame");
    require(queue.waitPop(frame, 0) == pipeline::PopResult::Item && frame.metadata.sequence == 4, "unexpected newest frame");

    const auto stats = queue.stats();
    require(stats.dropped_oldest == 1 && stats.pushed == 4 && stats.popped == 3, "drop_oldest statistics are invalid");
}

void testDropNewest() {
    pipeline::FrameQueue queue({2, 0, pipeline::DropPolicy::DropNewest});
    queue.push(makeFrame(1));
    queue.push(makeFrame(2));
    require(queue.push(makeFrame(3)) == pipeline::PushResult::DroppedNewest, "newest frame was not dropped");

    media::FramePacket frame;
    require(queue.waitPop(frame, 0) == pipeline::PopResult::Item && frame.metadata.sequence == 1, "drop_newest changed first frame");
    require(queue.waitPop(frame, 0) == pipeline::PopResult::Item && frame.metadata.sequence == 2, "drop_newest changed second frame");
    require(queue.stats().dropped_newest == 1, "drop_newest statistics are invalid");
}

void testCloseAndDrain() {
    pipeline::FrameQueue queue({2, 0, pipeline::DropPolicy::DropOldest});
    queue.push(makeFrame(7));
    queue.close();

    media::FramePacket frame;
    require(queue.push(makeFrame(8)) == pipeline::PushResult::Closed, "closed queue accepted a frame");
    require(queue.waitPop(frame, 0) == pipeline::PopResult::Item && frame.metadata.sequence == 7, "closed queue did not drain existing frame");
    require(queue.waitPop(frame, 0) == pipeline::PopResult::Closed, "closed and empty queue did not report closed");
    pipeline::FrameQueue waitingQueue({1, 0, pipeline::DropPolicy::DropOldest});
    pipeline::PopResult waitingResult = pipeline::PopResult::Item;
    std::thread waiter([&] {
        media::FramePacket waitingFrame;
        waitingResult = waitingQueue.waitPop(waitingFrame, -1);
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    waitingQueue.close();
    waiter.join();
    require(waitingResult == pipeline::PopResult::Closed, "close did not wake blocked consumer");
}

void testAbort() {
    pipeline::FrameQueue queue({2, 0, pipeline::DropPolicy::DropOldest});
    queue.push(makeFrame(1));
    queue.abort();

    media::FramePacket frame;
    require(queue.size() == 0, "aborted queue still contains frames");
    require(queue.waitPop(frame, 0) == pipeline::PopResult::Closed, "aborted queue did not report closed");
}

void testTimeoutAndExpiration() {
    pipeline::FrameQueue timeoutQueue({1, 0, pipeline::DropPolicy::DropOldest});
    media::FramePacket frame;
    require(timeoutQueue.waitPop(frame, 0) == pipeline::PopResult::Timeout, "empty queue did not report timeout");

    pipeline::FrameQueue ageQueue({2, 10, pipeline::DropPolicy::DropOldest});
    ageQueue.push(makeFrame(1, monotonicNowMs() - 100));
    ageQueue.push(makeFrame(2, monotonicNowMs()));
    require(ageQueue.waitPop(frame, 0) == pipeline::PopResult::Item && frame.metadata.sequence == 2, "expired frame was not removed");
    require(ageQueue.stats().expired == 1, "expiration statistics are invalid");
}

void testBlockAndWake() {
    pipeline::FrameQueue queue({1, 0, pipeline::DropPolicy::Block});
    queue.push(makeFrame(1));

    std::atomic<bool> producerFinished{false};
    pipeline::PushResult producerResult = pipeline::PushResult::Closed;
    std::thread producer([&] {
        producerResult = queue.push(makeFrame(2));
        producerFinished.store(true);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    require(!producerFinished.load(), "block policy did not block the producer");

    media::FramePacket frame;
    require(queue.waitPop(frame, 0) == pipeline::PopResult::Item && frame.metadata.sequence == 1, "block queue did not release first frame");
    producer.join();
    require(producerFinished.load() && producerResult == pipeline::PushResult::Enqueued, "blocked producer did not resume");
    require(queue.waitPop(frame, 0) == pipeline::PopResult::Item && frame.metadata.sequence == 2, "blocked frame was not queued");

    queue.push(makeFrame(3));
    std::atomic<bool> closingProducerFinished{false};
    pipeline::PushResult closingProducerResult = pipeline::PushResult::Enqueued;
    std::thread closingProducer([&] {
        closingProducerResult = queue.push(makeFrame(4));
        closingProducerFinished.store(true);
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    queue.close();
    closingProducer.join();
    require(closingProducerFinished.load() && closingProducerResult == pipeline::PushResult::Closed, "close did not wake blocked producer");
}

}

int main() {
    try {
        testDropOldest();
        testDropNewest();
        testCloseAndDrain();
        testAbort();
        testTimeoutAndExpiration();
        testBlockAndWake();
        std::cout << "frame_queue_test passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "frame_queue_test failed: " << error.what() << "\n";
        return 1;
    }
}