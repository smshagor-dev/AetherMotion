#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace arx::engine::ipc {

inline constexpr std::uint32_t kProtocolVersion = 1;
inline constexpr std::uint16_t kDefaultPort = 47651;
inline constexpr std::size_t kMaxPayloadBytes = 1024 * 1024;
inline constexpr std::size_t kMaxRequestIdBytes = 64;

enum class RuntimeCommandKind : std::uint8_t {
    kUnknown = 0,
    kPing,
    kStatus,
    kPause,
    kResume,
    kShutdown,
};

struct RuntimeCommand {
    RuntimeCommandKind kind{RuntimeCommandKind::kUnknown};
    std::string name;
    std::string request_id;
    std::uint32_t version{0};
};

inline constexpr std::string_view kPingCommand =
    R"({"type":"command","version":1,"name":"ping"})";

std::vector<std::uint8_t> frame_payload(std::string_view payload);
std::string protocol_hello(std::uint16_t port);
std::string protocol_pong(std::string_view request_id = {});

[[nodiscard]] std::string_view runtime_command_name(RuntimeCommandKind kind) noexcept;
[[nodiscard]] std::optional<RuntimeCommand> parse_runtime_command(std::string_view payload);
[[nodiscard]] std::string runtime_command_payload(
    RuntimeCommandKind kind,
    std::string_view request_id = {});
[[nodiscard]] std::string protocol_command_result(
    std::string_view name,
    std::string_view status,
    std::string_view detail = {},
    std::string_view request_id = {},
    std::string_view runtime_json = {});

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
