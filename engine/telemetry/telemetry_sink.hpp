#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

namespace arx::engine::telemetry {

struct TelemetrySample {
    std::string category;
    std::string key;
    double value{0.0};
    std::chrono::steady_clock::time_point timestamp{std::chrono::steady_clock::now()};
};

class TelemetrySink {
public:
    void push(TelemetrySample sample) {
        samples_.push_back(std::move(sample));
    }

    [[nodiscard]] const std::vector<TelemetrySample>& samples() const noexcept {
        return samples_;
    }

    void clear() {
        samples_.clear();
    }

private:
    std::vector<TelemetrySample> samples_;
};

}  // namespace arx::engine::telemetry
