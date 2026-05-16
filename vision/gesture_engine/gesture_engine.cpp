#include "vision/gesture_engine/gesture_engine.hpp"

namespace arx::vision {

namespace {
constexpr double kHoldThresholdSec = 0.4;
constexpr double kReleaseTimeoutSec = 0.25;
}

GestureEngine::GestureEngine(engine::EventBus* event_bus)
    : left_right_smoother_(kHandLandmarkCount, 3, 30.0, 1.5, 0.01),
      velocity_estimator_(8),
      event_bus_(event_bus) {}

void GestureEngine::configure(Config config) {
    config_ = config;
}

GestureEngine::Output GestureEngine::process(const FrameLandmarks& frame, double timestamp_seconds) {
    Output out;
    out.smoothed = left_right_smoother_.smooth(frame, timestamp_seconds);

    if (!out.smoothed.hands.empty()) {
        const auto& tip = out.smoothed.hands.front().points[8];
        out.velocity = velocity_estimator_.update(tip.x, tip.y, timestamp_seconds);
        out.gesture = classifier_.classify(out.smoothed, out.velocity);
    } else {
        out.gesture = classifier_.classify(out.smoothed, std::nullopt);
    }

    out.event = update_state_machine(out.gesture, timestamp_seconds);
    return out;
}

void GestureEngine::reset() {
    left_right_smoother_.reset();
    velocity_estimator_.reset();
    current_gesture_ = GestureType::kNone;
    start_ts_ = 0.0;
    last_seen_ts_ = 0.0;
    held_ = false;
}

std::optional<engine::GestureRuntimeEvent> GestureEngine::update_state_machine(const GestureResult& gesture, double timestamp_seconds) {
    const bool valid = gesture.gesture != GestureType::kNone && gesture.confidence >= config_.min_confidence;
    std::optional<engine::GestureRuntimeEvent> event;

    auto emit = [&](GestureEventKind kind, GestureType type, float confidence, double duration) {
        engine::GestureRuntimeEvent evt{kind, type, confidence, duration, static_cast<std::int64_t>(timestamp_seconds * 1e6)};
        if (event_bus_ != nullptr) {
            event_bus_->publish(evt);
        }
        return evt;
    };

    if (current_gesture_ == GestureType::kNone) {
        if (valid) {
            current_gesture_ = gesture.gesture;
            start_ts_ = timestamp_seconds;
            last_seen_ts_ = timestamp_seconds;
            held_ = false;
            event = emit(GestureEventKind::kStarted, gesture.gesture, gesture.confidence, 0.0);
        }
        return event;
    }

    if (config_.disable_debounce) {
        current_gesture_ = gesture.gesture;
        last_seen_ts_ = timestamp_seconds;
        held_ = valid;
        if (valid) {
            return emit(GestureEventKind::kChanged, gesture.gesture, gesture.confidence, 0.0);
        }
        current_gesture_ = GestureType::kNone;
        return std::nullopt;
    }

    if (!valid) {
        if ((timestamp_seconds - last_seen_ts_) > kReleaseTimeoutSec) {
            event = emit(GestureEventKind::kReleased, current_gesture_, 0.0f, timestamp_seconds - start_ts_);
            if (current_gesture_ == GestureType::kGrab) {
                if (event_bus_ != nullptr) {
                    engine::GestureRuntimeEvent release_evt = emit(GestureEventKind::kChanged, GestureType::kRelease, 0.8f, 0.0);
                    event = release_evt;
                }
            }
            current_gesture_ = GestureType::kNone;
            held_ = false;
        }
        return event;
    }

    last_seen_ts_ = timestamp_seconds;
    if (gesture.gesture != current_gesture_) {
        current_gesture_ = gesture.gesture;
        start_ts_ = timestamp_seconds;
        held_ = false;
        return emit(GestureEventKind::kChanged, gesture.gesture, gesture.confidence, 0.0);
    }

    if (!held_ && (timestamp_seconds - start_ts_) >= kHoldThresholdSec) {
        held_ = true;
        return emit(GestureEventKind::kHeld, gesture.gesture, gesture.confidence, timestamp_seconds - start_ts_);
    }

    return std::nullopt;
}

}  // namespace arx::vision
