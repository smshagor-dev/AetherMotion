#include "engine/ipc/local_ipc_protocol.hpp"

#include <sstream>

namespace arx::engine::ipc {

std::vector<std::uint8_t> frame_payload(std::string_view payload) {
    if (payload.empty() || payload.size() > kMaxPayloadBytes) {
        return {};
    }

    const auto length = static_cast<std::uint32_t>(payload.size());
    std::vector<std::uint8_t> framed;
    framed.reserve(4 + payload.size());
    framed.push_back(static_cast<std::uint8_t>((length >> 24U) & 0xFFU));
    framed.push_back(static_cast<std::uint8_t>((length >> 16U) & 0xFFU));
    framed.push_back(static_cast<std::uint8_t>((length >> 8U) & 0xFFU));
    framed.push_back(static_cast<std::uint8_t>(length & 0xFFU));
    framed.insert(framed.end(), payload.begin(), payload.end());
    return framed;
}

std::string protocol_hello(std::uint16_t port) {
    std::ostringstream out;
    out << "{"
        << "\"type\":\"protocol\","
        << "\"name\":\"aethermotion.local-ipc\","
        << "\"version\":" << kProtocolVersion << ","
        << "\"transport\":\"tcp-loopback-framed\","
        << "\"host\":\"127.0.0.1\","
        << "\"port\":" << port << ","
        << "\"max_payload_bytes\":" << kMaxPayloadBytes
        << "}";
    return out.str();
}

std::string protocol_pong() {
    std::ostringstream out;
    out << "{"
        << "\"type\":\"command_result\","
        << "\"version\":" << kProtocolVersion << ","
        << "\"name\":\"ping\","
        << "\"status\":\"ok\""
        << "}";
    return out.str();
}

void FrameDecoder::append(const std::uint8_t* data, std::size_t size) {
    if (data == nullptr || size == 0 || protocol_error_) {
        return;
    }
    buffer_.insert(buffer_.end(), data, data + size);
}

void FrameDecoder::append(std::string_view bytes) {
    append(reinterpret_cast<const std::uint8_t*>(bytes.data()), bytes.size());
}

std::vector<std::string> FrameDecoder::take_frames() {
    std::vector<std::string> frames;
    std::size_t offset = 0;

    while (buffer_.size() - offset >= 4) {
        const auto length =
            (static_cast<std::uint32_t>(buffer_[offset]) << 24U) |
            (static_cast<std::uint32_t>(buffer_[offset + 1]) << 16U) |
            (static_cast<std::uint32_t>(buffer_[offset + 2]) << 8U) |
            static_cast<std::uint32_t>(buffer_[offset + 3]);

        if (length == 0 || length > kMaxPayloadBytes) {
            protocol_error_ = true;
            buffer_.clear();
            return frames;
        }

        const auto frame_size = static_cast<std::size_t>(4) + length;
        if (buffer_.size() - offset < frame_size) {
            break;
        }

        const auto* payload = reinterpret_cast<const char*>(buffer_.data() + offset + 4);
        frames.emplace_back(payload, length);
        offset += frame_size;
    }

    if (offset > 0) {
        buffer_.erase(buffer_.begin(), buffer_.begin() + static_cast<std::ptrdiff_t>(offset));
    }
    return frames;
}

bool FrameDecoder::protocol_error() const noexcept {
    return protocol_error_;
}

void FrameDecoder::reset() {
    buffer_.clear();
    protocol_error_ = false;
}

}  // namespace arx::engine::ipc
