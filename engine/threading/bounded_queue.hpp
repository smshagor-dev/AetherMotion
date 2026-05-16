#pragma once

#include <atomic>
#include <cstddef>
#include <utility>

#include "engine/threading/spsc_ring_buffer.hpp"

namespace arx::engine::threading {

template <typename T, std::size_t Capacity>
class BoundedQueue {
public:
    bool try_push(T value) {
        if (!queue_.try_push(std::move(value))) {
            dropped_.fetch_add(1, std::memory_order_relaxed);
            return false;
        }
        return true;
    }

    bool try_pop(T& value) {
        return queue_.try_pop(value);
    }

    [[nodiscard]] std::size_t size() const noexcept {
        return queue_.size();
    }

    [[nodiscard]] constexpr std::size_t capacity() const noexcept {
        return Capacity;
    }

    [[nodiscard]] std::uint64_t dropped() const noexcept {
        return dropped_.load(std::memory_order_relaxed);
    }

private:
    SpscRingBuffer<T, Capacity> queue_;
    std::atomic<std::uint64_t> dropped_{0};
};

}  // namespace arx::engine::threading
