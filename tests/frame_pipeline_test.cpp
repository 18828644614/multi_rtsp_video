#include "pipeline/frame_pipeline.hpp"

#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

media::FramePacket makeFrame(uint64_t sequence) {
    media::FramePacket frame;
    frame.metadata.stream_id = "test";
    frame.metadata.sequence = sequence;
    frame.metadata.width = 1;
    frame.metadata.height = 1;
    frame.stride = 3;
    frame.image = {1, 2, 3};
    return frame;
}

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void testProducerConsumerFlow() {
    pipeline::FramePipeline framePipeline({2, 0, pipeline::DropPolicy::Block});
    std::size_t nextSequence = 0;
    std::vector<uint64_t> consumedSequences;
    std::thread::id producerThreadId;
    std::thread::id consumerThreadId;

    const pipeline::FrameQueueStats stats = framePipeline.run(
        [&](media::FramePacket& frame) {
            producerThreadId = std::this_thread::get_id();
            if (nextSequence == 24) {
                return false;
            }
            frame = makeFrame(nextSequence++);
            return true;
        },
        [&](const media::FramePacket& frame) {
            consumerThreadId = std::this_thread::get_id();
            consumedSequences.push_back(frame.metadata.sequence);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        });

    require(consumedSequences.size() == 24, "consumer did not receive every produced frame");
    for (std::size_t index = 0; index < consumedSequences.size(); ++index) {
        require(consumedSequences[index] == index, "consumer received frames out of order");
    }
    require(stats.pushed == 24 && stats.popped == 24, "pipeline queue counts are invalid");
    require(stats.peak_size <= 2, "pipeline queue exceeded its configured capacity");
    require(producerThreadId != consumerThreadId, "producer and consumer ran on the same thread");
}

void testProducerExceptionIsReported() {
    pipeline::FramePipeline framePipeline({1, 0, pipeline::DropPolicy::Block});
    bool exceptionReceived = false;

    try {
        framePipeline.run(
            [](media::FramePacket&) -> bool {
                throw std::runtime_error("producer failed");
            },
            [](const media::FramePacket&) {});
    } catch (const std::runtime_error& error) {
        exceptionReceived = std::string(error.what()) == "producer failed";
    }

    require(exceptionReceived, "producer exception was not returned to the caller");
}

void testConsumerExceptionIsReported() {
    pipeline::FramePipeline framePipeline({1, 0, pipeline::DropPolicy::Block});
    std::size_t nextSequence = 0;
    bool exceptionReceived = false;

    try {
        framePipeline.run(
            [&](media::FramePacket& frame) {
                frame = makeFrame(nextSequence++);
                return true;
            },
            [](const media::FramePacket&) {
                throw std::runtime_error("consumer failed");
            });
    } catch (const std::runtime_error& error) {
        exceptionReceived = std::string(error.what()) == "consumer failed";
    }

    require(exceptionReceived, "consumer exception was not returned to the caller");
}

}

int main() {
    try {
        testProducerConsumerFlow();
        testProducerExceptionIsReported();
        testConsumerExceptionIsReported();
        std::cout << "frame_pipeline_test passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "frame_pipeline_test failed: " << error.what() << "\n";
        return 1;
    }
}