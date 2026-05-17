#include <filesystem>
#include <fstream>
#include <iostream>

#include "engine/config/runtime_config.hpp"

namespace {

bool expect_true(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << '\n';
        return false;
    }
    return true;
}

}

int main() {
    using namespace arx::engine::config;
    bool ok = true;

    RuntimeConfig cfg;
    cfg.camera.source = "rtsp://example/stream";
    cfg.fusion.provider_type = "mediapipe-production";
    const auto valid = validate_runtime_config(cfg);
    ok &= expect_true(valid.valid(), "Production config should validate with sane defaults");

    RuntimeConfig bad = cfg;
    bad.camera.source.clear();
    bad.camera.width = 0;
    bad.fusion.max_latency_ms = -1.0;
    bad.fusion.provider_type = "unknown";
    const auto invalid = validate_runtime_config(bad);
    ok &= expect_true(!invalid.valid(), "Invalid production config should be rejected");
    ok &= expect_true(invalid.summary().find("camera source") != std::string::npos, "Config validation summary should include camera source error");

    {
        std::ofstream out("test_graphical_fusion_config.json");
        out << "{"
            << "\"source\":\"0\","
            << "\"open_timeout_ms\":2500,"
            << "\"reconnect_delay_ms\":800,"
            << "\"allow_reconnect\":true,"
            << "\"provider_type\":\"mediapipe-production\","
            << "\"max_latency_ms\":30.0,"
            << "\"telemetry_policy\":\"jsonl+timeline\""
            << "}";
    }
    const auto loaded = load_runtime_config("test_graphical_fusion_config.json");
    ok &= expect_true(loaded.camera.source == "0", "Config loader should parse camera source");
    ok &= expect_true(loaded.camera.open_timeout_ms == 2500, "Config loader should parse camera timeout");
    ok &= expect_true(loaded.camera.reconnect_delay_ms == 800, "Config loader should parse reconnect delay");
    ok &= expect_true(loaded.fusion.provider_type == "mediapipe-production", "Config loader should parse provider type");
    ok &= expect_true(loaded.fusion.max_latency_ms > 29.0 && loaded.fusion.max_latency_ms < 31.0, "Config loader should parse max latency");
    std::filesystem::remove("test_graphical_fusion_config.json");
    return ok ? 0 : 1;
}
