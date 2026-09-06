#include <csignal>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>

#include "apps/runtime_cli.hpp"
#include "engine/config/runtime_config.hpp"
#include "engine/core/application.hpp"
#include "engine/ipc/local_ipc_protocol.hpp"
#include "engine/ipc/local_ipc_server.hpp"
#include "engine/runtime/runtime_context.hpp"
#include "engine/telemetry/telemetry_encoder.hpp"
#include "vision/tracking/model_asset_validator.hpp"

namespace {

arx::engine::Application* g_runtime_app = nullptr;

void handle_sigint(int) {
    if (g_runtime_app != nullptr) {
        g_runtime_app->request_shutdown();
    }
}

std::string runtime_status_json(
    const arx::engine::Application& app,
    arx::engine::RuntimeMode mode) {
    std::ostringstream out;
    out << "{"
        << "\"state\":\"" << (app.shutdown_requested() ? "stopping" : (app.paused() ? "paused" : "running")) << "\","
        << "\"paused\":" << (app.paused() ? "true" : "false") << ","
        << "\"shutdown_requested\":" << (app.shutdown_requested() ? "true" : "false") << ","
        << "\"mode\":\"" << arx::apps::runtime_mode_name(mode) << "\""
        << "}";
    return out.str();
}

}  // namespace

int main(int argc, char** argv) {
    const auto options = arx::apps::parse_runtime_cli(argc, argv);
    const auto config_path =
        (options.mode == arx::engine::RuntimeMode::kGraphicalFusion ||
         options.mode == arx::engine::RuntimeMode::kFusionReplay ||
         options.mode == arx::engine::RuntimeMode::kValidateReplay)
            ? "configs/arx_graphical_fusion.json"
            : "configs/arx_v3_runtime.json";
    auto config = arx::engine::config::load_runtime_config(config_path);
    if (options.camera_id.has_value()) {
        config.camera_id = *options.camera_id;
    }
    config.tracking.image_path = options.image_path;
    config.tracking.disable_gesture_classifier = options.disable_gesture_classifier;
    config.tracking.debug_gestures = options.debug_gestures;
    config.tracking.disable_debounce = options.disable_debounce;
    config.tracking.gesture_threshold = options.gesture_threshold;
    auto session_path = options.session_path;
    if (session_path.empty() && options.mode == arx::engine::RuntimeMode::kFusionReplay) {
        session_path = config.fusion.replay_output_path;
    }
    const auto validation = arx::engine::config::validate_runtime_config(config);
    if (!validation.valid()) {
        std::cerr << validation.summary() << '\n';
        return 2;
    }
    if (options.mode == arx::engine::RuntimeMode::kGraphicalFusion &&
        config.fusion.provider_type == "synthetic") {
        std::cerr << "config invalid:\n - synthetic provider is not allowed in production graphical-fusion mode\n";
        return 2;
    }

    if (options.check_models) {
        const auto report = arx::vision::tracking::validate_tracking_models(config);
        std::cout << report.summary();
        return report.all_valid() ? 0 : 2;
    }

    arx::engine::Application app(config, options.mode, session_path);
    arx::engine::ipc::LocalIpcServer ipc_server;
    if (!options.disable_ipc) {
        ipc_server.set_command_handler([&ipc_server, &app, mode = options.mode](std::string_view payload) {
            const auto command = arx::engine::ipc::parse_runtime_command(payload);
            if (!command.has_value()) {
                ipc_server.publish(arx::engine::ipc::protocol_command_result(
                    "unknown", "error", "invalid command payload"));
                return;
            }

            using arx::engine::ipc::RuntimeCommandKind;
            switch (command->kind) {
            case RuntimeCommandKind::kPing:
                ipc_server.publish(arx::engine::ipc::protocol_pong(command->request_id));
                break;
            case RuntimeCommandKind::kStatus:
                ipc_server.publish(arx::engine::ipc::protocol_command_result(
                    command->name,
                    "ok",
                    {},
                    command->request_id,
                    runtime_status_json(app, mode)));
                break;
            case RuntimeCommandKind::kPause:
                app.request_pause(true);
                ipc_server.publish(arx::engine::ipc::protocol_command_result(
                    command->name, "ok", "pause requested", command->request_id,
                    runtime_status_json(app, mode)));
                break;
            case RuntimeCommandKind::kResume:
                app.request_pause(false);
                ipc_server.publish(arx::engine::ipc::protocol_command_result(
                    command->name, "ok", "resume requested", command->request_id,
                    runtime_status_json(app, mode)));
                break;
            case RuntimeCommandKind::kShutdown:
                ipc_server.publish(arx::engine::ipc::protocol_command_result(
                    command->name, "ok", "graceful shutdown requested", command->request_id,
                    runtime_status_json(app, mode)));
                app.request_shutdown();
                break;
            case RuntimeCommandKind::kUnknown:
            default:
                ipc_server.publish(arx::engine::ipc::protocol_command_result(
                    command->name, "error", "unsupported command", command->request_id));
                break;
            }
        });

        if (ipc_server.start(options.ipc_port)) {
            arx::engine::telemetry::TelemetryEncoder::install_process_sink(
                [&ipc_server](std::string_view payload) {
                    ipc_server.publish(payload);
                });
            std::cout << "Local IPC: 127.0.0.1:" << ipc_server.port()
                      << " protocol=v" << arx::engine::ipc::kProtocolVersion << '\n';
        } else {
            std::cerr << "[AetherMotion][ipc][warn] local IPC unavailable on 127.0.0.1:"
                      << options.ipc_port << "; continuing without desktop telemetry transport\n";
        }
    }

    std::cout << "Starting " << config.engine_name << '\n';
    std::cout << "Mode: " << arx::apps::runtime_mode_name(options.mode) << '\n';
    if (options.deprecated_fusion_demo_alias) {
        std::cerr << "[ARX][warn] --mode fusion-demo is deprecated; use --mode graphical-fusion\n";
    }

    g_runtime_app = &app;
    std::signal(SIGINT, handle_sigint);
    const int code = app.run();
    g_runtime_app = nullptr;

    arx::engine::telemetry::TelemetryEncoder::clear_process_sink();
    ipc_server.stop();
    return code;
}
