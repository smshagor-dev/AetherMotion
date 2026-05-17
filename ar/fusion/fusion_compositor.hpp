#pragma once

#include "ar/fusion/fusion_types.hpp"
#include "engine/config/runtime_config.hpp"
#include "vision/gesture_engine/gesture_engine.hpp"
#include "vision/landmarks/landmark_provider.hpp"

namespace arx::ar::fusion {

class FusionCompositor {
public:
    explicit FusionCompositor(engine::config::FusionConfig config = {});

    void configure(engine::config::FusionConfig config);
    ARFusionFrame compose(const vision::landmarks::LandmarkProviderOutput& raw,
                          const vision::GestureEngine::Output& gesture_output,
                          const std::optional<std::string>& gesture_event_name,
                          double camera_latency_ms,
                          double inference_latency_ms,
                          double smoothing_latency_ms,
                          double gesture_latency_ms,
                          double render_latency_ms,
                          std::uint64_t dropped_frames);
    void reset();

private:
    static float pinch_distance(const vision::HandLandmarks& hand);
    static vision::Point3f midpoint(const vision::Point3f& a, const vision::Point3f& b);

    engine::config::FusionConfig config_{};
    float tracking_fade_{0.0f};
    bool anchor_initialized_{false};
    vision::Point3f smoothed_cursor_{};
};

}  // namespace arx::ar::fusion
