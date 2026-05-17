#pragma once

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "engine/config/runtime_config.hpp"
#include "vision/landmarks/landmark_types.hpp"
#include "vision/tracking/landmark_runtime_bridge.hpp"

namespace arx::vision::landmarks {

struct LandmarkProviderDebugState {
    bool provider_initialized{false};
    bool live_source_active{false};
    bool hand_model_loaded{false};
    bool face_model_loaded{false};
    std::size_t raw_hand_count{0};
    std::size_t raw_face_count{0};
    float top_hand_confidence{0.0f};
    std::string provider_name{"offline"};
    std::string tracker_state{"offline"};
    std::string hand_model_path;
    std::string face_model_path;
    std::string status_detail;
};

struct LandmarkProviderOutput {
    FrameLandmarks frame;
    std::vector<std::array<Point3f, kHandLandmarkCount>> hand_world_landmarks;
    std::optional<std::array<Point3f, kFaceLandmarkCount>> face_world_landmarks;
    std::vector<float> handedness_scores;
    double hand_inference_ms{0.0};
    double face_inference_ms{0.0};
    bool inference_ok{false};
    std::string failure_reason;
    LandmarkProviderDebugState debug;
};

class LandmarkProvider {
public:
    virtual ~LandmarkProvider() = default;

    [[nodiscard]] virtual bool initialize() = 0;
    [[nodiscard]] virtual LandmarkProviderOutput process(tracking::VideoFrame& frame) = 0;
    [[nodiscard]] virtual bool available() const noexcept = 0;
    [[nodiscard]] virtual const std::string& last_error() const noexcept = 0;
    [[nodiscard]] virtual const char* name() const noexcept = 0;
};

std::unique_ptr<LandmarkProvider> make_landmark_provider(const engine::config::RuntimeConfig& config);

}  // namespace arx::vision::landmarks
