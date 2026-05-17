#include "engine/config/runtime_config.hpp"

#include <functional>
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
    cfg.camera.source = extract_string(text, "source").value_or(cfg.camera.source);
    cfg.camera.width = static_cast<int>(extract_number(text, "width").value_or(cfg.camera.width));
    cfg.camera.height = static_cast<int>(extract_number(text, "height").value_or(cfg.camera.height));
    cfg.camera.fps = extract_number(text, "fps").value_or(cfg.camera.fps);
    cfg.camera.open_timeout_ms = static_cast<int>(extract_number(text, "open_timeout_ms").value_or(cfg.camera.open_timeout_ms));
    cfg.camera.reconnect_delay_ms = static_cast<int>(extract_number(text, "reconnect_delay_ms").value_or(cfg.camera.reconnect_delay_ms));
    cfg.camera.allow_reconnect = extract_bool(text, "allow_reconnect").value_or(cfg.camera.allow_reconnect);
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
    cfg.fusion.provider_type = extract_string(text, "provider_type").value_or(cfg.fusion.provider_type);
    cfg.fusion.overlay_enabled = extract_bool(text, "overlay_enabled").value_or(cfg.fusion.overlay_enabled);
    cfg.fusion.hand_skeleton_enabled = extract_bool(text, "hand_skeleton_enabled").value_or(cfg.fusion.hand_skeleton_enabled);
    cfg.fusion.face_overlay_enabled = extract_bool(text, "face_overlay_enabled").value_or(cfg.fusion.face_overlay_enabled);
    cfg.fusion.binary_face_overlay_enabled = extract_bool(text, "binary_face_overlay_enabled").value_or(cfg.fusion.binary_face_overlay_enabled);
    cfg.fusion.binary_face_overlay_opacity = static_cast<float>(
        extract_number(text, "binary_face_overlay_opacity").value_or(cfg.fusion.binary_face_overlay_opacity));
    cfg.fusion.binary_face_overlay_density = static_cast<float>(
        extract_number(text, "binary_face_overlay_density").value_or(cfg.fusion.binary_face_overlay_density));
    cfg.fusion.binary_face_overlay_speed = static_cast<float>(
        extract_number(text, "binary_face_overlay_speed").value_or(cfg.fusion.binary_face_overlay_speed));
    cfg.fusion.hud_animation_gain = static_cast<float>(
        extract_number(text, "hud_animation_gain").value_or(cfg.fusion.hud_animation_gain));
    cfg.fusion.jitter_threshold = static_cast<float>(
        extract_number(text, "jitter_threshold").value_or(cfg.fusion.jitter_threshold));
    cfg.fusion.replay_output_path = extract_string(text, "replay_output_path").value_or(cfg.fusion.replay_output_path.string());
    cfg.fusion.max_latency_ms = extract_number(text, "max_latency_ms").value_or(cfg.fusion.max_latency_ms);
    cfg.fusion.smoothing_profile = extract_string(text, "smoothing_profile").value_or(cfg.fusion.smoothing_profile);
    cfg.fusion.overlay_profile = extract_string(text, "overlay_profile").value_or(cfg.fusion.overlay_profile);
    cfg.fusion.replay_policy = extract_string(text, "replay_policy").value_or(cfg.fusion.replay_policy);
    cfg.fusion.telemetry_policy = extract_string(text, "telemetry_policy").value_or(cfg.fusion.telemetry_policy);
    cfg.fusion.failover_policy = extract_string(text, "failover_policy").value_or(cfg.fusion.failover_policy);
    return cfg;
}

std::string ConfigValidationResult::summary() const {
    if (errors.empty()) {
        return "config valid";
    }
    std::ostringstream out;
    out << "config invalid:";
    for (const auto& error : errors) {
        out << "\n - " << error;
    }
    return out.str();
}

ConfigValidationResult validate_runtime_config(const RuntimeConfig& cfg) {
    ConfigValidationResult result;
    if (cfg.target_fps == 0) {
        result.errors.push_back("target_fps must be greater than 0");
    }
    if (cfg.fixed_timestep_hz == 0) {
        result.errors.push_back("fixed_timestep_hz must be greater than 0");
    }
    if (cfg.camera.width <= 0 || cfg.camera.height <= 0) {
        result.errors.push_back("camera width/height must be positive");
    }
    if (cfg.camera.fps <= 0.0) {
        result.errors.push_back("camera fps must be greater than 0");
    }
    if (cfg.camera.source.empty()) {
        result.errors.push_back("camera source must not be empty");
    }
    if (cfg.fusion.max_latency_ms <= 0.0) {
        result.errors.push_back("fusion max_latency_ms must be greater than 0");
    }
    if (cfg.fusion.provider_type.empty()) {
        result.errors.push_back("fusion provider_type must not be empty");
    }
    if (cfg.fusion.provider_type != "mediapipe-production" &&
        cfg.fusion.provider_type != "synthetic" &&
        cfg.fusion.provider_type != "external") {
        result.errors.push_back("fusion provider_type must be mediapipe-production, external, or synthetic");
    }
    if (cfg.fusion.binary_face_overlay_opacity < 0.0f || cfg.fusion.binary_face_overlay_opacity > 1.0f) {
        result.errors.push_back("binary_face_overlay_opacity must be between 0 and 1");
    }
    if (cfg.fusion.binary_face_overlay_density < 0.0f || cfg.fusion.binary_face_overlay_density > 1.0f) {
        result.errors.push_back("binary_face_overlay_density must be between 0 and 1");
    }
    if (cfg.fusion.binary_face_overlay_speed < 0.0f) {
        result.errors.push_back("binary_face_overlay_speed must be non-negative");
    }
    if (cfg.tracking.gesture_threshold < 0.0f || cfg.tracking.gesture_threshold > 1.0f) {
        result.errors.push_back("gesture_threshold must be between 0 and 1");
    }
    return result;
}

std::string runtime_config_hash(const RuntimeConfig& cfg) {
    std::ostringstream out;
    out << cfg.engine_name << '|'
        << cfg.camera.source << '|'
        << cfg.camera.width << 'x' << cfg.camera.height << '@' << cfg.camera.fps << '|'
        << cfg.fusion.provider_type << '|'
        << cfg.fusion.overlay_profile << '|'
        << cfg.fusion.smoothing_profile << '|'
        << cfg.tracking.hand_model_path.string() << '|'
        << cfg.tracking.face_model_path.string();
    return std::to_string(std::hash<std::string>{}(out.str()));
}

}  // namespace arx::engine::config
