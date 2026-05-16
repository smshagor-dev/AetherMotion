#include "engine/profiling/frame_profiler.hpp"

namespace arx::engine::profiling {

FrameProfiler::Scope::Scope(FrameProfiler& owner, std::string name)
    : owner_(owner), name_(std::move(name)), start_(std::chrono::steady_clock::now()) {}

FrameProfiler::Scope::~Scope() {
    const auto end = std::chrono::steady_clock::now();
    const double elapsed_ms =
        std::chrono::duration<double, std::milli>(end - start_).count();
    owner_.record(name_, elapsed_ms);
}

void FrameProfiler::record(const std::string& name, double elapsed_ms) {
    auto& stat = stats_[name];
    stat.last_ms = elapsed_ms;
    if (elapsed_ms > stat.max_ms) {
        stat.max_ms = elapsed_ms;
    }
}

const std::unordered_map<std::string, ScopeStat>& FrameProfiler::stats() const noexcept {
    return stats_;
}

}  // namespace arx::engine::profiling
