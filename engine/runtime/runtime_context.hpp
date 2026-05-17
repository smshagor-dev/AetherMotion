#pragma once

#include <atomic>
#include <filesystem>
#include <optional>
#include <string>

#include "engine/config/runtime_config.hpp"
#include "engine/diagnostics/health_monitor.hpp"
#include "engine/events/event_bus.hpp"
#include "engine/plugins/plugin_registry.hpp"
#include "engine/profiling/frame_profiler.hpp"
#include "engine/profiling/gpu_profiler.hpp"
#include "engine/profiling/runtime_metrics.hpp"
#include "engine/telemetry/telemetry_encoder.hpp"
#include "engine/threading/thread_pool.hpp"
#include "vision/gesture_engine/gesture_types.hpp"

namespace arx::engine {

enum class RuntimeMode : std::uint8_t {
    kLive = 0,
    kRecord = 1,
    kReplay = 2,
    kTrackerSmoke = 3,
    kGraphicalFusion = 4,
    kFusionReplay = 5,
    kValidateReplay = 6,
};

struct RuntimeStats {
    std::atomic<std::uint64_t> frames_processed{0};
    std::atomic<std::uint64_t> dropped_frames{0};
    std::atomic<double> last_latency_ms{0.0};
};

struct RuntimeContext {
    config::RuntimeConfig config;
    RuntimeMode mode{RuntimeMode::kLive};
    std::filesystem::path session_path;
    EventBus event_bus;
    ThreadPool worker_pool;
    profiling::FrameProfiler profiler;
    profiling::GpuProfiler gpu_profiler;
    profiling::RuntimeMetrics metrics;
    diagnostics::HealthMonitor health_monitor;
    telemetry::TelemetryEncoder telemetry;
    PluginRegistry plugins;
    RuntimeStats stats;
    std::string current_gesture{"none"};
    bool recording{false};
    bool replaying{false};
};

}  // namespace arx::engine
