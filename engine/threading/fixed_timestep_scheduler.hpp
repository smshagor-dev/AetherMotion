#pragma once

#include <chrono>
#include <thread>

namespace arx::engine {

class FixedTimestepScheduler {
public:
    explicit FixedTimestepScheduler(double hz)
        : tick_interval_(std::chrono::duration<double>(1.0 / hz)) {}

    template <typename Fn>
    void run_for_ticks(std::size_t ticks, Fn&& fn) {
        auto next_tick = std::chrono::steady_clock::now();
        for (std::size_t i = 0; i < ticks; ++i) {
            fn(tick_interval_.count());
            next_tick += std::chrono::duration_cast<std::chrono::steady_clock::duration>(tick_interval_);
            std::this_thread::sleep_until(next_tick);
        }
    }

    template <typename Fn>
    void run_while(Fn&& fn) {
        auto next_tick = std::chrono::steady_clock::now();
        while (fn(tick_interval_.count())) {
            next_tick += std::chrono::duration_cast<std::chrono::steady_clock::duration>(tick_interval_);
            std::this_thread::sleep_until(next_tick);
        }
    }

private:
    std::chrono::duration<double> tick_interval_;
};

}  // namespace arx::engine
