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
        "--mode", "fusion-demo",
        "--camera", "2",
        "--image", "test.jpg",
        "--check-models",
        "--disable-gesture-classifier",
        "--debug-gestures",
        "--disable-debounce",
        "--ipc-port", "48123",
        "--no-ipc",
        "--gesture-threshold", "0.65"
    };

    const auto options = arx::apps::parse_runtime_cli(static_cast<int>(sizeof(argv) / sizeof(argv[0])), const_cast<char**>(argv));
    bool ok = true;
    ok &= expect_true(options.mode == arx::engine::RuntimeMode::kGraphicalFusion, "CLI should parse deprecated fusion-demo alias");
    ok &= expect_true(options.camera_id.has_value() && *options.camera_id == 2, "CLI should parse camera id");
    ok &= expect_true(options.image_path == "test.jpg", "CLI should parse image path");
    ok &= expect_true(options.check_models, "CLI should parse --check-models");
    ok &= expect_true(options.disable_gesture_classifier, "CLI should parse --disable-gesture-classifier");
    ok &= expect_true(options.debug_gestures, "CLI should parse --debug-gestures");
    ok &= expect_true(options.disable_debounce, "CLI should parse --disable-debounce");
    ok &= expect_true(options.ipc_port == 48123, "CLI should parse --ipc-port");
    ok &= expect_true(options.disable_ipc, "CLI should parse --no-ipc");
    ok &= expect_true(options.gesture_threshold > 0.64f && options.gesture_threshold < 0.66f, "CLI should parse gesture threshold");
    ok &= expect_true(options.deprecated_fusion_demo_alias, "CLI should mark deprecated fusion-demo alias");
    ok &= expect_true(arx::apps::runtime_mode_name(options.mode) == "graphical-fusion", "Runtime mode name should match production mode");
    return ok ? 0 : 1;
}
