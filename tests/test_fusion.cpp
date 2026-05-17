#include <filesystem>
#include <iostream>

#include "ar/fusion/fusion_compositor.hpp"
#include "replay/playback/session_player.hpp"
#include "replay/recorder/session_recorder.hpp"

namespace {

bool expect_true(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << '\n';
        return false;
    }
    return true;
}

arx::vision::HandLandmarks make_hand(bool left, float confidence, float thumb_x, float index_x) {
    arx::vision::HandLandmarks hand;
    hand.is_left = left;
    hand.confidence = confidence;
    for (std::size_t i = 0; i < arx::vision::kHandLandmarkCount; ++i) {
        hand.points[i] = {0.40f + 0.01f * static_cast<float>(i), 0.50f - 0.01f * static_cast<float>(i), 0.0f};
    }
    hand.points[4].x = thumb_x;
    hand.points[8].x = index_x;
    hand.points[4].y = 0.40f;
    hand.points[8].y = 0.40f;
    return hand;
}

}

int main() {
    bool ok = true;
    arx::engine::config::FusionConfig config;
    config.binary_face_overlay_enabled = true;
    config.binary_face_overlay_density = 0.5f;
    config.jitter_threshold = 0.01f;
    arx::ar::fusion::FusionCompositor compositor(config);

    arx::vision::landmarks::LandmarkProviderOutput raw;
    raw.frame.meta.frame_id = 1;
    raw.frame.meta.capture_us = 1000;
    raw.frame.hands.push_back(make_hand(false, 0.92f, 0.48f, 0.50f));
    raw.inference_ok = true;
    raw.debug.tracker_state = "tracking";

    arx::vision::GestureEngine::Output gesture_out;
    gesture_out.smoothed = raw.frame;
    gesture_out.gesture.label = "pinch";
    gesture_out.gesture.confidence = 0.91f;

    auto fusion = compositor.compose(raw, gesture_out, std::string("STARTED"), 1.0, 2.0, 3.0, 4.0, 5.0, 0);
    fusion.provider_name = "mediapipe-production";
    fusion.config_hash = "abc123";
    ok &= expect_true(fusion.hud.visible, "Fusion HUD should be visible while tracking");
    ok &= expect_true(fusion.hud.pinch_ring_visible, "Fusion HUD should show pinch ring for pinch gesture");
    ok &= expect_true(!fusion.anchors.empty(), "Fusion frame should create anchors");

    arx::vision::landmarks::LandmarkProviderOutput lost_raw;
    lost_raw.frame.meta.frame_id = 2;
    lost_raw.frame.meta.capture_us = 2000;
    lost_raw.debug.tracker_state = "lost";
    arx::vision::GestureEngine::Output lost_out;
    lost_out.smoothed = lost_raw.frame;
    auto faded = compositor.compose(lost_raw, lost_out, std::nullopt, 0.0, 0.0, 0.0, 0.0, 0.0, 2);
    ok &= expect_true(faded.telemetry.tracking_fade < fusion.telemetry.tracking_fade, "Fusion fade should decay after tracking is lost");

    namespace fs = std::filesystem;
    const fs::path dir = "fusion_test_sessions";
    arx::replay::SessionRecorder recorder(dir);
    ok &= expect_true(recorder.begin_session("fusion"), "Fusion recorder should open a session");
    arx::replay::SessionFrame session_frame;
    session_frame.timestamp_us = 1234;
    session_frame.frame_id = 9;
    session_frame.fps = 60.0;
    session_frame.hands = fusion.smoothed_hands;
    session_frame.face = fusion.face;
    session_frame.gesture = gesture_out.gesture;
    session_frame.fusion = fusion;
    recorder.record_frame(session_frame);
    recorder.end_session();

    arx::replay::SessionPlayer player;
    ok &= expect_true(player.load(dir / "fusion.jsonl"), "Fusion player should load fusion replay");
    const auto replayed = player.next();
    ok &= expect_true(replayed.has_value(), "Fusion replay should return a frame");
    ok &= expect_true(replayed->fusion.has_value(), "Fusion replay should preserve fusion frame payload");
    ok &= expect_true(replayed->fusion->gesture_label == "pinch", "Fusion replay should preserve gesture label");
    ok &= expect_true(replayed->fusion->schema_version >= 1, "Fusion replay should preserve schema version");
    ok &= expect_true(replayed->fusion->provider_name == "mediapipe-production", "Fusion replay should preserve provider name");
    ok &= expect_true(!replayed->hands.empty(), "Fusion replay should preserve hand landmarks");
    fs::remove_all(dir);
    return ok ? 0 : 1;
}
