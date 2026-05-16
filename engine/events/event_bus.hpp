#pragma once

#include <functional>
#include <mutex>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace arx::engine {

class EventBus {
public:
    template <typename Event>
    using Handler = std::function<void(const Event&)>;

    template <typename Event>
    void subscribe(Handler<Event> handler) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto& subscribers = subscribers_[std::type_index(typeid(Event))];
        subscribers.push_back([handler = std::move(handler)](const void* raw) {
            handler(*static_cast<const Event*>(raw));
        });
    }

    template <typename Event>
    void publish(const Event& event) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = subscribers_.find(std::type_index(typeid(Event)));
        if (it == subscribers_.end()) {
            return;
        }
        for (const auto& handler : it->second) {
            handler(&event);
        }
    }

private:
    using UntypedHandler = std::function<void(const void*)>;

    std::mutex mutex_;
    std::unordered_map<std::type_index, std::vector<UntypedHandler>> subscribers_;
};

}  // namespace arx::engine
