#pragma once

#include <chrono>
#include <string>
#include <unordered_map>

namespace arx::engine::profiling {

struct ScopeStat {
    double last_ms{0.0};
    double max_ms{0.0};
};

class FrameProfiler {
public:
    class Scope {
    public:
        Scope(FrameProfiler& owner, std::string name);
        ~Scope();

    private:
        FrameProfiler& owner_;
        std::string name_;
        std::chrono::steady_clock::time_point start_;
    };

    [[nodiscard]] Scope scope(std::string name) {
        return Scope(*this, std::move(name));
    }

    void record(const std::string& name, double elapsed_ms);
    [[nodiscard]] const std::unordered_map<std::string, ScopeStat>& stats() const noexcept;

private:
    std::unordered_map<std::string, ScopeStat> stats_;
};

}  // namespace arx::engine::profiling
