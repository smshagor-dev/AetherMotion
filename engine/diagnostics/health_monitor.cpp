#include "engine/diagnostics/health_monitor.hpp"

namespace arx::engine::diagnostics {

void HealthMonitor::report(HealthSignal signal) {
    signals_[signal.name] = std::move(signal);
}

bool HealthMonitor::healthy() const noexcept {
    for (const auto& [_, signal] : signals_) {
        if (!signal.healthy) {
            return false;
        }
    }
    return true;
}

const std::unordered_map<std::string, HealthSignal>& HealthMonitor::signals() const noexcept {
    return signals_;
}

}  // namespace arx::engine::diagnostics
