#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <new>
#include <optional>
#include <type_traits>
#include <utility>

namespace arx::engine::threading {

template <typename T, std::size_t Capacity>
class SpscRingBuffer {
    static_assert(Capacity > 1, "SpscRingBuffer capacity must be greater than 1");

public:
    SpscRingBuffer() = default;

    SpscRingBuffer(const SpscRingBuffer&) = delete;
    SpscRingBuffer& operator=(const SpscRingBuffer&) = delete;

    ~SpscRingBuffer() {
        T value;
        while (try_pop(value)) {
        }
    }

    template <typename U>
    bool try_push(U&& value) {
        const std::size_t head = head_.load(std::memory_order_relaxed);
        const std::size_t next = increment(head);
        if (next == tail_.load(std::memory_order_acquire)) {
            return false;
        }

        auto* slot = std::launder(reinterpret_cast<T*>(&storage_[head]));
        ::new (static_cast<void*>(slot)) T(std::forward<U>(value));
        head_.store(next, std::memory_order_release);
        return true;
    }

    bool try_pop(T& out) {
        const std::size_t tail = tail_.load(std::memory_order_relaxed);
        if (tail == head_.load(std::memory_order_acquire)) {
            return false;
        }

        auto* slot = std::launder(reinterpret_cast<T*>(&storage_[tail]));
        out = std::move(*slot);
        slot->~T();
        tail_.store(increment(tail), std::memory_order_release);
        return true;
    }

    [[nodiscard]] bool empty() const noexcept {
        return tail_.load(std::memory_order_acquire) == head_.load(std::memory_order_acquire);
    }

    [[nodiscard]] std::size_t size() const noexcept {
        const std::size_t head = head_.load(std::memory_order_acquire);
        const std::size_t tail = tail_.load(std::memory_order_acquire);
        return head >= tail ? (head - tail) : ((Capacity + 1) - tail + head);
    }

    [[nodiscard]] static constexpr std::size_t capacity() noexcept {
        return Capacity;
    }

private:
    using Storage = std::aligned_storage_t<sizeof(T), alignof(T)>;

    static constexpr std::size_t increment(std::size_t index) noexcept {
        return (index + 1) % (Capacity + 1);
    }

    alignas(64) std::atomic<std::size_t> head_{0};
    alignas(64) std::atomic<std::size_t> tail_{0};
    std::array<Storage, Capacity + 1> storage_{};
};

}  // namespace arx::engine::threading
