#include <filesystem>
#include <fstream>
#include <iostream>

#include "engine/config/runtime_config.hpp"
#include "engine/telemetry/telemetry_encoder.hpp"
#include "vision/tracking/model_asset_validator.hpp"

namespace {

bool expect_true(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << '\n';
        return false;
    }
    return true;
}

}

int main() {
    namespace fs = std::filesystem;
    using arx::engine::config::RuntimeConfig;
    using arx::vision::tracking::validate_tracking_models;

    const fs::path root = "test_models";
    fs::create_directories(root);
    std::ofstream(root / "hand_landmarker.task") << "hand";

    RuntimeConfig config;
    config.tracking.models_dir = root;
    config.tracking.hand_model_path = "hand_landmarker.task";
    config.tracking.face_model_path = "face_landmarker.task";
    config.tracking.disable_gesture_classifier = true;
    config.tracking.debug_gestures = true;
    config.tracking.disable_debounce = true;
    config.tracking.gesture_threshold = 0.7f;

    const auto report = validate_tracking_models(config);
    bool ok = true;
    ok &= expect_true(report.assets.size() == 2, "Model validation should report both assets");
    ok &= expect_true(report.assets[0].exists, "Hand model should be found");
    ok &= expect_true(!report.assets[1].exists, "Face model should be reported missing");
    ok &= expect_true(report.summary().find("missing") != std::string::npos, "Summary should include missing model reporting");
    ok &= expect_true(config.tracking.models_dir == root, "Tracking models dir should be configurable");
    ok &= expect_true(config.tracking.disable_gesture_classifier, "Tracking config should retain gesture disable flag");
    ok &= expect_true(config.tracking.debug_gestures, "Tracking config should retain debug gestures flag");
    ok &= expect_true(config.tracking.disable_debounce, "Tracking config should retain debounce flag");
    ok &= expect_true(config.tracking.gesture_threshold > 0.69f && config.tracking.gesture_threshold < 0.71f,
        "Tracking config should retain gesture threshold");

    {
        std::ofstream cfg("test_runtime_config.json");
        cfg << "{"
            << "\"models_dir\":\"test_models\","
            << "\"hand_model_path\":\"hand_landmarker.task\","
            << "\"face_model_path\":\"face_landmarker.task\","
            << "\"enable_hands\":true,"
            << "\"enable_face\":false,"
            << "\"debug_gestures\":true,"
            << "\"disable_debounce\":true,"
            << "\"gesture_threshold\":0.75"
            << "}";
    }
    const auto loaded = arx::engine::config::load_runtime_config("test_runtime_config.json");
    ok &= expect_true(loaded.tracking.models_dir == "test_models", "Runtime config should parse models dir");
    ok &= expect_true(loaded.tracking.hand_model_path == "hand_landmarker.task", "Runtime config should parse hand model path");
    ok &= expect_true(loaded.tracking.enable_hands, "Runtime config should parse hand enable flag");
    ok &= expect_true(!loaded.tracking.enable_face, "Runtime config should parse face enable flag");
    ok &= expect_true(loaded.tracking.debug_gestures, "Runtime config should parse debug gestures flag");
    ok &= expect_true(loaded.tracking.disable_debounce, "Runtime config should parse debounce flag");
    ok &= expect_true(loaded.tracking.gesture_threshold > 0.74f && loaded.tracking.gesture_threshold < 0.76f,
        "Runtime config should parse gesture threshold");

    arx::engine::telemetry::RuntimeTelemetryFrame frame;
    frame.tracker_state = "model_missing";
    frame.tracker_error = "face model not found";
    frame.model_loaded = false;
    frame.raw_hand_count = 0;
    frame.camera_fps = 60.0;
    frame.end_to_end_latency_ms = 15.0;
    frame.landmark_confidence = 0.0f;
    frame.gesture_confidence = 0.0f;
    frame.provider_health = "degraded";
    frame.hand_model_path = (root / "hand_landmarker.task").string();
    frame.face_model_path = (root / "face_landmarker.task").string();

    arx::engine::telemetry::TelemetryEncoder encoder;
    const auto encoded = encoder.encode_frame(frame);
    ok &= expect_true(encoded.find("\"tracking\"") != std::string::npos, "Telemetry should encode tracking section");
    ok &= expect_true(encoded.find("model_missing") != std::string::npos, "Telemetry should include tracker state");
    ok &= expect_true(encoded.find("face model not found") != std::string::npos, "Telemetry should include tracker error");
    ok &= expect_true(encoded.find("\"camera_fps\":60") != std::string::npos, "Telemetry should include camera fps");
    ok &= expect_true(encoded.find("\"provider_health\":\"degraded\"") != std::string::npos, "Telemetry should include provider health");

    fs::remove_all(root);
    fs::remove("test_runtime_config.json");
    return ok ? 0 : 1;
}
