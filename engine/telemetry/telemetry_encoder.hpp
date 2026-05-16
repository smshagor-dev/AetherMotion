#pragma once

#include <cstdint>
#include <deque>
#include <optional>
#include <string>

#include "engine/events/gesture_events.hpp"
#include "vision/landmarks/landmark_types.hpp"

namespace arx::engine::telemetry {

struct RuntimeTelemetryFrame {
    vision::FrameMetadata frame_meta;
    std::size_t num_hands{0};
    std::string gesture;
    float confidence{0.0f};
    double latency_ms{0.0};
    double camera_latency_ms{0.0};
    double inference_latency_ms{0.0};
    double smoothing_latency_ms{0.0};
    double gesture_latency_ms{0.0};
    double render_latency_ms{0.0};
    std::size_t queue_depth{0};
    std::size_t queue_capacity{0};
    bool model_loaded{false};
    std::size_t raw_hand_count{0};
    float top_hand_confidence{0.0f};
    std::string tracker_state;
    std::string tracker_error;
    std::string hand_model_path;
    std::string face_model_path;
    std::uint64_t dropped_frames{0};
    bool recording{false};
    bool replaying{false};
    std::string profiler_snapshot;
};

class TelemetryEncoder {
public:
    std::string encode_frame(const RuntimeTelemetryFrame& frame) const;
    std::string encode_gesture_event(const engine::GestureRuntimeEvent& event) const;

    void push_timeline(std::string entry);
    [[nodiscard]] const std::deque<std::string>& timeline() const noexcept;

private:
    std::deque<std::string> timeline_;
};

}  // namespace arx::engine::telemetry
