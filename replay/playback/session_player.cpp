#include "replay/playback/session_player.hpp"

#include <cctype>
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

std::vector<vision::Point3f> extract_points(const std::string& section) {
    std::vector<vision::Point3f> points;
    const std::regex point_re("\\[(-?[0-9]+(?:\\.[0-9]+)?),(-?[0-9]+(?:\\.[0-9]+)?),(-?[0-9]+(?:\\.[0-9]+)?)\\]");
    auto begin = std::sregex_iterator(section.begin(), section.end(), point_re);
    auto end = std::sregex_iterator();
    for (auto it = begin; it != end; ++it) {
        points.push_back({
            std::stof((*it)[1].str()),
            std::stof((*it)[2].str()),
            std::stof((*it)[3].str())
        });
    }
    return points;
}

std::optional<std::string> extract_object(const std::string& source, const std::string& key) {
    const std::string needle = "\"" + key + "\":";
    const auto start = source.find(needle);
    if (start == std::string::npos) {
        return std::nullopt;
    }
    auto pos = start + needle.size();
    while (pos < source.size() && std::isspace(static_cast<unsigned char>(source[pos]))) {
        ++pos;
    }
    if (pos >= source.size() || source[pos] != '{') {
        return std::nullopt;
    }
    int depth = 0;
    const auto object_start = pos;
    for (; pos < source.size(); ++pos) {
        if (source[pos] == '{') {
            ++depth;
        } else if (source[pos] == '}') {
            --depth;
            if (depth == 0) {
                return source.substr(object_start, pos - object_start + 1);
            }
        }
    }
    return std::nullopt;
}

std::optional<std::string> extract_array(const std::string& source, const std::string& key) {
    const std::string needle = "\"" + key + "\":";
    const auto start = source.find(needle);
    if (start == std::string::npos) {
        return std::nullopt;
    }
    auto pos = start + needle.size();
    while (pos < source.size() && std::isspace(static_cast<unsigned char>(source[pos]))) {
        ++pos;
    }
    if (pos >= source.size() || source[pos] != '[') {
        return std::nullopt;
    }
    int depth = 0;
    const auto array_start = pos;
    for (; pos < source.size(); ++pos) {
        if (source[pos] == '[') {
            ++depth;
        } else if (source[pos] == ']') {
            --depth;
            if (depth == 0) {
                return source.substr(array_start, pos - array_start + 1);
            }
        }
    }
    return std::nullopt;
}

std::vector<std::string> extract_objects_from_array(const std::string& array_text) {
    std::vector<std::string> objects;
    int depth = 0;
    std::size_t object_start = std::string::npos;
    for (std::size_t i = 0; i < array_text.size(); ++i) {
        if (array_text[i] == '{') {
            if (depth == 0) {
                object_start = i;
            }
            ++depth;
        } else if (array_text[i] == '}') {
            --depth;
            if (depth == 0 && object_start != std::string::npos) {
                objects.push_back(array_text.substr(object_start, i - object_start + 1));
                object_start = std::string::npos;
            }
        }
    }
    return objects;
}

std::vector<vision::HandLandmarks> extract_hands(const std::string& source) {
    std::vector<vision::HandLandmarks> hands;
    const auto hands_array = extract_array(source, "hands");
    if (!hands_array.has_value()) {
        return hands;
    }
    for (const auto& hand_text : extract_objects_from_array(*hands_array)) {
        vision::HandLandmarks hand;
        hand.is_left = hand_text.find("\"is_left\":true") != std::string::npos;
        hand.confidence = static_cast<float>(extract_number(hand_text, "confidence").value_or(0.0));
        const auto points = extract_points(hand_text);
        if (points.size() == vision::kHandLandmarkCount) {
            for (std::size_t i = 0; i < vision::kHandLandmarkCount; ++i) {
                hand.points[i] = points[i];
            }
            hands.push_back(hand);
        }
    }
    return hands;
}

std::optional<vision::FaceLandmarks> extract_face(const std::string& source) {
    const auto object = extract_object(source, "face");
    if (!object.has_value()) {
        return std::nullopt;
    }
    const auto points = extract_points(*object);
    if (points.size() != vision::kFaceLandmarkCount) {
        return std::nullopt;
    }
    vision::FaceLandmarks face;
    face.confidence = static_cast<float>(extract_number(*object, "confidence").value_or(0.0));
    for (std::size_t i = 0; i < vision::kFaceLandmarkCount; ++i) {
        face.points[i] = points[i];
    }
    return face;
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
    frame.hands = extract_hands(line);
    frame.face = extract_face(line);
    if (const auto fusion = extract_object(line, "fusion"); fusion.has_value()) {
        ar::fusion::ARFusionFrame fusion_frame;
        fusion_frame.schema_version = static_cast<std::uint32_t>(extract_number(*fusion, "schema_version").value_or(1.0));
        fusion_frame.runtime_version = extract_string(*fusion, "runtime_version").value_or("3.0");
        fusion_frame.provider_name = extract_string(*fusion, "provider_name").value_or("legacy");
        fusion_frame.config_hash = extract_string(*fusion, "config_hash").value_or("");
        fusion_frame.meta.frame_id = frame.frame_id;
        fusion_frame.meta.capture_us = frame.timestamp_us;
        fusion_frame.gesture_label = extract_string(*fusion, "gesture_label").value_or(frame.gesture.label);
        fusion_frame.gesture_confidence = static_cast<float>(extract_number(*fusion, "gesture_confidence").value_or(frame.gesture.confidence));
        fusion_frame.gesture_event = extract_string(*fusion, "gesture_event").value_or("none");
        fusion_frame.telemetry.tracking_fade = static_cast<float>(extract_number(*fusion, "tracking_fade").value_or(0.0));
        fusion_frame.telemetry.tracker_state = extract_string(*fusion, "tracker_state").value_or("offline");
        if (const auto hud = extract_object(*fusion, "hud"); hud.has_value()) {
            fusion_frame.hud.visible = hud->find("\"visible\":true") != std::string::npos;
            fusion_frame.hud.opacity = static_cast<float>(extract_number(*hud, "opacity").value_or(0.0));
            fusion_frame.hud.pinch_distance = static_cast<float>(extract_number(*hud, "pinch_distance").value_or(0.0));
            fusion_frame.hud.primary_label = extract_string(*hud, "primary_label").value_or("idle");
        }
        fusion_frame.raw_hands = frame.hands;
        fusion_frame.smoothed_hands = frame.hands;
        fusion_frame.face = frame.face;
        frame.fusion = fusion_frame;
    }
    return frame.frame_id == 0 ? std::optional<SessionFrame>{} : std::optional<SessionFrame>{frame};
}

}  // namespace arx::replay
