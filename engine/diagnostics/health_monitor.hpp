#pragma once

#include <chrono>
#include <string>
#include <unordered_map>

namespace arx::engine::diagnostics {

struct HealthSignal {
    std::string name;
    bool healthy{true};
    std::string detail;
    std::chrono::steady_clock::time_point updated_at{std::chrono::steady_clock::now()};
};

class HealthMonitor {
public:
    void report(HealthSignal signal);
    [[nodiscard]] bool healthy() const noexcept;
    [[nodiscard]] const std::unordered_map<std::string, HealthSignal>& signals() const noexcept;

private:
    std::unordered_map<std::string, HealthSignal> signals_;
};

}  // namespace arx::engine::diagnostics
