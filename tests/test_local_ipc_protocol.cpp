#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

#include "engine/ipc/local_ipc_protocol.hpp"

namespace {

bool expect_true(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << '\n';
        return false;
    }
    return true;
}

}  // namespace

int main() {
    using namespace arx::engine::ipc;

    bool ok = true;

    const auto frame = frame_payload("hello");
    ok &= expect_true(frame.size() == 9, "framed payload should include 4-byte length prefix");
    ok &= expect_true(frame[0] == 0 && frame[1] == 0 && frame[2] == 0 && frame[3] == 5,
                      "frame length should be encoded big-endian");

    FrameDecoder partial;
    partial.append(frame.data(), 2);
    ok &= expect_true(partial.take_frames().empty(), "partial header must not decode early");
    partial.append(frame.data() + 2, frame.size() - 2);
    const auto decoded_partial = partial.take_frames();
    ok &= expect_true(decoded_partial.size() == 1 && decoded_partial.front() == "hello",
                      "decoder should reassemble fragmented frames");

    const auto one = frame_payload("one");
    const auto two = frame_payload("two");
    std::vector<std::uint8_t> combined = one;
    combined.insert(combined.end(), two.begin(), two.end());
    FrameDecoder multiple;
    multiple.append(combined.data(), combined.size());
    const auto decoded_multiple = multiple.take_frames();
    ok &= expect_true(decoded_multiple.size() == 2,
                      "decoder should return multiple frames from one receive buffer");
    ok &= expect_true(decoded_multiple.size() == 2 &&
                          decoded_multiple[0] == "one" && decoded_multiple[1] == "two",
                      "decoded frame ordering must be stable");

    const std::uint8_t oversized_header[] = {0x00, 0x10, 0x00, 0x01};
    FrameDecoder oversized;
    oversized.append(oversized_header, sizeof(oversized_header));
    (void)oversized.take_frames();
    ok &= expect_true(oversized.protocol_error(), "oversized payload length must fail closed");

    const std::string hello = protocol_hello(kDefaultPort);
    ok &= expect_true(hello.find("aethermotion.local-ipc") != std::string::npos,
                      "protocol hello should expose the protocol identity");
    ok &= expect_true(hello.find("\"version\":1") != std::string::npos,
                      "protocol hello should expose protocol version 1");
    ok &= expect_true(hello.find("\"pause\"") != std::string::npos &&
                          hello.find("\"shutdown\"") != std::string::npos,
                      "protocol hello should advertise runtime controls");
    ok &= expect_true(protocol_pong("qt-1").find("\"request_id\":\"qt-1\"") != std::string::npos,
                      "ping result should preserve request correlation id");

    const std::string pause_payload = runtime_command_payload(RuntimeCommandKind::kPause, "qt-42");
    const auto pause = parse_runtime_command(pause_payload);
    ok &= expect_true(pause.has_value(), "pause command should parse");
    ok &= expect_true(pause.has_value() && pause->kind == RuntimeCommandKind::kPause,
                      "pause command should map to pause kind");
    ok &= expect_true(pause.has_value() && pause->request_id == "qt-42",
                      "command parser should preserve request id");

    const auto reordered = parse_runtime_command(
        R"({"request_id":"desktop.9","name":"resume","type":"command","version":1})");
    ok &= expect_true(reordered.has_value() && reordered->kind == RuntimeCommandKind::kResume,
                      "command parser should not depend on field ordering");

    const auto unknown = parse_runtime_command(
        R"({"type":"command","version":1,"name":"format_disk","request_id":"qt-77"})");
    ok &= expect_true(unknown.has_value() && unknown->kind == RuntimeCommandKind::kUnknown,
                      "well-formed unsupported commands should remain distinguishable from malformed payloads");

    ok &= expect_true(!parse_runtime_command(
                          R"({"type":"command","version":2,"name":"pause"})")
                          .has_value(),
                      "commands from unsupported protocol versions should fail closed");
    ok &= expect_true(!parse_runtime_command(
                          R"({"type":"command","version":1,"name":"pause","request_id":"bad request id"})")
                          .has_value(),
                      "request ids containing unsafe characters should be rejected");

    const std::string result = protocol_command_result(
        "status",
        "ok",
        "runtime \"healthy\"",
        "qt-99",
        R"({"state":"paused","paused":true,"shutdown_requested":false,"mode":"live"})");
    ok &= expect_true(result.find("\"request_id\":\"qt-99\"") != std::string::npos,
                      "command result should include request id");
    ok &= expect_true(result.find("runtime \\\"healthy\\\"") != std::string::npos,
                      "command result detail should be JSON escaped");
    ok &= expect_true(result.find("\"runtime\":{\"state\":\"paused\"") != std::string::npos,
                      "status result should carry structured runtime state");

    return ok ? 0 : 1;
}
