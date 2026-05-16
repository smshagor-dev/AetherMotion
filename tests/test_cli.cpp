#include <iostream>

#include "apps/runtime_cli.hpp"

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
    const char* argv[] = {
        "arx_runtime",
        "--mode", "tracker-smoke",
        "--camera", "2",
        "--image", "test.jpg",
        "--check-models",
        "--disable-gesture-classifier",
        "--debug-gestures",
        "--disable-debounce",
        "--gesture-threshold", "0.65"
    };

    const auto options = arx::apps::parse_runtime_cli(static_cast<int>(sizeof(argv) / sizeof(argv[0])), const_cast<char**>(argv));
    bool ok = true;
    ok &= expect_true(options.mode == arx::engine::RuntimeMode::kTrackerSmoke, "CLI should parse tracker-smoke mode");
    ok &= expect_true(options.camera_id.has_value() && *options.camera_id == 2, "CLI should parse camera id");
    ok &= expect_true(options.image_path == "test.jpg", "CLI should parse image path");
    ok &= expect_true(options.check_models, "CLI should parse --check-models");
    ok &= expect_true(options.disable_gesture_classifier, "CLI should parse --disable-gesture-classifier");
    ok &= expect_true(options.debug_gestures, "CLI should parse --debug-gestures");
    ok &= expect_true(options.disable_debounce, "CLI should parse --disable-debounce");
    ok &= expect_true(options.gesture_threshold > 0.64f && options.gesture_threshold < 0.66f, "CLI should parse gesture threshold");
    ok &= expect_true(arx::apps::runtime_mode_name(options.mode) == "tracker-smoke", "Runtime mode name should match");
    return ok ? 0 : 1;
}
