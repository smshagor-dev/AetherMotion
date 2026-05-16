#pragma once

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <functional>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

namespace arx::engine {

class ThreadPool {
public:
    using Task = std::function<void()>;

    ThreadPool();
    ~ThreadPool();

    void start(std::size_t worker_count, const std::string& tag = "arx-worker");
    void enqueue(Task task);
    void stop();

    [[nodiscard]] bool running() const noexcept;
    [[nodiscard]] std::size_t worker_count() const noexcept;

private:
    void worker_loop(std::size_t worker_index, const std::string& tag);

    std::vector<std::thread> workers_;
    std::queue<Task> tasks_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::atomic<bool> running_{false};
};

}  // namespace arx::engine
