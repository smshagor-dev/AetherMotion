// ─────────────────────────────────────────────────────────────────────────────
// main.cpp  –  ARX Vision Engine entry point.
//              Wires up CameraManager → FramePipeline and blocks until Ctrl-C.
// ─────────────────────────────────────────────────────────────────────────────

#include "camera_manager.hpp"
#include "frame_pipeline.hpp"
#include "arx_types.hpp"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <csignal>
#include <atomic>
#include <chrono>
#include <thread>

namespace {
    std::atomic<bool> g_shutdown{false};
}

static void signal_handler(int) { g_shutdown.store(true); }

int main(int argc, char** argv) {
    // ── Logging ──────────────────────────────────────────────────────────
    auto logger = spdlog::stdout_color_mt("arx");
    spdlog::set_default_logger(logger);
    spdlog::set_level(spdlog::level::info);
    spdlog::set_pattern("[%H:%M:%S.%e] [%^%l%$] [arx] %v");

    std::signal(SIGINT,  signal_handler);
    std::signal(SIGTERM, signal_handler);

    spdlog::info("ARX Vision Engine v2.0 starting...");

    // ── Pipeline config ───────────────────────────────────────────────────
    arx::PipelineConfig cfg;
    cfg.display_width      = 1280;
    cfg.display_height     = 720;
    cfg.ai_width           = 640;
    cfg.ai_height          = 480;
    cfg.shm_key            = arx::kSharedMemKeyBase;
    cfg.zmq_telemetry_port = arx::kZmqTelemetryPort;
    cfg.show_window        = true;

    arx::FramePipeline pipeline(cfg);
    pipeline.start();

    // ── Camera setup ──────────────────────────────────────────────────────
    arx::CameraConfig cam_cfg;
    cam_cfg.width  = 1280;
    cam_cfg.height = 720;
    cam_cfg.fps    = 60;

    arx::CameraManager cameras([&](const cv::Mat& frame, const arx::FrameMeta& meta) {
        pipeline.push_raw(frame, meta);
    });

    const int cam_id = (argc > 1) ? std::atoi(argv[1]) : 0;
    cameras.add_camera(cam_id, cam_cfg);
    cameras.start();

    spdlog::info("Pipeline running. Press Ctrl-C to stop.");

    // ── Main loop: print stats every 5 s ─────────────────────────────────
    while (!g_shutdown.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        auto s = pipeline.stats();
        spdlog::info("Stats | displayed={} raw_drops={} ai_drops={} compose_drops={}",
                     s.frames_displayed, s.raw_drops, s.ai_drops, s.compose_drops);
    }

    spdlog::info("Shutting down...");
    cameras.stop();
    pipeline.stop();
    spdlog::info("ARX Vision Engine stopped cleanly.");
    return 0;
}
