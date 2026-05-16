#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>

namespace arx::engine::profiling {

struct MetricSample {
    double last{0.0};
    double average{0.0};
    double max{0.0};
    std::uint64_t samples{0};
};

class RuntimeMetrics {
public:
    void record_latency(const std::string& name, double value_ms);
    void record_counter(const std::string& name, std::uint64_t value);

    [[nodiscard]] std::unordered_map<std::string, MetricSample> snapshot() const;
    [[nodiscard]] std::string to_json() const;
    [[nodiscard]] std::string to_csv() const;

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, MetricSample> metrics_;
};

}  // namespace arx::engine::profiling
