#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <utility>

#include "engine/threading/spsc_ring_buffer.hpp"

namespace arx::engine::threading {

template <typename T, std::size_t Capacity>
class FramePool {
public:
    class Lease {
    public:
        Lease() = default;
        Lease(FramePool* owner, std::size_t index)
            : owner_(owner), index_(index) {}

        Lease(const Lease&) = delete;
        Lease& operator=(const Lease&) = delete;

        Lease(Lease&& other) noexcept
            : owner_(other.owner_), index_(other.index_) {
            other.owner_ = nullptr;
        }

        Lease& operator=(Lease&& other) noexcept {
            if (this == &other) {
                return *this;
            }
            reset();
            owner_ = other.owner_;
            index_ = other.index_;
            other.owner_ = nullptr;
            return *this;
        }

        ~Lease() {
            reset();
        }

        T& operator*() noexcept {
            return owner_->items_[index_];
        }

        const T& operator*() const noexcept {
            return owner_->items_[index_];
        }

        T* operator->() noexcept {
            return &owner_->items_[index_];
        }

        const T* operator->() const noexcept {
            return &owner_->items_[index_];
        }

        [[nodiscard]] explicit operator bool() const noexcept {
            return owner_ != nullptr;
        }

        void reset() noexcept {
            if (owner_ != nullptr) {
                owner_->release(index_);
                owner_ = nullptr;
            }
        }

    private:
        FramePool* owner_{nullptr};
        std::size_t index_{0};
    };

    FramePool() {
        for (std::size_t index = 0; index < Capacity; ++index) {
            free_indices_.try_push(index);
        }
    }

    [[nodiscard]] std::optional<Lease> acquire() {
        std::size_t index = 0;
        if (!free_indices_.try_pop(index)) {
            return std::nullopt;
        }
        return Lease(this, index);
    }

    [[nodiscard]] std::size_t available() const noexcept {
        return free_indices_.size();
    }

private:
    void release(std::size_t index) noexcept {
        items_[index] = T{};
        free_indices_.try_push(index);
    }

    friend class Lease;

    std::array<T, Capacity> items_{};
    SpscRingBuffer<std::size_t, Capacity> free_indices_{};
};

}  // namespace arx::engine::threading
