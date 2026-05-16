#include <iostream>

#include "engine/ipc/ipc_protocol.hpp"
#include "engine/profiling/frame_profiler.hpp"
#include "engine/profiling/runtime_metrics.hpp"

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
    using namespace arx::engine;

    ipc::PacketHeader header;
    profiling::FrameProfiler profiler;
    profiling::RuntimeMetrics metrics;
    {
        auto scope = profiler.scope("latency.frame");
    }
    metrics.record_latency("camera.capture_ms", 1.5);

    const auto& stats = profiler.stats();
    bool ok = true;
    ok &= expect_true(header.magic == 0x41525833, "IPC packet magic should match ARX3");
    ok &= expect_true(header.version == 300, "IPC packet version should be 300");
    ok &= expect_true(stats.find("latency.frame") != stats.end(), "Profiler should record a scope stat");
    ok &= expect_true(stats.at("latency.frame").last_ms >= 0.0, "Profiler stat should have a measured duration");
    ok &= expect_true(metrics.to_json().find("camera.capture_ms") != std::string::npos, "Runtime metrics should export recorded samples");

    return ok ? 0 : 1;
}
