#include <iostream>

#include "engine/threading/bounded_queue.hpp"
#include "engine/threading/frame_pool.hpp"

namespace {

bool expect_true(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << '\n';
        return false;
    }
    return true;
}

}

int main() {
    using namespace arx::engine::threading;

    bool ok = true;

    BoundedQueue<int, 2> queue;
    ok &= expect_true(queue.try_push(1), "Bounded queue should accept first item");
    ok &= expect_true(queue.try_push(2), "Bounded queue should accept second item");
    ok &= expect_true(!queue.try_push(3), "Bounded queue should reject items beyond capacity");

    int value = 0;
    ok &= expect_true(queue.try_pop(value) && value == 1, "Queue should pop items in FIFO order");
    ok &= expect_true(queue.try_pop(value) && value == 2, "Queue should pop second item in FIFO order");
    ok &= expect_true(queue.dropped() == 1, "Queue should count dropped items");

    FramePool<int, 2> pool;
    auto a = pool.acquire();
    auto b = pool.acquire();
    auto c = pool.acquire();
    ok &= expect_true(a.has_value(), "Frame pool should hand out first lease");
    ok &= expect_true(b.has_value(), "Frame pool should hand out second lease");
    ok &= expect_true(!c.has_value(), "Frame pool should be exhausted at capacity");
    a->reset();
    c = pool.acquire();
    ok &= expect_true(c.has_value(), "Frame pool should recycle released leases");

    return ok ? 0 : 1;
}
