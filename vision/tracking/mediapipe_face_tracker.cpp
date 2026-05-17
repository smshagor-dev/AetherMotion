#include "vision/tracking/mediapipe_face_tracker.hpp"

#include <chrono>
#include <cstring>
#include <filesystem>

#if defined(ARX_HAS_MEDIAPIPE)
#include "mediapipe/framework/formats/image_frame.h"
#include "mediapipe/tasks/cc/core/base_options.h"
#include "mediapipe/tasks/cc/vision/core/running_mode.h"
#include "mediapipe/tasks/cc/vision/face_landmarker/face_landmarker.h"
#endif

#include "vision/tracking/model_asset_validator.hpp"

namespace arx::vision::tracking {

class MediaPipeFaceTracker::Impl {
public:
    explicit Impl(const engine::config::RuntimeConfig& config)
        : config_(config) {
        const auto asset = validate_model_asset("face model", config_.tracking.face_model_path, config_.tracking.models_dir);
        model_path_ = asset.resolved_path.string();
    }

    bool initialize() {
        initialized_ = true;
        model_loaded_ = std::filesystem::exists(model_path_);
        if (!model_loaded_) {
            tracker_state_ = "model_missing";
            last_error_ = "face model not found: " + model_path_;
            return false;
        }
#if defined(ARX_HAS_MEDIAPIPE)
        auto options = std::make_unique<mediapipe::tasks::vision::face_landmarker::FaceLandmarkerOptions>();
        options->base_options.model_asset_path = model_path_;
        options->running_mode = mediapipe::tasks::vision::core::RunningMode::VIDEO;
        options->num_faces = static_cast<int>(config_.tracking.max_faces);
        options->min_face_detection_confidence = config_.tracking.min_face_detection_confidence;
        options->min_face_presence_confidence = config_.tracking.min_face_presence_confidence;
        options->min_tracking_confidence = config_.tracking.min_face_tracking_confidence;
        auto tracker = mediapipe::tasks::vision::face_landmarker::FaceLandmarker::Create(std::move(options));
        if (!tracker.ok()) {
            available_ = false;
            tracker_state_ = "init_failed";
            last_error_ = tracker.status().ToString();
            return false;
        }
        tracker_ = std::move(*tracker);
        available_ = true;
        tracker_state_ = "ready";
        last_error_.clear();
#else
        available_ = false;
        tracker_state_ = "mediapipe_missing";
        last_error_ = "MediaPipe face tracker unavailable: build without ARX_HAS_MEDIAPIPE";
#endif
        return available_;
    }

    FaceTrackingFrame track(const VideoFrame& frame) {
        FaceTrackingFrame output;
        const auto start = std::chrono::steady_clock::now();
        output.initialized = initialized_;
        output.model_loaded = model_loaded_;
        output.model_path = model_path_;
        output.tracker_state = tracker_state_;
        if (!available_) {
            output.error = last_error_;
        } else {
#if defined(ARX_HAS_MEDIAPIPE)
            if (frame.rgb.empty()) {
                output.error = "face tracker received empty RGB frame";
                output.tracker_state = "empty_frame";
            } else {
                auto image_frame = std::make_unique<mediapipe::ImageFrame>(
                    mediapipe::ImageFormat::SRGB,
                    frame.rgb.cols,
                    frame.rgb.rows,
                    mediapipe::ImageFrame::kDefaultAlignmentBoundary);
                const std::size_t bytes = static_cast<std::size_t>(frame.rgb.rows * frame.rgb.step);
                std::memcpy(image_frame->MutablePixelData(), frame.rgb.data, bytes);
                mediapipe::Image image(std::move(image_frame));
                auto result = tracker_->DetectForVideo(std::move(image), frame.meta.capture_us / 1000);
                if (!result.ok()) {
                    output.error = result.status().ToString();
                    output.tracker_state = "detect_failed";
                } else {
                    const auto& detected = *result;
                    output.tracker_state = detected.face_landmarks.empty() ? "no_faces" : "tracking";
                    output.ok = true;
                    if (!detected.face_landmarks.empty()) {
                        FaceLandmarks face;
                        const auto& landmarks = detected.face_landmarks.front().landmarks;
                        for (std::size_t point_index = 0;
                             point_index < std::min<std::size_t>(landmarks.size(), kFaceLandmarkCount);
                             ++point_index) {
                            const auto& p = landmarks[point_index];
                            face.points[point_index] = {p.x, p.y, p.z};
                        }
                        face.confidence = 1.0f;
                        output.face = face;
                    }
                }
            }
#else
            (void)frame;
#endif
        }
        output.latency_ms = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - start).count();
        return output;
    }

    [[nodiscard]] bool available() const noexcept {
        return available_;
    }

    [[nodiscard]] bool model_loaded() const noexcept {
        return model_loaded_;
    }

    [[nodiscard]] const std::string& model_path() const noexcept {
        return model_path_;
    }

    [[nodiscard]] const std::string& last_error() const noexcept {
        return last_error_;
    }

private:
    engine::config::RuntimeConfig config_;
#if defined(ARX_HAS_MEDIAPIPE)
    std::unique_ptr<mediapipe::tasks::vision::face_landmarker::FaceLandmarker> tracker_;
#endif
    bool initialized_{false};
    bool available_{false};
    bool model_loaded_{false};
    std::string model_path_;
    std::string tracker_state_{"offline"};
    std::string last_error_;
};

MediaPipeFaceTracker::MediaPipeFaceTracker(const engine::config::RuntimeConfig& config)
    : impl_(std::make_unique<Impl>(config)) {}

MediaPipeFaceTracker::~MediaPipeFaceTracker() = default;

bool MediaPipeFaceTracker::initialize() {
    return impl_->initialize();
}

FaceTrackingFrame MediaPipeFaceTracker::track(const VideoFrame& frame) {
    return impl_->track(frame);
}

bool MediaPipeFaceTracker::available() const noexcept {
    return impl_->available();
}

bool MediaPipeFaceTracker::model_loaded() const noexcept {
    return impl_->model_loaded();
}

const std::string& MediaPipeFaceTracker::model_path() const noexcept {
    return impl_->model_path();
}

const std::string& MediaPipeFaceTracker::last_error() const noexcept {
    return impl_->last_error();
}

}  // namespace arx::vision::tracking
