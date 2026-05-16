#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace arx::engine::ipc {

enum class Channel : std::uint8_t {
    kFrames = 0,
    kLandmarks = 1,
    kGestures = 2,
    kTelemetry = 3,
    kReplay = 4,
    kDiagnostics = 5,
};

struct PacketHeader {
    std::uint32_t magic{0x41525833};
    std::uint16_t version{300};
    std::uint16_t payload_type{0};
    std::uint64_t sequence{0};
    std::uint64_t timestamp_us{0};
};

struct Packet {
    Channel channel{Channel::kTelemetry};
    PacketHeader header{};
    std::vector<std::byte> payload;
    std::string source;
};

}  // namespace arx::engine::ipc
