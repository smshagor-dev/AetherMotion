#pragma once

#include <cstdint>

#include "vision/gesture_engine/gesture_types.hpp"

namespace arx::engine {

struct GestureRuntimeEvent {
    vision::GestureEventKind kind{vision::GestureEventKind::kStarted};
    vision::GestureType gesture{vision::GestureType::kNone};
    float confidence{0.0f};
    double duration_seconds{0.0};
    std::int64_t timestamp_us{0};
};

}  // namespace arx::engine
