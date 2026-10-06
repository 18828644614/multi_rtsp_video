#pragma once

#include "vision/detection.hpp"

#include <iosfwd>

namespace output {

class DetectionJsonlSink {
public:
    explicit DetectionJsonlSink(std::ostream& output);

    void write(const vision::DetectionResult& result);

private:
    std::ostream& output_;
};

}
