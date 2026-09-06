#pragma once

#include <atomic>
#include <filesystem>
#include <string>

#include "ar/fusion/fusion_compositor.hpp"
#include "ar/interactions/interaction_system.hpp"
#include "ar/scene/scene_graph.hpp"
#include "engine/runtime/runtime_context.hpp"
#include "replay/playback/session_player.hpp"
#include "replay/recorder/session_recorder.hpp"
#include "vision/gesture_engine/gesture_engine.hpp"
#include "vision/spatial_analysis/spatial_interaction_engine.hpp"
#include "vision/tracking/model_asset_validator.hpp"

#ifdef ARX_HAS_OPENCV
#include "ar/renderer/render_engine.hpp"
#include "engine/threading/bounded_queue.hpp"
#include "engine/threading/frame_pool.hpp"
#include "vision/camera/camera_manager.hpp"
#include "vision/tracking/landmark_runtime_bridge.hpp"
#endif

namespace arx::engine {

class Application {
public:
    Application(config::RuntimeConfig config, RuntimeMode mode, std::filesystem::path session_path = {});

    bool initialize();
    int run();
    void request_shutdown();
    void request_pause(bool paused);
    [[nodiscard]] bool paused() const noexcept;
    [[nodiscard]] bool shutdown_requested() const noexcept;
    void shutdown();

private:
    bool tick(double dt_seconds);
    int run_tracker_smoke();
    int run_validate_replay();
    bool check_model_assets(bool fail_on_missing);
    void log_gesture_debug(const vision::FrameLandmarks& frame,
                           const vision::GestureEngine::Output& output) const;
    std::optional<std::string> fusion_event_name(const vision::GestureEngine::Output& output) const;
    replay::SessionFrame make_live_frame(double dt_seconds);
    std::optional<vision::tracking::ModelValidationReport> model_report_;

#ifdef ARX_HAS_OPENCV
    using LiveFramePool = threading::FramePool<vision::tracking::VideoFrame, 8>;
    using LiveFrameLease = LiveFramePool::Lease;
    using LiveFrameQueue = threading::BoundedQueue<LiveFrameLease, 8>;

    bool initialize_live_runtime();
    void shutdown_live_runtime();
    bool pull_live_frame(replay::SessionFrame& source_frame, double dt_seconds, double& camera_latency_ms, double& inference_latency_ms);
    bool run_tracker_smoke_image();
    void render_live_frame(const vision::tracking::VideoFrame& frame,
                           const vision::landmarks::LandmarkProviderOutput& tracking,
                           const vision::GestureEngine::Output& output,
                           const ar::fusion::ARFusionFrame& fusion_frame,
                           double& render_latency_ms);

    std::unique_ptr<vision::camera::CameraManager> camera_manager_;
    std::unique_ptr<vision::landmarks::LandmarkProvider> landmark_provider_;
    std::unique_ptr<LiveFramePool> frame_pool_;
    std::unique_ptr<LiveFrameQueue> frame_queue_;
    std::optional<vision::tracking::VideoFrame> last_live_frame_;
    std::unique_ptr<ar::renderer::RenderEngine> live_renderer_;
    std::uint64_t last_queue_drop_count_{0};
#endif

    RuntimeContext context_;
    vision::GestureEngine gesture_engine_;
    vision::SpatialInteractionEngine spatial_engine_;
    ar::fusion::FusionCompositor fusion_compositor_;
    ar::SceneGraph scene_;
    ar::InteractionSystem interaction_system_;
    replay::SessionRecorder recorder_;
    replay::SessionPlayer player_;
    std::optional<vision::landmarks::LandmarkProviderOutput> last_tracking_result_;
    std::filesystem::path session_path_;
    bool initialized_{false};
    std::atomic_bool shutdown_requested_{false};
    std::atomic_bool pause_requested_{false};
    bool pause_state_reported_{false};
    std::uint64_t live_frame_id_{0};
};

}  // namespace arx::engine
