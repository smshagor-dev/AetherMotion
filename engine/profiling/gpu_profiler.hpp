#pragma once

#include <string>

namespace arx::engine::profiling {

class GpuProfiler {
public:
    void begin_frame() noexcept {}
    void end_frame() noexcept {}
    [[nodiscard]] double last_frame_ms() const noexcept { return 0.0; }
    [[nodiscard]] std::string backend() const { return "unavailable"; }
};

}  // namespace arx::engine::profiling
