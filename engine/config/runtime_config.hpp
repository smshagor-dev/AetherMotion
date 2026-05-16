#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

namespace arx::engine::config {

struct CameraRuntimeConfig {
    int id{0};
    int width{1280};
    int height{720};
    double fps{60.0};
};

struct RecordingConfig {
    std::filesystem::path session_dir{"sessions"};
    bool record_frames{true};
    bool record_gestures{true};
};

struct TrackingConfig {
    bool enable_hands{true};
    bool enable_face{true};
    bool enable_world_landmarks{true};
    std::filesystem::path models_dir{"models"};
    std::filesystem::path hand_model_path{"models/hand_landmarker.task"};
    std::filesystem::path face_model_path{"models/face_landmarker.task"};
    bool live_stream_mode{false};
    std::uint32_t max_hands{2};
    std::uint32_t max_faces{1};
    float min_hand_detection_confidence{0.5f};
    float min_hand_presence_confidence{0.5f};
    float min_hand_tracking_confidence{0.5f};
    float min_face_detection_confidence{0.5f};
    float min_face_presence_confidence{0.5f};
    float min_face_tracking_confidence{0.5f};
    bool disable_gesture_classifier{false};
    bool debug_gestures{false};
    bool disable_debounce{false};
    float gesture_threshold{0.5f};
    std::filesystem::path image_path;
};

struct RuntimeConfig {
    std::string engine_name{"ARX Platform v3.0"};
    std::string subtitle{"C++-First Real-Time Gesture Intelligence and Spatial AR Engine"};
    std::uint32_t target_fps{60};
    std::uint32_t fixed_timestep_hz{120};
    double latency_budget_ms{20.0};
    bool enable_profiler{true};
    bool enable_remote_services{false};
    bool headless{false};
    bool dashboard_enabled{true};
    int camera_id{0};
    CameraRuntimeConfig camera{};
    RecordingConfig recording{};
    TrackingConfig tracking{};
};

RuntimeConfig load_runtime_config(const std::string& path);

}  // namespace arx::engine::config
