#include "replay/recorder/session_recorder.hpp"

#include <iomanip>
#include <sstream>

namespace arx::replay {

namespace {

std::string encode_hand(const vision::HandLandmarks& hand) {
    std::ostringstream out;
    out << "{\"is_left\":" << (hand.is_left ? "true" : "false")
        << ",\"confidence\":" << hand.confidence << ",\"points\":[";
    for (std::size_t i = 0; i < vision::kHandLandmarkCount; ++i) {
        const auto& p = hand.points[i];
        out << "[" << p.x << "," << p.y << "," << p.z << "]";
        if (i + 1 != vision::kHandLandmarkCount) {
            out << ",";
        }
    }
    out << "]}";
    return out.str();
}

std::string encode_face(const vision::FaceLandmarks& face) {
    std::ostringstream out;
    out << "{\"confidence\":" << face.confidence << ",\"points\":[";
    for (std::size_t i = 0; i < vision::kFaceLandmarkCount; ++i) {
        const auto& p = face.points[i];
        out << "[" << p.x << "," << p.y << "," << p.z << "]";
        if (i + 1 != vision::kFaceLandmarkCount) {
            out << ",";
        }
    }
    out << "]}";
    return out.str();
}

std::string encode_fusion(const ar::fusion::ARFusionFrame& fusion) {
    std::ostringstream out;
    out << "{\"schema_version\":" << fusion.schema_version
        << ",\"runtime_version\":\"" << fusion.runtime_version
        << "\",\"provider_name\":\"" << fusion.provider_name
        << "\",\"config_hash\":\"" << fusion.config_hash
        << "\",\"gesture_label\":\"" << fusion.gesture_label
        << "\",\"gesture_confidence\":" << fusion.gesture_confidence
        << ",\"gesture_event\":\"" << fusion.gesture_event
        << "\",\"tracking_fade\":" << fusion.telemetry.tracking_fade
        << ",\"tracker_state\":\"" << fusion.telemetry.tracker_state
        << "\",\"hud\":{\"visible\":" << (fusion.hud.visible ? "true" : "false")
        << ",\"opacity\":" << fusion.hud.opacity
        << ",\"pinch_distance\":" << fusion.hud.pinch_distance
        << ",\"primary_label\":\"" << fusion.hud.primary_label << "\"}"
        << ",\"anchors\":[";
    for (std::size_t i = 0; i < fusion.anchors.size(); ++i) {
        const auto& anchor = fusion.anchors[i];
        out << "{\"id\":\"" << anchor.id
            << "\",\"active\":" << (anchor.active ? "true" : "false")
            << ",\"confidence\":" << anchor.confidence
            << ",\"position\":[" << anchor.position.x << "," << anchor.position.y << "," << anchor.position.z << "]}";
        if (i + 1 != fusion.anchors.size()) {
            out << ",";
        }
    }
    out << "]}";
    return out.str();
}

}  // namespace

SessionRecorder::SessionRecorder(std::filesystem::path session_dir)
    : session_dir_(std::move(session_dir)) {}

SessionRecorder::~SessionRecorder() {
    end_session();
}

bool SessionRecorder::begin_session(const std::string& session_id) {
    end_session();
    std::filesystem::create_directories(session_dir_);
    current_path_ = session_dir_ / (session_id + ".jsonl");
    out_.open(current_path_, std::ios::out | std::ios::trunc);
    return out_.is_open();
}

void SessionRecorder::record_frame(const SessionFrame& frame) {
    if (!out_.is_open()) {
        return;
    }

    out_ << "{\"timestamp_us\":" << frame.timestamp_us
         << ",\"frame_id\":" << frame.frame_id
         << ",\"fps\":" << std::fixed << std::setprecision(3) << frame.fps
         << ",\"latency_ms\":" << frame.latency_ms
         << ",\"dropped_frames\":" << frame.dropped_frames
         << ",\"gesture\":{\"label\":\"" << frame.gesture.label
         << "\",\"confidence\":" << frame.gesture.confidence
         << "},\"profiler\":" << (frame.profiler_snapshot.empty() ? "{}" : frame.profiler_snapshot)
         << ",\"hands\":[";
    for (std::size_t i = 0; i < frame.hands.size(); ++i) {
        out_ << encode_hand(frame.hands[i]);
        if (i + 1 != frame.hands.size()) {
            out_ << ",";
        }
    }
    out_ << "]";
    if (frame.face.has_value()) {
        out_ << ",\"face\":" << encode_face(*frame.face);
    }
    if (frame.fusion.has_value()) {
        out_ << ",\"fusion\":" << encode_fusion(*frame.fusion);
    }
    out_ << "}\n";
}

void SessionRecorder::end_session() {
    if (out_.is_open()) {
        out_.flush();
        out_.close();
    }
}

bool SessionRecorder::recording() const noexcept {
    return out_.is_open();
}

const std::filesystem::path& SessionRecorder::current_path() const noexcept {
    return current_path_;
}

}  // namespace arx::replay
