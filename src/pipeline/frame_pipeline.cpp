#include "pipeline/frame_pipeline.hpp"

#include <exception>
#include <stdexcept>
#include <thread>
#include <utility>

namespace pipeline {

FramePipeline::FramePipeline(FrameQueueOptions options)
    : options_(options) {
}

FrameQueueStats FramePipeline::run(const Producer& producer, const Consumer& consumer) {
    if (!producer) {
        throw std::invalid_argument("frame pipeline producer must be set");
    }
    if (!consumer) {
        throw std::invalid_argument("frame pipeline consumer must be set");
    }

    FrameQueue queue(options_);
    std::exception_ptr producerError;
    std::exception_ptr consumerError;

    std::thread consumerThread([&] {
        try {
            media::FramePacket frame;
            while (true) {
                const PopResult result = queue.waitPop(frame, -1);
                if (result == PopResult::Closed) {
                    break;
                }
                if (result == PopResult::Item) {
                    consumer(frame);
                }
            }
        } catch (...) {
            consumerError = std::current_exception();
            queue.abort();
        }
    });

    std::thread producerThread;
    try {
        producerThread = std::thread([&] {
            try {
                media::FramePacket frame;
                while (producer(frame)) {
                    if (queue.push(std::move(frame)) == PushResult::Closed) {
                        break;
                    }
                }
                queue.close();
            } catch (...) {
                producerError = std::current_exception();
                queue.abort();
            }
        });
    } catch (...) {
        queue.abort();
        consumerThread.join();
        throw;
    }

    producerThread.join();
    consumerThread.join();

    if (producerError) {
        std::rethrow_exception(producerError);
    }
    if (consumerError) {
        std::rethrow_exception(consumerError);
    }

    return queue.stats();
}

}