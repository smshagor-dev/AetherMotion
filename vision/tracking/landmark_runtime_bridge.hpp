#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#ifdef ARX_HAS_OPENCV
#include <opencv2/core.hpp>
#endif

#include "engine/config/runtime_config.hpp"
#include "vision/landmarks/landmark_types.hpp"

namespace arx::vision::tracking {

struct VideoFrame {
#ifdef ARX_HAS_OPENCV
    cv::Mat bgr;
    cv::Mat rgb;
    cv::Mat render;
#endif
    FrameMetadata meta{};
};

struct TrackingResult {
    struct DebugState {
        bool mediapipe_compiled{false};
        bool frame_received{false};
        bool hand_tracker_initialized{false};
        bool face_tracker_initialized{false};
        bool hand_model_loaded{false};
        bool face_model_loaded{false};
        std::size_t raw_hand_count{0};
        std::size_t raw_face_count{0};
        float top_hand_confidence{0.0f};
        std::string tracker_state{"offline"};
        std::string hand_model_path;
        std::string face_model_path;
        std::string status_detail;
    };

    FrameLandmarks frame;
    std::vector<std::array<Point3f, kHandLandmarkCount>> hand_world_landmarks;
    std::optional<std::array<Point3f, kFaceLandmarkCount>> face_world_landmarks;
    std::vector<float> handedness_scores;
    double hand_inference_ms{0.0};
    double face_inference_ms{0.0};
    bool inference_ok{false};
    std::string failure_reason;
    DebugState debug;
};

class LandmarkRuntimeBridge {
public:
    explicit LandmarkRuntimeBridge(const engine::config::RuntimeConfig& config);
    ~LandmarkRuntimeBridge();

    LandmarkRuntimeBridge(const LandmarkRuntimeBridge&) = delete;
    LandmarkRuntimeBridge& operator=(const LandmarkRuntimeBridge&) = delete;

    [[nodiscard]] bool initialize();
    [[nodiscard]] TrackingResult process(VideoFrame& frame);
    [[nodiscard]] bool available() const noexcept;
    [[nodiscard]] const std::string& last_error() const noexcept;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace arx::vision::tracking
