#pragma once

#include <optional>
#include <string>
#include <vector>

#include "ar/fusion/fusion_types.hpp"
#include "vision/gesture_engine/gesture_types.hpp"
#include "vision/landmarks/landmark_types.hpp"

namespace arx::replay {

struct SessionFrame {
    std::int64_t timestamp_us{0};
    std::uint64_t frame_id{0};
    double fps{0.0};
    double latency_ms{0.0};
    std::uint64_t dropped_frames{0};
    std::vector<vision::HandLandmarks> hands;
    std::optional<vision::FaceLandmarks> face;
    vision::GestureResult gesture;
    std::string profiler_snapshot;
    std::optional<ar::fusion::ARFusionFrame> fusion;
};

}  // namespace arx::replay
