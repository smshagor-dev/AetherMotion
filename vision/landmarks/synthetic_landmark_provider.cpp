#include "vision/landmarks/synthetic_landmark_provider.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace arx::vision::landmarks {

namespace {

using HandTemplate = std::array<Point3f, kHandLandmarkCount>;

HandTemplate make_base_hand(bool left) {
    HandTemplate hand{};
    const float sign = left ? -1.0f : 1.0f;
    hand[0] = {0.5f + sign * 0.10f, 0.72f, 0.02f};
    hand[1] = {0.48f + sign * 0.12f, 0.66f, 0.00f};
    hand[2] = {0.46f + sign * 0.15f, 0.60f, -0.02f};
    hand[3] = {0.44f + sign * 0.18f, 0.55f, -0.03f};
    hand[4] = {0.43f + sign * 0.21f, 0.50f, -0.04f};
    hand[5] = {0.50f + sign * 0.05f, 0.58f, 0.00f};
    hand[6] = {0.50f + sign * 0.05f, 0.50f, -0.02f};
    hand[7] = {0.50f + sign * 0.05f, 0.42f, -0.03f};
    hand[8] = {0.50f + sign * 0.05f, 0.34f, -0.04f};
    hand[9] = {0.50f, 0.60f, 0.00f};
    hand[10] = {0.50f, 0.51f, -0.02f};
    hand[11] = {0.50f, 0.42f, -0.03f};
    hand[12] = {0.50f, 0.33f, -0.04f};
    hand[13] = {0.50f - sign * 0.04f, 0.62f, 0.00f};
    hand[14] = {0.50f - sign * 0.04f, 0.54f, -0.02f};
    hand[15] = {0.50f - sign * 0.04f, 0.47f, -0.03f};
    hand[16] = {0.50f - sign * 0.04f, 0.40f, -0.04f};
    hand[17] = {0.50f - sign * 0.08f, 0.65f, 0.00f};
    hand[18] = {0.50f - sign * 0.08f, 0.59f, -0.01f};
    hand[19] = {0.50f - sign * 0.08f, 0.54f, -0.02f};
    hand[20] = {0.50f - sign * 0.08f, 0.49f, -0.03f};
    return hand;
}

std::array<Point3f, kFaceLandmarkCount> make_face(float t) {
    std::array<Point3f, kFaceLandmarkCount> face{};
    const float cx = 0.50f + 0.01f * std::sin(t * 0.7f);
    const float cy = 0.35f + 0.01f * std::cos(t * 0.9f);
    for (std::size_t i = 0; i < kFaceLandmarkCount; ++i) {
        const float angle = static_cast<float>(i) / static_cast<float>(kFaceLandmarkCount) * 6.2831853f;
        const float ring = 0.12f + 0.02f * std::sin(angle * 3.0f + t);
        face[i] = {cx + ring * std::cos(angle), cy + ring * 1.15f * std::sin(angle), -0.02f};
    }
    return face;
}

void animate_hand(HandTemplate& hand, float t, bool left, bool pinch) {
    const float sign = left ? -1.0f : 1.0f;
    const float sway_x = 0.04f * std::sin(t * 1.1f + (left ? 0.0f : 1.7f));
    const float sway_y = 0.03f * std::cos(t * 0.9f + (left ? 0.4f : 1.1f));
    for (auto& point : hand) {
        point.x = std::clamp(point.x + sway_x, 0.05f, 0.95f);
        point.y = std::clamp(point.y + sway_y, 0.05f, 0.95f);
    }
    const float spread = 0.06f + 0.02f * std::sin(t * 2.4f);
    hand[8].x = 0.50f + sign * spread + sway_x;
    hand[8].y = 0.33f + sway_y;
    hand[4].x = pinch ? hand[8].x - sign * 0.015f : 0.43f + sign * 0.21f + sway_x;
    hand[4].y = pinch ? hand[8].y + 0.01f : 0.50f + sway_y;
}

}  // namespace

SyntheticLandmarkProvider::SyntheticLandmarkProvider(const engine::config::RuntimeConfig& config)
    : config_(config) {}

bool SyntheticLandmarkProvider::initialize() {
    available_ = true;
    last_error_.clear();
    return true;
}

LandmarkProviderOutput SyntheticLandmarkProvider::process(tracking::VideoFrame& frame) {
    LandmarkProviderOutput out;
    out.frame.frame_id = frame.meta.frame_id;
    out.frame.meta = frame.meta;
    const float t = static_cast<float>(frame.meta.capture_us > 0 ? frame.meta.capture_us : now_us()) / 1000000.0f;

    HandTemplate right_template = make_base_hand(false);
    const bool pinch = std::sin(t * 1.5f) > 0.35f;
    animate_hand(right_template, t, false, pinch);

    HandLandmarks right{};
    right.points = right_template;
    right.confidence = 0.93f;
    right.is_left = false;
    out.frame.hands.push_back(right);
    out.hand_world_landmarks.push_back(right_template);
    out.handedness_scores.push_back(0.93f);

    if (config_.tracking.max_hands > 1 && std::sin(t * 0.7f) > -0.2f) {
        HandTemplate left_template = make_base_hand(true);
        animate_hand(left_template, t + 0.7f, true, false);
        HandLandmarks left{};
        left.points = left_template;
        left.confidence = 0.88f;
        left.is_left = true;
        out.frame.hands.push_back(left);
        out.hand_world_landmarks.push_back(left_template);
        out.handedness_scores.push_back(0.88f);
    }

    if (config_.tracking.enable_face) {
        FaceLandmarks face{};
        face.points = make_face(t);
        face.confidence = 0.84f;
        out.frame.face = face;
        out.face_world_landmarks = face.points;
    }

    out.hand_inference_ms = 0.2;
    out.face_inference_ms = out.frame.face.has_value() ? 0.15 : 0.0;
    out.inference_ok = true;
    out.debug.provider_initialized = true;
    out.debug.live_source_active = true;
    out.debug.hand_model_loaded = true;
    out.debug.face_model_loaded = config_.tracking.enable_face;
    out.debug.raw_hand_count = out.frame.hands.size();
    out.debug.raw_face_count = out.frame.face.has_value() ? 1u : 0u;
    out.debug.top_hand_confidence = out.frame.hands.empty() ? 0.0f : out.frame.hands.front().confidence;
    out.debug.provider_name = name();
    out.debug.tracker_state = out.frame.hands.empty() ? "synthetic_idle" : "synthetic_tracking";
    out.debug.status_detail = "synthetic landmark stream";
    return out;
}

bool SyntheticLandmarkProvider::available() const noexcept {
    return available_;
}

const std::string& SyntheticLandmarkProvider::last_error() const noexcept {
    return last_error_;
}

const char* SyntheticLandmarkProvider::name() const noexcept {
    return "synthetic";
}

}  // namespace arx::vision::landmarks
