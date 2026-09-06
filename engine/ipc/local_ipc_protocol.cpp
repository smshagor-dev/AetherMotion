#include "engine/ipc/local_ipc_protocol.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace arx::engine::ipc {

namespace {

std::optional<std::string> extract_json_string(std::string_view json, std::string_view key) {
    const std::string token = "\"" + std::string(key) + "\"";
    const auto key_pos = json.find(token);
    if (key_pos == std::string_view::npos) {
        return std::nullopt;
    }

    auto cursor = json.find(':', key_pos + token.size());
    if (cursor == std::string_view::npos) {
        return std::nullopt;
    }
    ++cursor;
    while (cursor < json.size() && std::isspace(static_cast<unsigned char>(json[cursor])) != 0) {
        ++cursor;
    }
    if (cursor >= json.size() || json[cursor] != '"') {
        return std::nullopt;
    }
    ++cursor;

    std::string value;
    bool escaped = false;
    for (; cursor < json.size(); ++cursor) {
        const char ch = json[cursor];
        if (escaped) {
            switch (ch) {
            case '"': value.push_back('"'); break;
            case '\\': value.push_back('\\'); break;
            case 'n': value.push_back('\n'); break;
            case 'r': value.push_back('\r'); break;
            case 't': value.push_back('\t'); break;
            default: return std::nullopt;
            }
            escaped = false;
            continue;
        }
        if (ch == '\\') {
            escaped = true;
            continue;
        }
        if (ch == '"') {
            return value;
        }
        value.push_back(ch);
    }
    return std::nullopt;
}

std::optional<std::uint32_t> extract_json_uint(std::string_view json, std::string_view key) {
    const std::string token = "\"" + std::string(key) + "\"";
    const auto key_pos = json.find(token);
    if (key_pos == std::string_view::npos) {
        return std::nullopt;
    }

    auto cursor = json.find(':', key_pos + token.size());
    if (cursor == std::string_view::npos) {
        return std::nullopt;
    }
    ++cursor;
    while (cursor < json.size() && std::isspace(static_cast<unsigned char>(json[cursor])) != 0) {
        ++cursor;
    }
    if (cursor >= json.size() || !std::isdigit(static_cast<unsigned char>(json[cursor]))) {
        return std::nullopt;
    }

    std::uint64_t value = 0;
    while (cursor < json.size() && std::isdigit(static_cast<unsigned char>(json[cursor])) != 0) {
        value = value * 10U + static_cast<unsigned>(json[cursor] - '0');
        if (value > static_cast<std::uint64_t>(UINT32_MAX)) {
            return std::nullopt;
        }
        ++cursor;
    }
    return static_cast<std::uint32_t>(value);
}

bool valid_request_id(std::string_view value) {
    if (value.size() > kMaxRequestIdBytes) {
        return false;
    }
    return std::all_of(value.begin(), value.end(), [](char ch) {
        const auto c = static_cast<unsigned char>(ch);
        return std::isalnum(c) != 0 || ch == '-' || ch == '_' || ch == '.' || ch == ':';
    });
}

std::string json_escape(std::string_view value) {
    std::string escaped;
    escaped.reserve(value.size());
    for (const char ch : value) {
        switch (ch) {
        case '"': escaped += "\\\""; break;
        case '\\': escaped += "\\\\"; break;
        case '\n': escaped += "\\n"; break;
        case '\r': escaped += "\\r"; break;
        case '\t': escaped += "\\t"; break;
        default:
            if (static_cast<unsigned char>(ch) >= 0x20U) {
                escaped.push_back(ch);
            }
            break;
        }
    }
    return escaped;
}

RuntimeCommandKind command_kind_from_name(std::string_view name) {
    if (name == "ping") return RuntimeCommandKind::kPing;
    if (name == "status") return RuntimeCommandKind::kStatus;
    if (name == "pause") return RuntimeCommandKind::kPause;
    if (name == "resume") return RuntimeCommandKind::kResume;
    if (name == "shutdown") return RuntimeCommandKind::kShutdown;
    return RuntimeCommandKind::kUnknown;
}

}  // namespace

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
        << "\"max_payload_bytes\":" << kMaxPayloadBytes << ","
        << "\"commands\":[\"ping\",\"status\",\"pause\",\"resume\",\"shutdown\"]"
        << "}";
    return out.str();
}

std::string protocol_pong(std::string_view request_id) {
    return protocol_command_result("ping", "ok", {}, request_id);
}

std::string_view runtime_command_name(RuntimeCommandKind kind) noexcept {
    switch (kind) {
    case RuntimeCommandKind::kPing: return "ping";
    case RuntimeCommandKind::kStatus: return "status";
    case RuntimeCommandKind::kPause: return "pause";
    case RuntimeCommandKind::kResume: return "resume";
    case RuntimeCommandKind::kShutdown: return "shutdown";
    case RuntimeCommandKind::kUnknown:
    default:
        return "unknown";
    }
}

std::optional<RuntimeCommand> parse_runtime_command(std::string_view payload) {
    if (payload.empty() || payload.size() > kMaxPayloadBytes) {
        return std::nullopt;
    }

    const auto type = extract_json_string(payload, "type");
    const auto version = extract_json_uint(payload, "version");
    const auto name = extract_json_string(payload, "name");
    if (!type.has_value() || *type != "command" || !version.has_value() ||
        *version != kProtocolVersion || !name.has_value() || name->empty()) {
        return std::nullopt;
    }

    std::string request_id;
    if (const auto extracted = extract_json_string(payload, "request_id"); extracted.has_value()) {
        if (!valid_request_id(*extracted)) {
            return std::nullopt;
        }
        request_id = *extracted;
    }

    return RuntimeCommand{
        command_kind_from_name(*name),
        *name,
        std::move(request_id),
        *version,
    };
}

std::string runtime_command_payload(RuntimeCommandKind kind, std::string_view request_id) {
    const auto name = runtime_command_name(kind);
    if (kind == RuntimeCommandKind::kUnknown || (!request_id.empty() && !valid_request_id(request_id))) {
        return {};
    }

    std::ostringstream out;
    out << "{"
        << "\"type\":\"command\","
        << "\"version\":" << kProtocolVersion << ","
        << "\"name\":\"" << name << "\"";
    if (!request_id.empty()) {
        out << ",\"request_id\":\"" << json_escape(request_id) << "\"";
    }
    out << "}";
    return out.str();
}

std::string protocol_command_result(
    std::string_view name,
    std::string_view status,
    std::string_view detail,
    std::string_view request_id,
    std::string_view runtime_json) {
    std::ostringstream out;
    out << "{"
        << "\"type\":\"command_result\","
        << "\"version\":" << kProtocolVersion << ","
        << "\"name\":\"" << json_escape(name) << "\","
        << "\"status\":\"" << json_escape(status) << "\"";
    if (!request_id.empty() && valid_request_id(request_id)) {
        out << ",\"request_id\":\"" << json_escape(request_id) << "\"";
    }
    if (!detail.empty()) {
        out << ",\"detail\":\"" << json_escape(detail) << "\"";
    }
    if (!runtime_json.empty()) {
        out << ",\"runtime\":" << runtime_json;
    }
    out << "}";
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
