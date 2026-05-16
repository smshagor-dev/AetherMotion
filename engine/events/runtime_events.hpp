#pragma once

#include <cstdint>
#include <string>

namespace arx::engine {

struct RuntimeStatusEvent {
    std::string status;
    std::string detail;
    std::int64_t timestamp_us{0};
};

struct LatencySnapshotEvent {
    double frame_ms{0.0};
    double gesture_ms{0.0};
    double total_ms{0.0};
    std::uint64_t frame_id{0};
};

}  // namespace arx::engine
