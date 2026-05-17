#include <iostream>

#include "engine/config/runtime_config.hpp"
#include "vision/landmarks/synthetic_landmark_provider.hpp"

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
    arx::engine::config::RuntimeConfig config;
    config.tracking.enable_face = true;
    config.tracking.max_hands = 2;
    arx::vision::landmarks::SyntheticLandmarkProvider provider(config);
    bool ok = provider.initialize();

    arx::vision::tracking::VideoFrame frame;
    frame.meta.frame_id = 7;
    frame.meta.capture_us = arx::vision::now_us();
    frame.meta.width = 1280;
    frame.meta.height = 720;
    const auto output = provider.process(frame);

    ok &= expect_true(provider.available(), "Synthetic landmark provider should be available");
    ok &= expect_true(output.inference_ok, "Synthetic provider should emit landmarks");
    ok &= expect_true(output.frame.frame_id == 7, "Synthetic provider should preserve frame id");
    ok &= expect_true(!output.frame.hands.empty(), "Synthetic provider should emit at least one hand");
    ok &= expect_true(output.debug.provider_name == "synthetic", "Synthetic provider should report its name");
    ok &= expect_true(output.frame.face.has_value(), "Synthetic provider should emit face landmarks when enabled");
    return ok ? 0 : 1;
}
