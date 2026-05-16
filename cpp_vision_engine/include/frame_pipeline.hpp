#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// frame_pipeline.hpp
// ─────────────────────────────────────────────────────────────────────────────

#include "arx_types.hpp"

#include <atomic>
#include <memory>
#include <optional>
#include <thread>
#include <vector>

#include <opencv2/core.hpp>

namespace arx {

class RenderEngine;
class SharedMemoryBridge;
class TelemetryEncoder;

// ─── Pipeline configuration ───────────────────────────────────────────────

struct PipelineConfig {
    int    display_width      = 1280;
    int    display_height     = 720;
    int    ai_width           = 640;
    int    ai_height          = 480;
    int    shm_key            = kSharedMemKeyBase;
    int    zmq_telemetry_port = kZmqTelemetryPort;
    bool   show_window        = true;
};

// ─── Internal staged frame ────────────────────────────────────────────────

struct StagedFrame {
    cv::Mat  raw;
    cv::Mat  ai_rgb;
    cv::Mat  render;
    FrameMeta meta{};
    std::vector<HandLandmark> hands;
    std::optional<FaceLandmark> face;
    bool     has_landmarks{false};
};

// ─── Pipeline stats ───────────────────────────────────────────────────────

struct PipelineStats {
    uint64_t frames_displayed{0};
    uint64_t raw_drops{0};
    uint64_t ai_drops{0};
    uint64_t compose_drops{0};
    uint64_t display_drops{0};
};

// ─────────────────────────────────────────────────────────────────────────────

class FramePipeline {
public:
    explicit FramePipeline(const PipelineConfig& cfg);
    ~FramePipeline();

    void start();
    void stop();
    void push_raw(const cv::Mat& frame, const FrameMeta& meta);

    PipelineStats stats() const noexcept;

private:
    void preprocess_worker();
    void ai_publish_worker();
    void compose_worker();
    void display_worker();

    PipelineConfig cfg_;
    std::unique_ptr<RenderEngine>       render_engine_;
    std::unique_ptr<SharedMemoryBridge> shm_bridge_;
    std::unique_ptr<TelemetryEncoder>   telemetry_;

    // Four inter-stage lock-free ring buffers (power-of-2 capacity)
    RingBuffer<StagedFrame, 8> raw_queue_;
    RingBuffer<StagedFrame, 8> ai_queue_;
    RingBuffer<StagedFrame, 8> compose_queue_;
    RingBuffer<StagedFrame, 8> display_queue_;

    std::atomic<bool>        running_{false};
    std::vector<std::thread> workers_;
    PipelineStats            stats_;
};

}  // namespace arx
