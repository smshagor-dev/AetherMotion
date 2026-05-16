#include <filesystem>
#include <iostream>

#include "replay/exporters/csv_exporter.hpp"
#include "replay/playback/session_player.hpp"
#include "replay/recorder/session_recorder.hpp"

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
    using namespace arx::replay;
    namespace fs = std::filesystem;

    const fs::path dir = "test_sessions";
    SessionRecorder recorder(dir);
    bool ok = recorder.begin_session("sample");

    SessionFrame frame;
    frame.timestamp_us = 1000;
    frame.frame_id = 1;
    frame.fps = 60.0;
    frame.latency_ms = 12.5;
    frame.gesture.label = "pinch";
    frame.gesture.confidence = 0.9f;
    recorder.record_frame(frame);
    recorder.end_session();

    SessionPlayer player;
    ok &= expect_true(player.load(dir / "sample.jsonl"), "Session player should load recorded JSONL");
    auto replayed = player.next();
    ok &= expect_true(replayed.has_value(), "Session player should yield a replay frame");
    ok &= expect_true(replayed->frame_id == 1, "Replay frame id should round-trip");

    ok &= expect_true(CsvExporter::export_from_jsonl(dir / "sample.jsonl", dir / "sample.csv"), "CSV exporter should generate a CSV file");
    ok &= expect_true(fs::exists(dir / "sample.csv"), "CSV output file should exist");

    fs::remove_all(dir);
    return ok ? 0 : 1;
}
