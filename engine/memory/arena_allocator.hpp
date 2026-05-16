#pragma once

#include <cstddef>
#include <memory>
#include <stdexcept>
#include <vector>

namespace arx::engine::memory {

class ArenaAllocator {
public:
    explicit ArenaAllocator(std::size_t capacity_bytes)
        : storage_(capacity_bytes), offset_(0) {}

    void* allocate(std::size_t bytes, std::size_t alignment = alignof(std::max_align_t)) {
        std::size_t current = reinterpret_cast<std::size_t>(storage_.data()) + offset_;
        std::size_t aligned = (current + alignment - 1) & ~(alignment - 1);
        std::size_t new_offset = (aligned - reinterpret_cast<std::size_t>(storage_.data())) + bytes;
        if (new_offset > storage_.size()) {
            throw std::bad_alloc();
        }
        offset_ = new_offset;
        return reinterpret_cast<void*>(aligned);
    }

    void reset() noexcept {
        offset_ = 0;
    }

    [[nodiscard]] std::size_t capacity() const noexcept { return storage_.size(); }
    [[nodiscard]] std::size_t used() const noexcept { return offset_; }

private:
    std::vector<std::byte> storage_;
    std::size_t offset_;
};

}  // namespace arx::engine::memory
