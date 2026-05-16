#pragma once

#include <array>
#include <memory>
#include <string>
#include <vector>

#include "engine/config/runtime_config.hpp"
#include "vision/landmarks/landmark_types.hpp"
#include "vision/tracking/landmark_runtime_bridge.hpp"

namespace arx::vision::tracking {

struct HandTrackingFrame {
    std::vector<HandLandmarks> hands;
    std::vector<std::array<Point3f, kHandLandmarkCount>> world_landmarks;
    std::vector<float> handedness_scores;
    double latency_ms{0.0};
    bool ok{false};
    bool initialized{false};
    bool model_loaded{false};
    std::string tracker_state{"offline"};
    std::string model_path;
    std::string error;
};

class MediaPipeHandTracker {
public:
    explicit MediaPipeHandTracker(const engine::config::RuntimeConfig& config);
    ~MediaPipeHandTracker();

    [[nodiscard]] bool initialize();
    [[nodiscard]] HandTrackingFrame track(const VideoFrame& frame);
    [[nodiscard]] bool available() const noexcept;
    [[nodiscard]] bool model_loaded() const noexcept;
    [[nodiscard]] const std::string& model_path() const noexcept;
    [[nodiscard]] const std::string& last_error() const noexcept;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace arx::vision::tracking
