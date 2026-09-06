#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace arx::engine::ipc {

inline constexpr std::uint32_t kProtocolVersion = 1;
inline constexpr std::uint16_t kDefaultPort = 47651;
inline constexpr std::size_t kMaxPayloadBytes = 1024 * 1024;

inline constexpr std::string_view kPingCommand =
    R"({"type":"command","version":1,"name":"ping"})";

std::vector<std::uint8_t> frame_payload(std::string_view payload);
std::string protocol_hello(std::uint16_t port);
std::string protocol_pong();

class FrameDecoder {
public:
    void append(const std::uint8_t* data, std::size_t size);
    void append(std::string_view bytes);
    [[nodiscard]] std::vector<std::string> take_frames();
    [[nodiscard]] bool protocol_error() const noexcept;
    void reset();

private:
    std::vector<std::uint8_t> buffer_;
    bool protocol_error_{false};
};

}  // namespace arx::engine::ipc
