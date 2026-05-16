#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// arx_types.hpp  –  Shared types, constants, and data structures for the ARX
//                    real-time vision engine.
// ─────────────────────────────────────────────────────────────────────────────

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace arx {

// ─── Compile-time constants ───────────────────────────────────────────────
constexpr int    kMaxCameras         = 4;
constexpr int    kFrameQueueCapacity = 8;
constexpr int    kTargetFPS          = 60;
constexpr double kTargetFrameMs      = 1000.0 / kTargetFPS;
constexpr int    kSharedMemKeyBase   = 0x4152'5800;  // 'ARX\0'
constexpr int    kZmqPubPort         = 5555;
constexpr int    kZmqTelemetryPort   = 5556;

// ─── Timestamp type ───────────────────────────────────────────────────────
using Clock     = std::chrono::high_resolution_clock;
using TimePoint = Clock::time_point;
using Micros    = std::chrono::microseconds;

inline int64_t now_us() {
    return std::chrono::duration_cast<Micros>(Clock::now().time_since_epoch()).count();
}

// ─── 2-D / 3-D point types ───────────────────────────────────────────────
struct Point2f { float x, y; };
struct Point3f { float x, y, z; };

// ─── Landmark structures ─────────────────────────────────────────────────
struct HandLandmark {
    std::array<Point3f, 21> points;   // MediaPipe 21-point hand model
    float                    confidence{0.f};
    bool                     is_left{false};
};

struct FaceLandmark {
    std::array<Point3f, 468> points;  // MediaPipe face mesh
    float                    confidence{0.f};
};

// ─── Gesture event ───────────────────────────────────────────────────────
enum class GestureType : uint8_t {
    kNone         = 0,
    kOpenHand     = 1,
    kClosedFist   = 2,
    kPinch        = 3,
    kSwipeLeft    = 4,
    kSwipeRight   = 5,
    kTwoHand      = 6,
    kRotate       = 7,
    kZoom         = 8,
    kPointUp      = 9,
    kVSign        = 10,
};

struct GestureEvent {
    GestureType type{GestureType::kNone};
    float       confidence{0.f};
    int64_t     timestamp_us{0};
    Point2f     origin{};
};

// ─── Frame metadata ──────────────────────────────────────────────────────
struct FrameMeta {
    int64_t  frame_id{0};
    int64_t  capture_us{0};
    int64_t  process_us{0};
    int      camera_id{0};
    int      width{0};
    int      height{0};
    double   fps{0.0};
    uint32_t dropped_frames{0};
};

// ─── Telemetry packet (sent to Go control plane) ─────────────────────────
struct TelemetryPacket {
    FrameMeta                 frame_meta;
    std::vector<HandLandmark> hands;
    std::optional<FaceLandmark> face;
    GestureEvent              gesture;
    float                     cpu_usage_pct{0.f};
    float                     gpu_usage_pct{0.f};
    float                     pipeline_latency_ms{0.f};
};

// ─── Ring-buffer frame queue (lock-free via atomic index) ─────────────────
template<typename T, std::size_t N>
class RingBuffer {
    static_assert((N & (N - 1)) == 0, "N must be a power of 2");
public:
    bool push(T item) {
        const auto head = head_.load(std::memory_order_relaxed);
        const auto next = (head + 1) & mask_;
        if (next == tail_.load(std::memory_order_acquire)) return false;  // full
        data_[head] = std::move(item);
        head_.store(next, std::memory_order_release);
        return true;
    }

    bool pop(T& out) {
        const auto tail = tail_.load(std::memory_order_relaxed);
        if (tail == head_.load(std::memory_order_acquire)) return false;  // empty
        out = std::move(data_[tail]);
        tail_.store((tail + 1) & mask_, std::memory_order_release);
        return true;
    }

    bool empty() const {
        return tail_.load(std::memory_order_acquire) ==
               head_.load(std::memory_order_acquire);
    }

private:
    static constexpr std::size_t mask_ = N - 1;
    std::array<T, N>             data_{};
    std::atomic<std::size_t>     head_{0};
    std::atomic<std::size_t>     tail_{0};
};

}  // namespace arx
