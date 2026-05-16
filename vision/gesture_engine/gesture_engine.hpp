#pragma once

#include <deque>
#include <optional>

#include "engine/events/event_bus.hpp"
#include "engine/events/gesture_events.hpp"
#include "vision/gesture_engine/gesture_classifier.hpp"
#include "vision/smoothing/landmark_smoother.hpp"
#include "vision/smoothing/one_euro_filter.hpp"

namespace arx::vision {

class GestureEngine {
public:
    struct Config {
        float min_confidence{0.55f};
        bool disable_debounce{false};
    };

    struct Output {
        FrameLandmarks smoothed;
        GestureResult gesture;
        std::optional<engine::GestureRuntimeEvent> event;
        VelocityFrame velocity;
    };

    explicit GestureEngine(engine::EventBus* event_bus = nullptr);

    void configure(Config config);
    Output process(const FrameLandmarks& frame, double timestamp_seconds);
    void reset();

private:
    std::optional<engine::GestureRuntimeEvent> update_state_machine(const GestureResult& gesture, double timestamp_seconds);

    LandmarkSmoother left_right_smoother_;
    VelocityEstimator velocity_estimator_;
    GestureClassifier classifier_;
    engine::EventBus* event_bus_{nullptr};
    Config config_{};
    GestureType current_gesture_{GestureType::kNone};
    double start_ts_{0.0};
    double last_seen_ts_{0.0};
    bool held_{false};
};

}  // namespace arx::vision
