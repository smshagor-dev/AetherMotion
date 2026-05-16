#include "engine/config/runtime_config.hpp"

#include <fstream>
#include <optional>
#include <regex>
#include <sstream>

namespace arx::engine::config {

namespace {

std::optional<double> extract_number(const std::string& source, const std::string& key) {
    const std::regex re("\"" + key + "\"\\s*:\\s*([0-9]+(?:\\.[0-9]+)?)");
    std::smatch match;
    if (std::regex_search(source, match, re)) {
        return std::stod(match[1].str());
    }
    return std::nullopt;
}

std::optional<bool> extract_bool(const std::string& source, const std::string& key) {
    const std::regex re("\"" + key + "\"\\s*:\\s*(true|false)");
    std::smatch match;
    if (std::regex_search(source, match, re)) {
        return match[1].str() == "true";
    }
    return std::nullopt;
}

std::optional<std::string> extract_string(const std::string& source, const std::string& key) {
    const std::regex re("\"" + key + "\"\\s*:\\s*\"([^\"]*)\"");
    std::smatch match;
    if (std::regex_search(source, match, re)) {
        return match[1].str();
    }
    return std::nullopt;
}

}

RuntimeConfig load_runtime_config(const std::string& path) {
    RuntimeConfig cfg;
    std::ifstream in(path);
    if (!in.is_open()) {
        return cfg;
    }

    std::ostringstream buffer;
    buffer << in.rdbuf();
    const std::string text = buffer.str();

    cfg.target_fps = static_cast<std::uint32_t>(extract_number(text, "target_fps").value_or(cfg.target_fps));
    cfg.fixed_timestep_hz = static_cast<std::uint32_t>(extract_number(text, "fixed_timestep_hz").value_or(cfg.fixed_timestep_hz));
    cfg.latency_budget_ms = extract_number(text, "latency_budget_ms").value_or(cfg.latency_budget_ms);
    cfg.camera_id = static_cast<int>(extract_number(text, "id").value_or(cfg.camera_id));
    cfg.engine_name = extract_string(text, "engine_name").value_or(cfg.engine_name);
    cfg.subtitle = extract_string(text, "subtitle").value_or(cfg.subtitle);
    cfg.enable_profiler = extract_bool(text, "enable_profiler").value_or(cfg.enable_profiler);
    cfg.enable_remote_services = extract_bool(text, "enable_remote_services").value_or(cfg.enable_remote_services);
    cfg.headless = extract_bool(text, "headless").value_or(cfg.headless);
    cfg.dashboard_enabled = extract_bool(text, "enabled").value_or(cfg.dashboard_enabled);
    cfg.camera.id = cfg.camera_id;
    cfg.camera.width = static_cast<int>(extract_number(text, "width").value_or(cfg.camera.width));
    cfg.camera.height = static_cast<int>(extract_number(text, "height").value_or(cfg.camera.height));
    cfg.camera.fps = extract_number(text, "fps").value_or(cfg.camera.fps);
    cfg.recording.session_dir = extract_string(text, "session_dir").value_or(cfg.recording.session_dir.string());
    cfg.recording.record_frames = extract_bool(text, "record_frames").value_or(cfg.recording.record_frames);
    cfg.recording.record_gestures = extract_bool(text, "record_gestures").value_or(cfg.recording.record_gestures);
    cfg.tracking.enable_hands = extract_bool(text, "enable_hands").value_or(cfg.tracking.enable_hands);
    cfg.tracking.enable_face = extract_bool(text, "enable_face").value_or(cfg.tracking.enable_face);
    cfg.tracking.enable_world_landmarks = extract_bool(text, "enable_world_landmarks").value_or(cfg.tracking.enable_world_landmarks);
    cfg.tracking.models_dir = extract_string(text, "models_dir").value_or(cfg.tracking.models_dir.string());
    cfg.tracking.hand_model_path = extract_string(text, "hand_model_path").value_or(cfg.tracking.hand_model_path.string());
    cfg.tracking.face_model_path = extract_string(text, "face_model_path").value_or(cfg.tracking.face_model_path.string());
    cfg.tracking.live_stream_mode = extract_bool(text, "live_stream_mode").value_or(cfg.tracking.live_stream_mode);
    cfg.tracking.max_hands = static_cast<std::uint32_t>(extract_number(text, "max_hands").value_or(cfg.tracking.max_hands));
    cfg.tracking.max_faces = static_cast<std::uint32_t>(extract_number(text, "max_faces").value_or(cfg.tracking.max_faces));
    cfg.tracking.min_hand_detection_confidence = static_cast<float>(
        extract_number(text, "min_hand_detection_confidence").value_or(cfg.tracking.min_hand_detection_confidence));
    cfg.tracking.min_hand_presence_confidence = static_cast<float>(
        extract_number(text, "min_hand_presence_confidence").value_or(cfg.tracking.min_hand_presence_confidence));
    cfg.tracking.min_hand_tracking_confidence = static_cast<float>(
        extract_number(text, "min_hand_tracking_confidence").value_or(cfg.tracking.min_hand_tracking_confidence));
    cfg.tracking.min_face_detection_confidence = static_cast<float>(
        extract_number(text, "min_face_detection_confidence").value_or(cfg.tracking.min_face_detection_confidence));
    cfg.tracking.min_face_presence_confidence = static_cast<float>(
        extract_number(text, "min_face_presence_confidence").value_or(cfg.tracking.min_face_presence_confidence));
    cfg.tracking.min_face_tracking_confidence = static_cast<float>(
        extract_number(text, "min_face_tracking_confidence").value_or(cfg.tracking.min_face_tracking_confidence));
    cfg.tracking.disable_gesture_classifier = extract_bool(text, "disable_gesture_classifier").value_or(cfg.tracking.disable_gesture_classifier);
    cfg.tracking.debug_gestures = extract_bool(text, "debug_gestures").value_or(cfg.tracking.debug_gestures);
    cfg.tracking.disable_debounce = extract_bool(text, "disable_debounce").value_or(cfg.tracking.disable_debounce);
    cfg.tracking.gesture_threshold = static_cast<float>(
        extract_number(text, "gesture_threshold").value_or(cfg.tracking.gesture_threshold));
    return cfg;
}

}  // namespace arx::engine::config
