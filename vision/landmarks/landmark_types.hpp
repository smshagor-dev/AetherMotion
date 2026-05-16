#pragma once

#include <array>
#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace arx::vision {

constexpr std::size_t kHandLandmarkCount = 21;
constexpr std::size_t kFaceLandmarkCount = 468;

struct Point2f {
    float x{0.0f};
    float y{0.0f};
};

struct Point3f {
    float x{0.0f};
    float y{0.0f};
    float z{0.0f};
};

struct FrameMetadata {
    std::uint64_t frame_id{0};
    std::int64_t capture_us{0};
    std::int64_t process_us{0};
    int camera_id{0};
    int width{0};
    int height{0};
    double fps{0.0};
    std::uint32_t dropped_frames{0};
};

struct HandLandmarks {
    std::array<Point3f, kHandLandmarkCount> points{};
    float confidence{0.0f};
    bool is_left{false};
};

struct FaceLandmarks {
    std::array<Point3f, kFaceLandmarkCount> points{};
    float confidence{0.0f};
};

struct FrameLandmarks {
    std::uint64_t frame_id{0};
    FrameMetadata meta{};
    std::vector<HandLandmarks> hands;
    std::optional<FaceLandmarks> face;
};

struct VelocityFrame {
    float vx{0.0f};
    float vy{0.0f};
    float speed{0.0f};
};

inline std::int64_t now_us() {
    using clock = std::chrono::high_resolution_clock;
    using micros = std::chrono::microseconds;
    return std::chrono::duration_cast<micros>(clock::now().time_since_epoch()).count();
}

}  // namespace arx::vision
