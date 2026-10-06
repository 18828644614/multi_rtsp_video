#include "output/detection_jsonl_sink.hpp"

#include <cmath>
#include <cstddef>
#include <iomanip>
#include <limits>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {

void writeJsonString(std::ostream& output, const std::string& value) {
    output.put('"');
    for (const unsigned char character : value) {
        switch (character) {
        case '"':
            output << "\\\"";
            break;
        case '\\':
            output << "\\\\";
            break;
        case '\b':
            output << "\\b";
            break;
        case '\f':
            output << "\\f";
            break;
        case '\n':
            output << "\\n";
            break;
        case '\r':
            output << "\\r";
            break;
        case '\t':
            output << "\\t";
            break;
        default:
            if (character < 0x20) {
                output << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                       << static_cast<int>(character) << std::dec << std::setfill(' ');
            } else {
                output.put(static_cast<char>(character));
            }
            break;
        }
    }
    output.put('"');
}

std::string formatJsonNumber(double value, const std::string& field) {
    if (!std::isfinite(value)) {
        throw std::invalid_argument(field + " must be finite");
    }

    std::ostringstream number;
    number << std::setprecision(std::numeric_limits<double>::max_digits10) << value;
    return number.str();
}

void validateResult(const vision::DetectionResult& result) {
    if (result.width < 0 || result.height < 0) {
        throw std::invalid_argument("detection result dimensions must not be negative");
    }

    for (const auto& detection : result.detections) {
        if (!std::isfinite(detection.confidence) ||
            detection.confidence < 0.0 || detection.confidence > 1.0) {
            throw std::invalid_argument("detection confidence must be in [0, 1]");
        }

        formatJsonNumber(detection.bbox.x, "bbox.x");
        formatJsonNumber(detection.bbox.y, "bbox.y");
        formatJsonNumber(detection.bbox.width, "bbox.width");
        formatJsonNumber(detection.bbox.height, "bbox.height");

        if (!detection.bbox.isWithinFrame(result.width, result.height)) {
            throw std::invalid_argument("detection bounding box is outside the frame");
        }
    }
}

}

namespace output {

DetectionJsonlSink::DetectionJsonlSink(std::ostream& output)
    : output_(output) {
}

void DetectionJsonlSink::write(const vision::DetectionResult& result) {
    validateResult(result);

    std::ostringstream line;
    line << "{\"schema_version\":1"
         << ",\"type\":\"detection\""
         << ",\"stream_id\":";
    writeJsonString(line, result.stream_id);
    line << ",\"sequence\":" << result.sequence
         << ",\"timestamp_ms\":" << result.capture_time_ms
         << ",\"width\":" << result.width
         << ",\"height\":" << result.height
         << ",\"objects\":[";

    for (std::size_t index = 0; index < result.detections.size(); ++index) {
        if (index != 0) {
            line.put(',');
        }

        const auto& detection = result.detections[index];
        line << "{\"class_id\":" << detection.class_id << ",\"label\":";
        writeJsonString(line, detection.label);
        line << ",\"confidence\":"
             << formatJsonNumber(detection.confidence, "confidence")
             << ",\"bbox\":{\"x\":"
             << formatJsonNumber(detection.bbox.x, "bbox.x")
             << ",\"y\":" << formatJsonNumber(detection.bbox.y, "bbox.y")
             << ",\"width\":" << formatJsonNumber(detection.bbox.width, "bbox.width")
             << ",\"height\":" << formatJsonNumber(detection.bbox.height, "bbox.height")
             << "}}";
    }

    line << "]}";
    output_ << line.str() << '\n';
    if (!output_) {
        throw std::runtime_error("failed to write detection JSONL");
    }
}

}
