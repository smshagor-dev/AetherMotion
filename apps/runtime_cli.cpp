#include "apps/runtime_cli.hpp"

#include <cstdlib>
#include <string>

namespace arx::apps {

RuntimeCliOptions parse_runtime_cli(int argc, char** argv) {
    RuntimeCliOptions options;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--mode" && i + 1 < argc) {
            const std::string value = argv[++i];
            if (value == "record") {
                options.mode = engine::RuntimeMode::kRecord;
            } else if (value == "replay") {
                options.mode = engine::RuntimeMode::kReplay;
            } else if (value == "tracker-smoke") {
                options.mode = engine::RuntimeMode::kTrackerSmoke;
            } else {
                options.mode = engine::RuntimeMode::kLive;
            }
        } else if (arg == "--session" && i + 1 < argc) {
            options.session_path = argv[++i];
        } else if (arg == "--camera" && i + 1 < argc) {
            options.camera_id = std::atoi(argv[++i]);
        } else if (arg == "--image" && i + 1 < argc) {
            options.image_path = argv[++i];
        } else if (arg == "--check-models") {
            options.check_models = true;
        } else if (arg == "--disable-gesture-classifier") {
            options.disable_gesture_classifier = true;
        } else if (arg == "--debug-gestures") {
            options.debug_gestures = true;
        } else if (arg == "--disable-debounce") {
            options.disable_debounce = true;
        } else if (arg == "--gesture-threshold" && i + 1 < argc) {
            options.gesture_threshold = std::strtof(argv[++i], nullptr);
        }
    }
    return options;
}

std::string runtime_mode_name(engine::RuntimeMode mode) {
    switch (mode) {
    case engine::RuntimeMode::kLive:
        return "live";
    case engine::RuntimeMode::kRecord:
        return "record";
    case engine::RuntimeMode::kReplay:
        return "replay";
    case engine::RuntimeMode::kTrackerSmoke:
        return "tracker-smoke";
    default:
        return "live";
    }
}

}  // namespace arx::apps
