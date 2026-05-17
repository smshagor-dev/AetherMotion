#pragma once

#include <optional>
#include <string>
#include <vector>

#include "vision/landmarks/landmark_types.hpp"

namespace arx::ar::fusion {

struct ARAnchor {
    std::string id;
    vision::Point3f position{};
    float confidence{0.0f};
    bool active{false};
};

struct HUDState {
    bool visible{false};
    bool triangular_frame_visible{false};
    bool gesture_card_visible{false};
    bool spatial_cursor_visible{false};
    bool pinch_ring_visible{false};
    bool zoom_guide_visible{false};
    float opacity{0.0f};
    float pinch_distance{0.0f};
    std::string primary_label{"idle"};
    vision::Point3f cursor{};
};

struct FaceBinaryOverlayState {
    bool enabled{false};
    bool active{false};
    float opacity{0.0f};
    float density{0.0f};
    float speed{0.0f};
    float min_x{0.0f};
    float min_y{0.0f};
    float max_x{0.0f};
    float max_y{0.0f};
};

struct FusionTelemetry {
    double camera_latency_ms{0.0};
    double inference_latency_ms{0.0};
    double smoothing_latency_ms{0.0};
    double gesture_latency_ms{0.0};
    double render_latency_ms{0.0};
    std::uint64_t dropped_frames{0};
    float tracking_fade{0.0f};
    std::string tracker_state{"offline"};
};

struct ARFusionFrame {
    std::uint32_t schema_version{2};
    std::string runtime_version{"3.0"};
    std::string provider_name{"unknown"};
    std::string config_hash;
    vision::FrameMetadata meta{};
    std::vector<vision::HandLandmarks> raw_hands;
    std::vector<vision::HandLandmarks> smoothed_hands;
    std::optional<vision::FaceLandmarks> face;
    std::string gesture_label{"none"};
    float gesture_confidence{0.0f};
    std::string gesture_event{"none"};
    std::vector<ARAnchor> anchors;
    HUDState hud{};
    FaceBinaryOverlayState face_binary{};
    FusionTelemetry telemetry{};
};

}  // namespace arx::ar::fusion
