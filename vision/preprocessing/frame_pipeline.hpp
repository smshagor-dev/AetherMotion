#pragma once

#include <atomic>
#include <memory>
#include <optional>
#include <thread>
#include <vector>

#ifdef ARX_HAS_OPENCV
#include <opencv2/core.hpp>
#endif

#include "ar/renderer/render_engine.hpp"
#include "engine/ipc/shared_memory_bridge.hpp"
#include "engine/telemetry/telemetry_encoder.hpp"
#include "vision/landmarks/landmark_types.hpp"

namespace arx::vision::preprocessing {

struct PipelineConfig {
    int display_width{1280};
    int display_height{720};
    int ai_width{640};
    int ai_height{480};
    int shm_key{0x41525800};
    bool show_window{true};
};

#ifdef ARX_HAS_OPENCV
struct StagedFrame {
    cv::Mat raw;
    cv::Mat ai_rgb;
    cv::Mat render;
    FrameMetadata meta{};
    std::vector<HandLandmarks> hands;
    std::optional<FaceLandmarks> face;
    bool has_landmarks{false};
};

class FramePipeline {
public:
    explicit FramePipeline(PipelineConfig config);
    ~FramePipeline();

    void start();
    void stop();
    void push_raw(const cv::Mat& frame, const FrameMetadata& meta);

private:
    void preprocess_worker();
    void bridge_worker();
    void compose_worker();
    void display_worker();

    PipelineConfig config_;
    std::unique_ptr<ar::renderer::RenderEngine> renderer_;
    std::unique_ptr<engine::ipc::SharedMemoryBridge> bridge_;
    std::unique_ptr<engine::telemetry::TelemetryEncoder> telemetry_;

    std::vector<StagedFrame> raw_queue_;
    std::vector<StagedFrame> ai_queue_;
    std::vector<StagedFrame> compose_queue_;
    std::vector<StagedFrame> display_queue_;
    std::atomic<bool> running_{false};
    std::vector<std::thread> workers_;
};
#endif

}  // namespace arx::vision::preprocessing
