#include "engine/telemetry/telemetry_encoder.hpp"

#include <sstream>

#include "vision/gesture_engine/gesture_types.hpp"

namespace arx::engine::telemetry {

std::string TelemetryEncoder::encode_frame(const RuntimeTelemetryFrame& frame) const {
    std::ostringstream out;
    out << "{"
        << "\"type\":\"frame_telemetry\","
        << "\"timestamp\":" << frame.frame_meta.capture_us / 1000000.0 << ","
        << "\"fps\":" << frame.frame_meta.fps << ","
        << "\"num_hands\":" << frame.num_hands << ","
        << "\"gesture\":\"" << frame.gesture << "\","
        << "\"confidence\":" << frame.confidence << ","
        << "\"source\":\"arx_runtime\","
        << "\"recording\":" << (frame.recording ? "true" : "false") << ","
        << "\"replaying\":" << (frame.replaying ? "true" : "false") << ","
        << "\"dropped_frames\":" << frame.dropped_frames << ","
        << "\"camera_fps\":" << frame.camera_fps << ","
        << "\"landmark_confidence\":" << frame.landmark_confidence << ","
        << "\"gesture_confidence\":" << frame.gesture_confidence << ","
        << "\"provider_health\":\"" << frame.provider_health << "\","
        << "\"queue\":{\"depth\":" << frame.queue_depth << ",\"capacity\":" << frame.queue_capacity << "},"
        << "\"tracking\":{\"state\":\"" << frame.tracker_state << "\","
        << "\"error\":\"" << frame.tracker_error << "\","
        << "\"model_loaded\":" << (frame.model_loaded ? "true" : "false") << ","
        << "\"raw_hand_count\":" << frame.raw_hand_count << ","
        << "\"top_hand_confidence\":" << frame.top_hand_confidence << ","
        << "\"hand_model_path\":\"" << frame.hand_model_path << "\","
        << "\"face_model_path\":\"" << frame.face_model_path << "\"},"
        << "\"perf\":{\"latency_ms\":" << frame.latency_ms
        << ",\"camera_ms\":" << frame.camera_latency_ms
        << ",\"inference_ms\":" << frame.inference_latency_ms
        << ",\"smoothing_ms\":" << frame.smoothing_latency_ms
        << ",\"gesture_ms\":" << frame.gesture_latency_ms
        << ",\"render_ms\":" << frame.render_latency_ms
        << ",\"end_to_end_latency_ms\":" << frame.end_to_end_latency_ms
        << "},\"profiler\":" << (frame.profiler_snapshot.empty() ? "{}" : frame.profiler_snapshot)
        << "}";
    return out.str();
}

std::string TelemetryEncoder::encode_gesture_event(const engine::GestureRuntimeEvent& event) const {
    std::ostringstream out;
    out << "{"
        << "\"type\":\"gesture_event\","
        << "\"event_kind\":\"" << vision::gesture_event_name(event.kind) << "\","
        << "\"gesture\":\"" << vision::gesture_name(event.gesture) << "\","
        << "\"confidence\":" << event.confidence << ","
        << "\"duration\":" << event.duration_seconds << ","
        << "\"timestamp\":" << event.timestamp_us / 1000000.0 << ","
        << "\"source\":\"arx_runtime\""
        << "}";
    return out.str();
}

void TelemetryEncoder::push_timeline(std::string entry) {
    timeline_.push_front(std::move(entry));
    while (timeline_.size() > 120) {
        timeline_.pop_back();
    }
}

const std::deque<std::string>& TelemetryEncoder::timeline() const noexcept {
    return timeline_;
}

}  // namespace arx::engine::telemetry
