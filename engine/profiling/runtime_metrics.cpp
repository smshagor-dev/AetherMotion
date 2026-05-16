#include "engine/profiling/runtime_metrics.hpp"

#include <algorithm>
#include <sstream>

namespace arx::engine::profiling {

void RuntimeMetrics::record_latency(const std::string& name, double value_ms) {
    std::scoped_lock lock(mutex_);
    auto& sample = metrics_[name];
    sample.last = value_ms;
    sample.max = sample.samples == 0 ? value_ms : std::max(sample.max, value_ms);
    sample.average = ((sample.average * static_cast<double>(sample.samples)) + value_ms) /
        static_cast<double>(sample.samples + 1);
    ++sample.samples;
}

void RuntimeMetrics::record_counter(const std::string& name, std::uint64_t value) {
    std::scoped_lock lock(mutex_);
    auto& sample = metrics_[name];
    sample.last = static_cast<double>(value);
    sample.max = std::max(sample.max, sample.last);
    sample.average = sample.last;
    sample.samples = std::max<std::uint64_t>(sample.samples, 1);
}

std::unordered_map<std::string, MetricSample> RuntimeMetrics::snapshot() const {
    std::scoped_lock lock(mutex_);
    return metrics_;
}

std::string RuntimeMetrics::to_json() const {
    const auto samples = snapshot();
    std::ostringstream out;
    out << "{";
    bool first = true;
    for (const auto& [name, sample] : samples) {
        if (!first) {
            out << ",";
        }
        first = false;
        out << "\"" << name << "\":{"
            << "\"last\":" << sample.last << ","
            << "\"average\":" << sample.average << ","
            << "\"max\":" << sample.max << ","
            << "\"samples\":" << sample.samples
            << "}";
    }
    out << "}";
    return out.str();
}

std::string RuntimeMetrics::to_csv() const {
    const auto samples = snapshot();
    std::ostringstream out;
    out << "metric,last,average,max,samples\n";
    for (const auto& [name, sample] : samples) {
        out << name << ","
            << sample.last << ","
            << sample.average << ","
            << sample.max << ","
            << sample.samples << "\n";
    }
    return out.str();
}

}  // namespace arx::engine::profiling
