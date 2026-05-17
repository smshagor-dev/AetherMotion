#include "ar/fusion/fusion_compositor.hpp"

#include <algorithm>
#include <cmath>

namespace arx::ar::fusion {

FusionCompositor::FusionCompositor(engine::config::FusionConfig config)
    : config_(std::move(config)) {}

void FusionCompositor::configure(engine::config::FusionConfig config) {
    config_ = std::move(config);
}

ARFusionFrame FusionCompositor::compose(const vision::landmarks::LandmarkProviderOutput& raw,
                                        const vision::GestureEngine::Output& gesture_output,
                                        const std::optional<std::string>& gesture_event_name,
                                        double camera_latency_ms,
                                        double inference_latency_ms,
                                        double smoothing_latency_ms,
                                        double gesture_latency_ms,
                                        double render_latency_ms,
                                        std::uint64_t dropped_frames) {
    ARFusionFrame frame;
    frame.provider_name = raw.debug.provider_name;
    frame.meta = raw.frame.meta;
    frame.raw_hands = raw.frame.hands;
    frame.smoothed_hands = gesture_output.smoothed.hands;
    frame.face = gesture_output.smoothed.face.has_value() ? gesture_output.smoothed.face : raw.frame.face;
    frame.gesture_label = gesture_output.gesture.label;
    frame.gesture_confidence = gesture_output.gesture.confidence;
    frame.gesture_event = gesture_event_name.value_or("none");

    const bool tracked = !frame.smoothed_hands.empty();
    const float target_fade = tracked ? 1.0f : 0.0f;
    tracking_fade_ += (target_fade - tracking_fade_) * (tracked ? 0.35f : 0.12f);
    if (tracking_fade_ < config_.jitter_threshold) {
        tracking_fade_ = 0.0f;
    }

    frame.telemetry.camera_latency_ms = camera_latency_ms;
    frame.telemetry.inference_latency_ms = inference_latency_ms;
    frame.telemetry.smoothing_latency_ms = smoothing_latency_ms;
    frame.telemetry.gesture_latency_ms = gesture_latency_ms;
    frame.telemetry.render_latency_ms = render_latency_ms;
    frame.telemetry.dropped_frames = dropped_frames;
    frame.telemetry.tracking_fade = tracking_fade_;
    frame.telemetry.tracker_state = raw.debug.tracker_state;

    frame.hud.opacity = tracking_fade_;
    frame.hud.visible = tracking_fade_ > 0.01f;
    frame.hud.gesture_card_visible = frame.hud.visible;
    frame.hud.primary_label = frame.gesture_label;

    if (!frame.smoothed_hands.empty()) {
        const auto& hand = frame.smoothed_hands.front();
        frame.hud.pinch_distance = pinch_distance(hand);
        const auto cursor = midpoint(hand.points[4], hand.points[8]);
        if (!anchor_initialized_) {
            smoothed_cursor_ = cursor;
            anchor_initialized_ = true;
        } else {
            const float alpha = std::clamp(config_.hud_animation_gain, 0.05f, 1.0f);
            smoothed_cursor_.x += (cursor.x - smoothed_cursor_.x) * alpha;
            smoothed_cursor_.y += (cursor.y - smoothed_cursor_.y) * alpha;
            smoothed_cursor_.z += (cursor.z - smoothed_cursor_.z) * alpha;
        }
        frame.hud.cursor = smoothed_cursor_;
        frame.hud.spatial_cursor_visible = config_.overlay_enabled;
        frame.hud.pinch_ring_visible = frame.gesture_label == "pinch" || frame.hud.pinch_distance < 0.08f;
        frame.hud.triangular_frame_visible = frame.gesture_label == "pinch" || frame.gesture_label == "point_up";
        frame.hud.zoom_guide_visible = frame.gesture_label == "zoom" || frame.gesture_label == "rotate" || frame.smoothed_hands.size() > 1;

        frame.anchors.push_back({"cursor", smoothed_cursor_, hand.confidence, true});
        frame.anchors.push_back({"wrist", hand.points[0], hand.confidence, true});
    } else {
        frame.hud.primary_label = raw.debug.tracker_state;
        frame.hud.spatial_cursor_visible = tracking_fade_ > 0.0f;
        frame.hud.pinch_ring_visible = false;
        frame.hud.triangular_frame_visible = false;
        frame.hud.zoom_guide_visible = false;
    }

    frame.face_binary.enabled = config_.binary_face_overlay_enabled;
    frame.face_binary.active = config_.binary_face_overlay_enabled && frame.face.has_value();
    frame.face_binary.opacity = frame.face_binary.active ? config_.binary_face_overlay_opacity * tracking_fade_ : 0.0f;
    frame.face_binary.density = config_.binary_face_overlay_density;
    frame.face_binary.speed = config_.binary_face_overlay_speed;
    if (frame.face_binary.active) {
        float min_x = 1.0f;
        float min_y = 1.0f;
        float max_x = 0.0f;
        float max_y = 0.0f;
        for (const auto& point : frame.face->points) {
            min_x = std::min(min_x, point.x);
            min_y = std::min(min_y, point.y);
            max_x = std::max(max_x, point.x);
            max_y = std::max(max_y, point.y);
        }
        frame.face_binary.min_x = min_x;
        frame.face_binary.min_y = min_y;
        frame.face_binary.max_x = max_x;
        frame.face_binary.max_y = max_y;
    }

    return frame;
}

void FusionCompositor::reset() {
    tracking_fade_ = 0.0f;
    anchor_initialized_ = false;
    smoothed_cursor_ = {};
}

float FusionCompositor::pinch_distance(const vision::HandLandmarks& hand) {
    const auto dx = hand.points[4].x - hand.points[8].x;
    const auto dy = hand.points[4].y - hand.points[8].y;
    const auto dz = hand.points[4].z - hand.points[8].z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

vision::Point3f FusionCompositor::midpoint(const vision::Point3f& a, const vision::Point3f& b) {
    return {(a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f, (a.z + b.z) * 0.5f};
}

}  // namespace arx::ar::fusion
