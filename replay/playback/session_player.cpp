#include "replay/playback/session_player.hpp"

#include <fstream>
#include <regex>

namespace arx::replay {

namespace {

std::optional<double> extract_number(const std::string& source, const std::string& key) {
    const std::regex re("\"" + key + "\":(-?[0-9]+(?:\\.[0-9]+)?)");
    std::smatch match;
    if (std::regex_search(source, match, re)) {
        return std::stod(match[1].str());
    }
    return std::nullopt;
}

std::optional<std::string> extract_string(const std::string& source, const std::string& key) {
    const std::regex re("\"" + key + "\":\"([^\"]*)\"");
    std::smatch match;
    if (std::regex_search(source, match, re)) {
        return match[1].str();
    }
    return std::nullopt;
}

}  // namespace

bool SessionPlayer::load(const std::filesystem::path& path) {
    frames_.clear();
    index_ = 0;

    std::ifstream in(path);
    if (!in.is_open()) {
        return false;
    }

    std::string line;
    while (std::getline(in, line)) {
        if (auto frame = parse_line(line); frame.has_value()) {
            frames_.push_back(*frame);
        }
    }
    return !frames_.empty();
}

std::optional<SessionFrame> SessionPlayer::next() {
    if (index_ >= frames_.size()) {
        return std::nullopt;
    }
    return frames_[index_++];
}

void SessionPlayer::reset() {
    index_ = 0;
}

bool SessionPlayer::loaded() const noexcept {
    return !frames_.empty();
}

std::optional<SessionFrame> SessionPlayer::parse_line(const std::string& line) {
    SessionFrame frame;
    frame.timestamp_us = static_cast<std::int64_t>(extract_number(line, "timestamp_us").value_or(0.0));
    frame.frame_id = static_cast<std::uint64_t>(extract_number(line, "frame_id").value_or(0.0));
    frame.fps = extract_number(line, "fps").value_or(0.0);
    frame.latency_ms = extract_number(line, "latency_ms").value_or(0.0);
    frame.dropped_frames = static_cast<std::uint64_t>(extract_number(line, "dropped_frames").value_or(0.0));
    frame.gesture.label = extract_string(line, "label").value_or("none");
    frame.gesture.confidence = static_cast<float>(extract_number(line, "confidence").value_or(0.0));
    return frame.frame_id == 0 ? std::optional<SessionFrame>{} : std::optional<SessionFrame>{frame};
}

}  // namespace arx::replay
