#include "engine/threading/thread_pool.hpp"

namespace arx::engine {

ThreadPool::ThreadPool() = default;

ThreadPool::~ThreadPool() {
    stop();
}

void ThreadPool::start(std::size_t worker_count, const std::string& tag) {
    if (running_.exchange(true)) {
        return;
    }

    workers_.reserve(worker_count);
    for (std::size_t i = 0; i < worker_count; ++i) {
        workers_.emplace_back([this, i, tag]() { worker_loop(i, tag); });
    }
}

void ThreadPool::enqueue(Task task) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        tasks_.push(std::move(task));
    }
    cv_.notify_one();
}

void ThreadPool::stop() {
    if (!running_.exchange(false)) {
        return;
    }

    cv_.notify_all();
    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    workers_.clear();
}

bool ThreadPool::running() const noexcept {
    return running_.load();
}

std::size_t ThreadPool::worker_count() const noexcept {
    return workers_.size();
}

void ThreadPool::worker_loop(std::size_t, const std::string&) {
    while (running_.load()) {
        Task task;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait(lock, [this]() { return !running_.load() || !tasks_.empty(); });
            if (!running_.load() && tasks_.empty()) {
                return;
            }
            task = std::move(tasks_.front());
            tasks_.pop();
        }
        if (task) {
            task();
        }
    }
}

}  // namespace arx::engine
