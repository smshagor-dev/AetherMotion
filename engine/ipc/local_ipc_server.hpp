#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string_view>

namespace arx::engine::ipc {

class LocalIpcServer final {
public:
    using CommandHandler = std::function<void(std::string_view)>;

    LocalIpcServer();
    ~LocalIpcServer();

    LocalIpcServer(const LocalIpcServer&) = delete;
    LocalIpcServer& operator=(const LocalIpcServer&) = delete;

    bool start(std::uint16_t port);
    void stop();
    void publish(std::string_view payload);
    void set_command_handler(CommandHandler handler);

    [[nodiscard]] bool running() const noexcept;
    [[nodiscard]] std::uint16_t port() const noexcept;
    [[nodiscard]] std::uint64_t dropped_messages() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace arx::engine::ipc
