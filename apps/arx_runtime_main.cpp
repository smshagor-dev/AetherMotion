#include <filesystem>
#include <iostream>
#include <csignal>
#include <string>

#include "apps/runtime_cli.hpp"
#include "engine/config/runtime_config.hpp"
#include "engine/core/application.hpp"
#include "engine/runtime/runtime_context.hpp"
#include "vision/tracking/model_asset_validator.hpp"

namespace {

arx::engine::Application* g_runtime_app = nullptr;

void handle_sigint(int) {
    if (g_runtime_app != nullptr) {
        g_runtime_app->request_shutdown();
    }
}

}

int main(int argc, char** argv) {
    auto config = arx::engine::config::load_runtime_config("configs/arx_v3_runtime.json");
    const auto options = arx::apps::parse_runtime_cli(argc, argv);
    if (options.camera_id.has_value()) {
        config.camera_id = *options.camera_id;
    }
    config.tracking.image_path = options.image_path;
    config.tracking.disable_gesture_classifier = options.disable_gesture_classifier;
    config.tracking.debug_gestures = options.debug_gestures;
    config.tracking.disable_debounce = options.disable_debounce;
    config.tracking.gesture_threshold = options.gesture_threshold;

    if (options.check_models) {
        const auto report = arx::vision::tracking::validate_tracking_models(config);
        std::cout << report.summary();
        return report.all_valid() ? 0 : 2;
    }

    std::cout << "Starting " << config.engine_name << '\n';
    std::cout << "Mode: " << arx::apps::runtime_mode_name(options.mode) << '\n';
    arx::engine::Application app(config, options.mode, options.session_path);
    g_runtime_app = &app;
    std::signal(SIGINT, handle_sigint);
    const int code = app.run();
    g_runtime_app = nullptr;
    return code;
}
