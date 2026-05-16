#include "vision/tracking/landmark_runtime_bridge.hpp"

#include <algorithm>
#include <sstream>

#ifdef ARX_HAS_OPENCV
#include <opencv2/imgproc.hpp>
#endif

#include "vision/tracking/mediapipe_face_tracker.hpp"
#include "vision/tracking/mediapipe_hand_tracker.hpp"

namespace arx::vision::tracking {

class LandmarkRuntimeBridge::Impl {
public:
    explicit Impl(const engine::config::RuntimeConfig& config)
        : config_(config),
          hand_tracker_(config),
          face_tracker_(config) {}

    bool initialize() {
        bool ready = false;
        last_error_.clear();
        if (config_.tracking.enable_hands) {
            ready = hand_tracker_.initialize() || ready;
            if (!hand_tracker_.available()) {
                last_error_ = hand_tracker_.last_error();
            }
        }
        if (config_.tracking.enable_face) {
            ready = face_tracker_.initialize() || ready;
            if (!face_tracker_.available() && last_error_.empty()) {
                last_error_ = face_tracker_.last_error();
            }
        }
        available_ = ready;
        return available_;
    }

    TrackingResult process(VideoFrame& frame) {
        TrackingResult result;
        result.frame.frame_id = frame.meta.frame_id;
        result.frame.meta = frame.meta;
        result.debug.mediapipe_compiled =
#if defined(ARX_HAS_MEDIAPIPE)
            true;
#else
            false;
#endif
        result.debug.frame_received = true;
        result.debug.hand_tracker_initialized = config_.tracking.enable_hands;
        result.debug.face_tracker_initialized = config_.tracking.enable_face;
        result.debug.hand_model_loaded = hand_tracker_.model_loaded();
        result.debug.face_model_loaded = face_tracker_.model_loaded();
        result.debug.hand_model_path = hand_tracker_.model_path();
        result.debug.face_model_path = face_tracker_.model_path();
        result.debug.tracker_state = available_ ? "ready" : "degraded";
        result.debug.status_detail = last_error_;

#ifdef ARX_HAS_OPENCV
        if (frame.rgb.empty() && !frame.bgr.empty()) {
            cv::cvtColor(frame.bgr, frame.rgb, cv::COLOR_BGR2RGB);
        }
        if (frame.render.empty() && !frame.bgr.empty()) {
            frame.render = frame.bgr.clone();
        }
#endif

        if (config_.tracking.enable_hands) {
            auto hands = hand_tracker_.track(frame);
            result.hand_inference_ms = hands.latency_ms;
            result.debug.hand_tracker_initialized = hands.initialized;
            result.debug.hand_model_loaded = hands.model_loaded;
            result.debug.hand_model_path = hands.model_path;
            result.debug.tracker_state = hands.tracker_state;
            if (hands.ok) {
                result.frame.hands = std::move(hands.hands);
                result.hand_world_landmarks = std::move(hands.world_landmarks);
                result.handedness_scores = std::move(hands.handedness_scores);
                result.debug.raw_hand_count = result.frame.hands.size();
                for (const auto& hand : result.frame.hands) {
                    result.debug.top_hand_confidence = std::max(result.debug.top_hand_confidence, hand.confidence);
                }
                result.inference_ok = true;
            } else if (!hands.error.empty()) {
                result.failure_reason = hands.error;
                result.debug.status_detail = hands.error;
            }
        }

        if (config_.tracking.enable_face) {
            auto face = face_tracker_.track(frame);
            result.face_inference_ms = face.latency_ms;
            result.debug.face_tracker_initialized = face.initialized;
            result.debug.face_model_loaded = face.model_loaded;
            result.debug.face_model_path = face.model_path;
            if (result.debug.tracker_state == "ready" || result.debug.tracker_state == "degraded") {
                result.debug.tracker_state = face.tracker_state;
            }
            if (face.ok) {
                result.frame.face = std::move(face.face);
                result.face_world_landmarks = std::move(face.world_landmarks);
                result.debug.raw_face_count = result.frame.face.has_value() ? 1u : 0u;
                result.inference_ok = result.inference_ok || result.frame.face.has_value();
            } else if (!face.error.empty() && result.failure_reason.empty()) {
                result.failure_reason = face.error;
                result.debug.status_detail = face.error;
            }
        }

        if (result.inference_ok && result.debug.raw_hand_count > 0) {
            result.debug.tracker_state = "tracking";
        } else if (result.inference_ok) {
            result.debug.tracker_state = result.debug.raw_face_count > 0 ? "tracking_face" : result.debug.tracker_state;
        } else if (result.debug.status_detail.empty()) {
            result.debug.status_detail = "tracker returned no landmarks";
            result.debug.tracker_state = "no_landmarks";
        }

        return result;
    }

    [[nodiscard]] bool available() const noexcept {
        return available_;
    }

    [[nodiscard]] const std::string& last_error() const noexcept {
        return last_error_;
    }

private:
    engine::config::RuntimeConfig config_;
    MediaPipeHandTracker hand_tracker_;
    MediaPipeFaceTracker face_tracker_;
    bool available_{false};
    std::string last_error_;
};

LandmarkRuntimeBridge::LandmarkRuntimeBridge(const engine::config::RuntimeConfig& config)
    : impl_(std::make_unique<Impl>(config)) {}

LandmarkRuntimeBridge::~LandmarkRuntimeBridge() = default;

bool LandmarkRuntimeBridge::initialize() {
    return impl_->initialize();
}

TrackingResult LandmarkRuntimeBridge::process(VideoFrame& frame) {
    return impl_->process(frame);
}

bool LandmarkRuntimeBridge::available() const noexcept {
    return impl_->available();
}

const std::string& LandmarkRuntimeBridge::last_error() const noexcept {
    return impl_->last_error();
}

}  // namespace arx::vision::tracking
