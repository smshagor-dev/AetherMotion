#include "engine/ipc/local_ipc_server.hpp"

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "engine/ipc/local_ipc_protocol.hpp"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace arx::engine::ipc {

namespace {

#ifdef _WIN32
using NativeSocket = SOCKET;
constexpr NativeSocket kInvalidSocket = INVALID_SOCKET;

void close_socket(NativeSocket socket) {
    if (socket != kInvalidSocket) {
        ::closesocket(socket);
    }
}

bool set_nonblocking(NativeSocket socket) {
    u_long enabled = 1;
    return ::ioctlsocket(socket, FIONBIO, &enabled) == 0;
}

bool socket_would_block() {
    return ::WSAGetLastError() == WSAEWOULDBLOCK;
}
#else
using NativeSocket = int;
constexpr NativeSocket kInvalidSocket = -1;

void close_socket(NativeSocket socket) {
    if (socket != kInvalidSocket) {
        ::close(socket);
    }
}

bool set_nonblocking(NativeSocket socket) {
    const int flags = ::fcntl(socket, F_GETFL, 0);
    return flags >= 0 && ::fcntl(socket, F_SETFL, flags | O_NONBLOCK) == 0;
}

bool socket_would_block() {
    return errno == EAGAIN || errno == EWOULDBLOCK;
}
#endif

bool send_frame_bytes(NativeSocket socket, const std::vector<std::uint8_t>& bytes) {
    std::size_t offset = 0;
    while (offset < bytes.size()) {
#ifdef _WIN32
        const int sent = ::send(
            socket,
            reinterpret_cast<const char*>(bytes.data() + offset),
            static_cast<int>(bytes.size() - offset),
            0);
#else
#ifdef MSG_NOSIGNAL
        constexpr int flags = MSG_NOSIGNAL;
#else
        constexpr int flags = 0;
#endif
        const auto sent = ::send(
            socket,
            reinterpret_cast<const char*>(bytes.data() + offset),
            bytes.size() - offset,
            flags);
#endif
        if (sent > 0) {
            offset += static_cast<std::size_t>(sent);
            continue;
        }
        if (socket_would_block()) {
            return false;
        }
        return false;
    }
    return true;
}

}  // namespace

struct LocalIpcServer::Impl {
    struct Client {
        NativeSocket socket{kInvalidSocket};
        FrameDecoder decoder;
        bool alive{true};
    };

    static constexpr std::size_t kMaxClients = 8;
    static constexpr std::size_t kMaxQueuedMessages = 256;

    NativeSocket listener{kInvalidSocket};
    std::atomic_bool is_running{false};
    std::atomic_uint64_t dropped{0};
    std::uint16_t listen_port{0};
    std::thread worker;
    std::mutex outbound_mutex;
    std::deque<std::vector<std::uint8_t>> outbound;
    std::mutex handler_mutex;
    CommandHandler command_handler;
#ifdef _WIN32
    bool winsock_started{false};
#endif

    bool initialize_socket_stack() {
#ifdef _WIN32
        WSADATA data{};
        if (::WSAStartup(MAKEWORD(2, 2), &data) != 0) {
            return false;
        }
        winsock_started = true;
#endif
        return true;
    }

    void cleanup_socket_stack() {
#ifdef _WIN32
        if (winsock_started) {
            ::WSACleanup();
            winsock_started = false;
        }
#endif
    }

    bool open_listener(std::uint16_t port) {
        listener = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (listener == kInvalidSocket) {
            return false;
        }

        int reuse = 1;
        (void)::setsockopt(
            listener,
            SOL_SOCKET,
            SO_REUSEADDR,
            reinterpret_cast<const char*>(&reuse),
            static_cast<int>(sizeof(reuse)));

        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_port = htons(port);
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

        if (::bind(listener, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) != 0) {
            close_socket(listener);
            listener = kInvalidSocket;
            return false;
        }
        if (::listen(listener, static_cast<int>(kMaxClients)) != 0) {
            close_socket(listener);
            listener = kInvalidSocket;
            return false;
        }
        if (!set_nonblocking(listener)) {
            close_socket(listener);
            listener = kInvalidSocket;
            return false;
        }

        listen_port = port;
        return true;
    }

    void accept_clients(std::vector<Client>& clients) {
        if (listener == kInvalidSocket) {
            return;
        }

        for (;;) {
            sockaddr_in peer{};
#ifdef _WIN32
            int peer_size = sizeof(peer);
#else
            socklen_t peer_size = sizeof(peer);
#endif
            const auto client_socket = ::accept(
                listener,
                reinterpret_cast<sockaddr*>(&peer),
                &peer_size);

            if (client_socket == kInvalidSocket) {
                if (socket_would_block()) {
                    return;
                }
                return;
            }

            if (clients.size() >= kMaxClients || !set_nonblocking(client_socket)) {
                close_socket(client_socket);
                continue;
            }

            auto hello = frame_payload(protocol_hello(listen_port));
            if (hello.empty() || !send_frame_bytes(client_socket, hello)) {
                close_socket(client_socket);
                continue;
            }

            clients.push_back(Client{client_socket, {}, true});
        }
    }

    void read_commands(std::vector<Client>& clients) {
        CommandHandler handler;
        {
            std::lock_guard lock(handler_mutex);
            handler = command_handler;
        }

        std::uint8_t buffer[4096]{};
        for (auto& client : clients) {
            if (!client.alive) {
                continue;
            }

            for (;;) {
#ifdef _WIN32
                const int received = ::recv(
                    client.socket,
                    reinterpret_cast<char*>(buffer),
                    static_cast<int>(sizeof(buffer)),
                    0);
#else
                const auto received = ::recv(client.socket, buffer, sizeof(buffer), 0);
#endif
                if (received > 0) {
                    client.decoder.append(buffer, static_cast<std::size_t>(received));
                    const auto frames = client.decoder.take_frames();
                    if (client.decoder.protocol_error()) {
                        client.alive = false;
                        break;
                    }
                    if (handler) {
                        for (const auto& frame : frames) {
                            handler(frame);
                        }
                    }
                    continue;
                }

                if (received == 0) {
                    client.alive = false;
                    break;
                }
                if (socket_would_block()) {
                    break;
                }
                client.alive = false;
                break;
            }
        }
    }

    std::deque<std::vector<std::uint8_t>> take_outbound() {
        std::deque<std::vector<std::uint8_t>> messages;
        std::lock_guard lock(outbound_mutex);
        messages.swap(outbound);
        return messages;
    }

    void send_outbound(std::vector<Client>& clients) {
        auto messages = take_outbound();
        if (messages.empty() || clients.empty()) {
            return;
        }

        for (const auto& message : messages) {
            for (auto& client : clients) {
                if (client.alive && !send_frame_bytes(client.socket, message)) {
                    client.alive = false;
                }
            }
        }
    }

    static void remove_dead_clients(std::vector<Client>& clients) {
        auto write = clients.begin();
        for (auto read = clients.begin(); read != clients.end(); ++read) {
            if (!read->alive) {
                close_socket(read->socket);
                continue;
            }
            if (write != read) {
                *write = std::move(*read);
            }
            ++write;
        }
        clients.erase(write, clients.end());
    }

    void run() {
        std::vector<Client> clients;
        clients.reserve(kMaxClients);

        while (is_running.load(std::memory_order_acquire)) {
            accept_clients(clients);
            read_commands(clients);
            send_outbound(clients);
            remove_dead_clients(clients);
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }

        for (auto& client : clients) {
            close_socket(client.socket);
        }
    }
};

LocalIpcServer::LocalIpcServer() : impl_(std::make_unique<Impl>()) {}

LocalIpcServer::~LocalIpcServer() {
    stop();
}

bool LocalIpcServer::start(std::uint16_t port) {
    if (port == 0) {
        return false;
    }
    if (impl_->is_running.load(std::memory_order_acquire)) {
        return impl_->listen_port == port;
    }
    if (!impl_->initialize_socket_stack()) {
        return false;
    }
    if (!impl_->open_listener(port)) {
        impl_->cleanup_socket_stack();
        return false;
    }

    impl_->is_running.store(true, std::memory_order_release);
    try {
        impl_->worker = std::thread([this] { impl_->run(); });
    } catch (...) {
        impl_->is_running.store(false, std::memory_order_release);
        close_socket(impl_->listener);
        impl_->listener = kInvalidSocket;
        impl_->cleanup_socket_stack();
        return false;
    }
    return true;
}

void LocalIpcServer::stop() {
    if (!impl_->is_running.exchange(false, std::memory_order_acq_rel)) {
        return;
    }

    close_socket(impl_->listener);
    impl_->listener = kInvalidSocket;
    if (impl_->worker.joinable()) {
        impl_->worker.join();
    }
    {
        std::lock_guard lock(impl_->outbound_mutex);
        impl_->outbound.clear();
    }
    impl_->listen_port = 0;
    impl_->cleanup_socket_stack();
}

void LocalIpcServer::publish(std::string_view payload) {
    auto framed = frame_payload(payload);
    if (framed.empty() || !impl_->is_running.load(std::memory_order_acquire)) {
        return;
    }

    std::lock_guard lock(impl_->outbound_mutex);
    if (impl_->outbound.size() >= Impl::kMaxQueuedMessages) {
        impl_->outbound.pop_front();
        impl_->dropped.fetch_add(1, std::memory_order_relaxed);
    }
    impl_->outbound.push_back(std::move(framed));
}

void LocalIpcServer::set_command_handler(CommandHandler handler) {
    std::lock_guard lock(impl_->handler_mutex);
    impl_->command_handler = std::move(handler);
}

bool LocalIpcServer::running() const noexcept {
    return impl_->is_running.load(std::memory_order_acquire);
}

std::uint16_t LocalIpcServer::port() const noexcept {
    return impl_->listen_port;
}

std::uint64_t LocalIpcServer::dropped_messages() const noexcept {
    return impl_->dropped.load(std::memory_order_relaxed);
}

}  // namespace arx::engine::ipc
