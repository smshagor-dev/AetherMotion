#include "vision/landmarks/external_landmark_provider.hpp"

#include "vision/landmarks/synthetic_landmark_provider.hpp"

namespace arx::vision::landmarks {

ProductionMediaPipeLandmarkProvider::ProductionMediaPipeLandmarkProvider(const engine::config::RuntimeConfig& config)
    : config_(config) {}

ProductionMediaPipeLandmarkProvider::~ProductionMediaPipeLandmarkProvider() = default;

bool ProductionMediaPipeLandmarkProvider::initialize() {
#ifdef ARX_HAS_OPENCV
    bridge_ = std::make_unique<tracking::LandmarkRuntimeBridge>(config_);
    available_ = bridge_->initialize();
    last_error_ = bridge_->last_error();
    return available_;
#else
    available_ = false;
    last_error_ = "external landmark provider requires OpenCV-enabled runtime support";
    return false;
#endif
}

LandmarkProviderOutput ProductionMediaPipeLandmarkProvider::process(tracking::VideoFrame& frame) {
    LandmarkProviderOutput out;
#ifdef ARX_HAS_OPENCV
    if (bridge_ == nullptr) {
        out.failure_reason = "external landmark provider not initialized";
        out.debug.provider_name = name();
        out.debug.tracker_state = "offline";
        out.debug.status_detail = out.failure_reason;
        return out;
    }

    const auto tracking = bridge_->process(frame);
    out.frame = tracking.frame;
    out.hand_world_landmarks = tracking.hand_world_landmarks;
    out.face_world_landmarks = tracking.face_world_landmarks;
    out.handedness_scores = tracking.handedness_scores;
    out.hand_inference_ms = tracking.hand_inference_ms;
    out.face_inference_ms = tracking.face_inference_ms;
    out.inference_ok = tracking.inference_ok;
    out.failure_reason = tracking.failure_reason;
    out.debug.provider_initialized = available_;
    out.debug.live_source_active = true;
    out.debug.hand_model_loaded = tracking.debug.hand_model_loaded;
    out.debug.face_model_loaded = tracking.debug.face_model_loaded;
    out.debug.raw_hand_count = tracking.debug.raw_hand_count;
    out.debug.raw_face_count = tracking.debug.raw_face_count;
    out.debug.top_hand_confidence = tracking.debug.top_hand_confidence;
    out.debug.provider_name = name();
    out.debug.tracker_state = tracking.debug.tracker_state;
    out.debug.hand_model_path = tracking.debug.hand_model_path;
    out.debug.face_model_path = tracking.debug.face_model_path;
    out.debug.status_detail = tracking.debug.status_detail;
#else
    (void)frame;
    out.failure_reason = "external landmark provider requires OpenCV-enabled runtime support";
    out.debug.provider_name = name();
    out.debug.tracker_state = "offline";
    out.debug.status_detail = out.failure_reason;
#endif
    return out;
}

bool ProductionMediaPipeLandmarkProvider::available() const noexcept {
    return available_;
}

const std::string& ProductionMediaPipeLandmarkProvider::last_error() const noexcept {
    return last_error_;
}

const char* ProductionMediaPipeLandmarkProvider::name() const noexcept {
    return "mediapipe-production";
}

std::unique_ptr<LandmarkProvider> make_landmark_provider(const engine::config::RuntimeConfig& config) {
    if (config.fusion.provider_type == "synthetic") {
        return std::make_unique<SyntheticLandmarkProvider>(config);
    }
    return std::make_unique<ProductionMediaPipeLandmarkProvider>(config);
}

}  // namespace arx::vision::landmarks
