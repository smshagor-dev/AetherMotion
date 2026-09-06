#include "apps/runtime_cli.hpp"

#include <cerrno>
#include <cstdlib>
#include <optional>
#include <string>

namespace arx::apps {

namespace {

std::optional<std::uint16_t> parse_port(const char* raw) {
    if (raw == nullptr || *raw == '\0') {
        return std::nullopt;
    }

    errno = 0;
    char* end = nullptr;
    const unsigned long value = std::strtoul(raw, &end, 10);
    if (errno != 0 || end == raw || *end != '\0' || value == 0 || value > 65535UL) {
        return std::nullopt;
    }
    return static_cast<std::uint16_t>(value);
}

}  // namespace

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
            } else if (value == "graphical-fusion" || value == "fusion-live") {
                options.mode = engine::RuntimeMode::kGraphicalFusion;
            } else if (value == "fusion-demo") {
                options.mode = engine::RuntimeMode::kGraphicalFusion;
                options.deprecated_fusion_demo_alias = true;
            } else if (value == "fusion-replay") {
                options.mode = engine::RuntimeMode::kFusionReplay;
            } else if (value == "validate-replay") {
                options.mode = engine::RuntimeMode::kValidateReplay;
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
        } else if (arg == "--no-ipc") {
            options.disable_ipc = true;
        } else if (arg == "--ipc-port" && i + 1 < argc) {
            if (const auto port = parse_port(argv[++i]); port.has_value()) {
                options.ipc_port = *port;
            }
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
    case engine::RuntimeMode::kGraphicalFusion:
        return "graphical-fusion";
    case engine::RuntimeMode::kFusionReplay:
        return "fusion-replay";
    case engine::RuntimeMode::kValidateReplay:
        return "validate-replay";
    default:
        return "live";
    }
}

}  // namespace arx::apps
