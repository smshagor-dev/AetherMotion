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
    out_ << "]}\n";
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
