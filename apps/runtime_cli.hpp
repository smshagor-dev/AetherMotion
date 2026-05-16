#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "engine/config/runtime_config.hpp"
#include "engine/runtime/runtime_context.hpp"

namespace arx::apps {

struct RuntimeCliOptions {
    engine::RuntimeMode mode{engine::RuntimeMode::kLive};
    std::filesystem::path session_path;
    std::optional<int> camera_id;
    std::filesystem::path image_path;
    bool check_models{false};
    bool disable_gesture_classifier{false};
    bool debug_gestures{false};
    bool disable_debounce{false};
    float gesture_threshold{0.5f};
};

RuntimeCliOptions parse_runtime_cli(int argc, char** argv);
std::string runtime_mode_name(engine::RuntimeMode mode);

}  // namespace arx::apps
