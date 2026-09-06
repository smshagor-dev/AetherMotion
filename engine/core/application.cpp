#include "engine/core/application.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
#include <sstream>
#include <thread>

#include "engine/events/gesture_events.hpp"
#include "engine/events/runtime_events.hpp"
#include "engine/threading/fixed_timestep_scheduler.hpp"
#include "vision/landmarks/landmark_provider.hpp"
#include "vision/tracking/model_asset_validator.hpp"

#ifdef ARX_HAS_OPENCV
#include <opencv2/highgui.hpp>
#include <opencv2/imgcodecs.hpp>
#endif

namespace arx::engine {

namespace {

std::string session_name() {
    const std::time_t now = std::time(nullptr);
    std::tm tm_now{};
#ifdef _WIN32
    localtime_s(&tm_now, &now);
#else
    localtime_r(&now, &tm_now);
#endif
    std::ostringstream out;
    out << "session_" << std::put_time(&tm_now, "%Y%m%d_%H%M%S");
    return out.str();
}

void runtime_log(const char* stage, const std::string& message) {
    std::clog << "[ARX][" << stage << "] " << message << '\n';
}

}  // namespace

Application::Application(config::RuntimeConfig config, RuntimeMode mode, std::filesystem::path session_path)
    : gesture_engine_(&context_.event_bus),
      fusion_compositor_(config.fusion),
      recorder_(config.recording.session_dir),
      session_path_(std::move(session_path)) {
    context_.config = std::move(config);
    context_.mode = mode;
    gesture_engine_.configure({
        context_.config.tracking.gesture_threshold,
        context_.config.tracking.disable_debounce
    });
}

bool Application::initialize() {
    context_.worker_pool.start(std::max(1u, std::thread::hardware_concurrency() > 1 ? std::thread::hardware_concurrency() - 1 : 1u));
    (void)check_model_assets(false);

    context_.event_bus.subscribe<GestureRuntimeEvent>([this](const GestureRuntimeEvent& event) {
        context_.current_gesture = vision::gesture_name(event.gesture);
        context_.telemetry.push_timeline(context_.telemetry.encode_gesture_event(event));
        runtime_log("gesture",
            std::string(vision::gesture_event_name(event.kind)) + " " +
            vision::gesture_name(event.gesture) + " conf=" + std::to_string(event.confidence));
    });

    scene_.add(ar::ARObject{1, ar::ObjectType::kCube, "PrimaryCube"});
    scene_.add(ar::ARObject{2, ar::ObjectType::kHudCard, "LatencyHud"});

    if (context_.mode == RuntimeMode::kRecord || context_.mode == RuntimeMode::kGraphicalFusion) {
        context_.recording = recorder_.begin_session(session_name());
    }

    if (context_.mode == RuntimeMode::kTrackerSmoke && !context_.config.tracking.image_path.empty()) {
#ifdef ARX_HAS_OPENCV
        return true;
#else
        context_.health_monitor.report({"tracking", false, "tracker-smoke image mode requires OpenCV support"});
        return false;
#endif
    }

    if (context_.mode != RuntimeMode::kReplay &&
        context_.mode != RuntimeMode::kFusionReplay &&
        context_.mode != RuntimeMode::kValidateReplay) {
#ifdef ARX_HAS_OPENCV
        if (!initialize_live_runtime()) {
            return false;
        }
#else
        context_.health_monitor.report({"camera", false, "live runtime built without OpenCV camera support"});
        return false;
#endif
    }

    if (context_.mode == RuntimeMode::kReplay ||
        context_.mode == RuntimeMode::kFusionReplay ||
        context_.mode == RuntimeMode::kValidateReplay) {
        context_.replaying = player_.load(session_path_);
        if (!context_.replaying) {
            context_.health_monitor.report({"replay", false, "failed to load session"});
            return false;
        }
    }

    context_.health_monitor.report({"runtime", true, "initialized"});
    initialized_ = true;
    return true;
}

int Application::run() {
    if (!initialized_ && !initialize()) {
        return 1;
    }

    if (context_.mode == RuntimeMode::kTrackerSmoke) {
        return run_tracker_smoke();
    }
    if (context_.mode == RuntimeMode::kValidateReplay) {
        const int code = run_validate_replay();
        shutdown();
        return code;
    }

    FixedTimestepScheduler scheduler(static_cast<double>(context_.config.fixed_timestep_hz));
    bool running = true;
    scheduler.run_while([this, &running](double dt_seconds) {
        if (running) {
            running = tick(dt_seconds);
        }
        return running;
    });
    shutdown();
    return 0;
}

void Application::request_shutdown() {
    shutdown_requested_.store(true, std::memory_order_release);
}

void Application::request_pause(bool paused) {
    pause_requested_.store(paused, std::memory_order_release);
}

bool Application::paused() const noexcept {
    return pause_requested_.load(std::memory_order_acquire);
}

bool Application::shutdown_requested() const noexcept {
    return shutdown_requested_.load(std::memory_order_acquire);
}

void Application::shutdown() {
    recorder_.end_session();
#ifdef ARX_HAS_OPENCV
    shutdown_live_runtime();
#endif
    context_.worker_pool.stop();
    initialized_ = false;
}

bool Application::tick(double dt_seconds) {
    if (shutdown_requested()) {
        return false;
    }

    const bool paused_now = paused();
    if (paused_now) {
        if (!pause_state_reported_) {
            context_.health_monitor.report({"runtime", true, "paused by operator"});
            runtime_log("runtime", "paused by operator");
            pause_state_reported_ = true;
        }
#ifdef ARX_HAS_OPENCV
        if (frame_queue_ != nullptr) {
            LiveFrameLease discarded;
            while (frame_queue_->try_pop(discarded)) {
            }
        }
#endif
        return !shutdown_requested();
    }

    if (pause_state_reported_) {
        context_.health_monitor.report({"runtime", true, "resumed by operator"});
        runtime_log("runtime", "resumed by operator");
        pause_state_reported_ = false;
    }

    auto frame_scope = context_.profiler.scope("runtime.frame");

    replay::SessionFrame source_frame;
    double camera_latency_ms = 0.0;
    double inference_latency_ms = 0.0;
    if (context_.mode == RuntimeMode::kReplay || context_.mode == RuntimeMode::kFusionReplay) {
        const auto next = player_.next();
        if (!next.has_value()) {
            return false;
        }
        source_frame = *next;
    } else {
#ifdef ARX_HAS_OPENCV
        if (!pull_live_frame(source_frame, dt_seconds, camera_latency_ms, inference_latency_ms)) {
            return true;
        }
#else
        return false;
#endif
    }

    vision::FrameLandmarks frame;
    frame.frame_id = source_frame.frame_id;
    frame.meta.frame_id = source_frame.frame_id;
    frame.meta.capture_us = source_frame.timestamp_us;
    frame.meta.process_us = source_frame.timestamp_us;
    frame.meta.fps = source_frame.fps;
    frame.meta.width = context_.config.camera.width;
    frame.meta.height = context_.config.camera.height;
    frame.meta.dropped_frames = static_cast<std::uint32_t>(std::min<std::uint64_t>(
        context_.stats.dropped_frames.load(),
        std::numeric_limits<std::uint32_t>::max()));
    frame.hands = source_frame.hands;
    frame.face = source_frame.face;

#ifdef ARX_HAS_OPENCV
    if (context_.mode == RuntimeMode::kFusionReplay) {
        if (!last_live_frame_.has_value()) {
            vision::tracking::VideoFrame replay_frame;
            replay_frame.meta = frame.meta;
            replay_frame.render = cv::Mat::zeros(frame.meta.height > 0 ? frame.meta.height : context_.config.camera.height,
                frame.meta.width > 0 ? frame.meta.width : context_.config.camera.width, CV_8UC3);
            last_live_frame_ = replay_frame;
        }
        last_live_frame_->meta = frame.meta;
    }
#endif

    const double ts_seconds = source_frame.timestamp_us / 1000000.0;
    double smoothing_latency_ms = 0.0;
    double gesture_latency_ms = 0.0;
    vision::GestureEngine::Output output;
    if (context_.config.tracking.disable_gesture_classifier || context_.mode == RuntimeMode::kTrackerSmoke) {
        output.smoothed = frame;
        output.gesture = {};
    } else {
        const auto gesture_start = std::chrono::steady_clock::now();
        output = gesture_engine_.process(frame, ts_seconds);
        gesture_latency_ms = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - gesture_start).count();
        smoothing_latency_ms = gesture_latency_ms;
        if (context_.config.tracking.debug_gestures) {
            log_gesture_debug(frame, output);
        }
        const auto spatial = spatial_engine_.update(output.gesture);
        const auto interaction = interaction_system_.update(scene_, spatial);
        (void)interaction;
    }

    double render_latency_ms = 0.0;
    const auto event_name = fusion_event_name(output);
    auto fallback_provider = vision::landmarks::LandmarkProviderOutput{};
    fallback_provider.frame = frame;
    const auto base_provider = last_tracking_result_.value_or(fallback_provider);
    auto fusion_frame = fusion_compositor_.compose(
        base_provider,
        output,
        event_name,
        camera_latency_ms,
        inference_latency_ms,
        smoothing_latency_ms,
        gesture_latency_ms,
        render_latency_ms,
        context_.stats.dropped_frames.load());
    fusion_frame.config_hash = config::runtime_config_hash(context_.config);

    if (context_.mode == RuntimeMode::kRecord || context_.mode == RuntimeMode::kGraphicalFusion) {
        replay::SessionFrame recorded = source_frame;
        recorded.gesture = output.gesture;
        recorded.hands = output.smoothed.hands;
        recorded.face = output.smoothed.face;
        recorded.dropped_frames = context_.stats.dropped_frames.load();
        recorded.profiler_snapshot = context_.metrics.to_json();
        if (context_.mode == RuntimeMode::kGraphicalFusion) {
            recorded.fusion = fusion_frame;
        }
        recorder_.record_frame(recorded);
    }

#ifdef ARX_HAS_OPENCV
    if (last_live_frame_.has_value() && last_tracking_result_.has_value()) {
        render_live_frame(*last_live_frame_, *last_tracking_result_, output, fusion_frame, render_latency_ms);
    }
#endif

    const double latency_ms = static_cast<double>(vision::now_us() - source_frame.timestamp_us) / 1000.0;
    context_.stats.frames_processed.fetch_add(1);
    context_.stats.last_latency_ms.store(latency_ms);
#ifdef ARX_HAS_OPENCV
    if (frame_queue_ != nullptr) {
        const auto dropped = frame_queue_->dropped();
        if (dropped > last_queue_drop_count_) {
            context_.stats.dropped_frames.fetch_add(dropped - last_queue_drop_count_);
            last_queue_drop_count_ = dropped;
        }
    }
#endif
    context_.metrics.record_latency("runtime.total_ms", latency_ms);
    context_.metrics.record_latency("camera.capture_ms", camera_latency_ms);
    context_.metrics.record_latency("inference.total_ms", inference_latency_ms);
    context_.metrics.record_latency("gesture.total_ms", gesture_latency_ms);
    context_.metrics.record_latency("smoothing.total_ms", smoothing_latency_ms);
    context_.metrics.record_counter("runtime.dropped_frames", context_.stats.dropped_frames.load());

    telemetry::RuntimeTelemetryFrame telemetry_frame;
    telemetry_frame.frame_meta = frame.meta;
    telemetry_frame.num_hands = output.smoothed.hands.size();
    telemetry_frame.gesture = output.gesture.label;
    telemetry_frame.confidence = output.gesture.confidence;
    telemetry_frame.latency_ms = latency_ms;
    telemetry_frame.end_to_end_latency_ms = latency_ms;
    telemetry_frame.camera_latency_ms = camera_latency_ms;
    telemetry_frame.inference_latency_ms = inference_latency_ms;
    telemetry_frame.smoothing_latency_ms = smoothing_latency_ms;
    telemetry_frame.gesture_latency_ms = gesture_latency_ms;
    telemetry_frame.camera_fps = frame.meta.fps;
    telemetry_frame.gesture_confidence = output.gesture.confidence;
#ifdef ARX_HAS_OPENCV
    telemetry_frame.render_latency_ms = render_latency_ms;
    if (last_tracking_result_.has_value()) {
        telemetry_frame.model_loaded = last_tracking_result_->debug.hand_model_loaded || last_tracking_result_->debug.face_model_loaded;
        telemetry_frame.raw_hand_count = last_tracking_result_->debug.raw_hand_count;
        telemetry_frame.top_hand_confidence = last_tracking_result_->debug.top_hand_confidence;
        telemetry_frame.landmark_confidence = last_tracking_result_->debug.top_hand_confidence;
        telemetry_frame.provider_health = last_tracking_result_->inference_ok ? "healthy" : "degraded";
        telemetry_frame.tracker_state = last_tracking_result_->debug.tracker_state;
        telemetry_frame.tracker_error = last_tracking_result_->failure_reason.empty()
            ? last_tracking_result_->debug.status_detail
            : last_tracking_result_->failure_reason;
        telemetry_frame.hand_model_path = last_tracking_result_->debug.hand_model_path;
        telemetry_frame.face_model_path = last_tracking_result_->debug.face_model_path;
    }
#endif
    telemetry_frame.dropped_frames = context_.stats.dropped_frames.load();
    telemetry_frame.recording = context_.recording;
    telemetry_frame.replaying = context_.replaying;
    telemetry_frame.profiler_snapshot = context_.metrics.to_json();
#ifdef ARX_HAS_OPENCV
    telemetry_frame.queue_depth = frame_queue_ ? frame_queue_->size() : 0;
    telemetry_frame.queue_capacity = frame_queue_ ? frame_queue_->capacity() : 0;
#endif
    context_.telemetry.push_timeline(context_.telemetry.encode_frame(telemetry_frame));

    return !shutdown_requested();
}

int Application::run_tracker_smoke() {
#ifdef ARX_HAS_OPENCV
    if (!context_.config.tracking.image_path.empty()) {
        const bool ok = run_tracker_smoke_image();
        shutdown();
        return ok ? 0 : 1;
    }

    FixedTimestepScheduler scheduler(static_cast<double>(context_.config.fixed_timestep_hz));
    bool running = true;
    scheduler.run_while([this, &running](double dt_seconds) {
        if (running) {
            running = tick(dt_seconds);
        }
        return running;
    });
    shutdown();
    return 0;
#else
    context_.health_monitor.report({"tracking", false, "tracker-smoke mode requires OpenCV support"});
    shutdown();
    return 1;
#endif
}

int Application::run_validate_replay() {
    if (session_path_.empty()) {
        runtime_log("replay", "validate-replay requires --session");
        return 2;
    }
    replay::SessionPlayer validator;
    if (!validator.load(session_path_)) {
        runtime_log("replay", "failed to load replay: " + session_path_.string());
        return 1;
    }
    std::size_t frames = 0;
    while (const auto frame = validator.next()) {
        ++frames;
        if (frame->frame_id == 0) {
            runtime_log("replay", "corrupted replay frame with frame_id=0");
            return 1;
        }
    }
    std::cout << "replay-valid frames=" << frames << " path=" << session_path_.string() << '\n';
    return frames > 0 ? 0 : 1;
}

std::optional<std::string> Application::fusion_event_name(const vision::GestureEngine::Output& output) const {
    if (!output.event.has_value()) {
        return std::nullopt;
    }
    return std::string(vision::gesture_event_name(output.event->kind));
}

bool Application::check_model_assets(bool fail_on_missing) {
    model_report_ = vision::tracking::validate_tracking_models(context_.config);
    runtime_log("models", model_report_->summary());
    const bool valid = model_report_->all_valid();
    if (!valid) {
        context_.health_monitor.report({"models", false, "MediaPipe .task model assets missing or unreadable"});
        context_.telemetry.push_timeline(model_report_->summary());
    } else {
        context_.health_monitor.report({"models", true, "MediaPipe .task model assets validated"});
    }
    return !fail_on_missing || valid;
}

void Application::log_gesture_debug(const vision::FrameLandmarks& frame,
                                    const vision::GestureEngine::Output& output) const {
    std::ostringstream geometry;
    geometry << "frames=" << frame.hands.size()
             << " smoothed=" << output.smoothed.hands.size()
             << " gesture=" << output.gesture.label
             << " conf=" << output.gesture.confidence
             << " debounce=" << (context_.config.tracking.disable_debounce ? "disabled" : "enabled");
    if (!output.smoothed.hands.empty()) {
        const auto& hand = output.smoothed.hands.front();
        const auto& thumb = hand.points[4];
        const auto& index = hand.points[8];
        const auto dx = thumb.x - index.x;
        const auto dy = thumb.y - index.y;
        const auto dz = thumb.z - index.z;
        const auto pinch = std::sqrt(dx * dx + dy * dy + dz * dz);
        geometry << " pinch=" << pinch
                 << " wrist=(" << hand.points[0].x << "," << hand.points[0].y << "," << hand.points[0].z << ")";
    }
    if (output.event.has_value()) {
        geometry << " event=" << vision::gesture_event_name(output.event->kind);
    }
    runtime_log("gesture-debug", geometry.str());
}

replay::SessionFrame Application::make_live_frame(double dt_seconds) {
    replay::SessionFrame frame;
    frame.timestamp_us = vision::now_us();
    frame.frame_id = ++live_frame_id_;
    frame.fps = dt_seconds > 0.0 ? 1.0 / dt_seconds : static_cast<double>(context_.config.target_fps);
    frame.latency_ms = dt_seconds * 1000.0;
    return frame;
}

#ifdef ARX_HAS_OPENCV
bool Application::initialize_live_runtime() {
    landmark_provider_ = vision::landmarks::make_landmark_provider(context_.config);
    if (landmark_provider_ == nullptr) {
        context_.health_monitor.report({"tracking", false, "failed to create landmark provider"});
        runtime_log("tracking", "failed to create landmark provider");
        return false;
    }
    (void)landmark_provider_->initialize();
    if (!landmark_provider_->available()) {
        context_.health_monitor.report({"tracking", false, landmark_provider_->last_error()});
        runtime_log("tracking", landmark_provider_->last_error());
        if (context_.mode == RuntimeMode::kGraphicalFusion) {
            return false;
        }
    } else {
        context_.health_monitor.report({"tracking", true, std::string(landmark_provider_->name()) + " landmark provider ready"});
        runtime_log("tracking", std::string(landmark_provider_->name()) + " landmark provider ready");
    }

    frame_pool_ = std::make_unique<LiveFramePool>();
    frame_queue_ = std::make_unique<LiveFrameQueue>();
    live_renderer_ = std::make_unique<ar::renderer::RenderEngine>();
    live_renderer_->init(context_.config.camera.width, context_.config.camera.height);

    camera_manager_ = std::make_unique<vision::camera::CameraManager>(
        [this](const cv::Mat& frame, const vision::FrameMetadata& meta) {
            auto lease = frame_pool_->acquire();
            if (!lease.has_value()) {
                context_.stats.dropped_frames.fetch_add(1);
                context_.health_monitor.report({"camera_queue", false, "frame pool exhausted"});
                return;
            }

            auto owned = std::move(*lease);
            owned->meta = meta;
            owned->bgr = frame.clone();
            owned->render.release();
            owned->rgb.release();

            if (!frame_queue_->try_push(std::move(owned))) {
                context_.stats.dropped_frames.fetch_add(1);
                context_.health_monitor.report({"camera_queue", false, "bounded frame queue saturated"});
            }
        });

    vision::camera::CameraConfig camera_cfg;
    camera_cfg.width = context_.config.camera.width;
    camera_cfg.height = context_.config.camera.height;
    camera_cfg.fps = context_.config.camera.fps;
    camera_manager_->add_camera(context_.config.camera_id, camera_cfg);
    if (!camera_manager_->start(context_.config.camera_id)) {
        context_.health_monitor.report({"camera", false, camera_manager_->last_error()});
        runtime_log("camera", camera_manager_->last_error());
        return false;
    }

    context_.health_monitor.report({"camera", true, "camera producer thread running"});
    runtime_log("camera", "camera producer thread running");
    return true;
}

void Application::shutdown_live_runtime() {
    if (camera_manager_ != nullptr) {
        camera_manager_->stop();
    }
    last_live_frame_.reset();
    last_tracking_result_.reset();
    landmark_provider_.reset();
    frame_queue_.reset();
    frame_pool_.reset();
}

bool Application::pull_live_frame(replay::SessionFrame& source_frame,
                                  double dt_seconds,
                                  double& camera_latency_ms,
                                  double& inference_latency_ms) {
    if (frame_queue_ == nullptr) {
        return false;
    }

    LiveFrameLease lease;
    if (!frame_queue_->try_pop(lease)) {
        return false;
    }

    auto& video_frame = *lease;
    camera_latency_ms = static_cast<double>(vision::now_us() - video_frame.meta.capture_us) / 1000.0;
    context_.metrics.record_counter("runtime.queue_depth", frame_queue_->size());

    if (landmark_provider_ != nullptr) {
        auto tracking = landmark_provider_->process(video_frame);
        inference_latency_ms = tracking.hand_inference_ms + tracking.face_inference_ms;
        if ((camera_latency_ms + inference_latency_ms) > context_.config.fusion.max_latency_ms) {
            tracking.frame.hands.clear();
            tracking.frame.face.reset();
            tracking.inference_ok = false;
            tracking.failure_reason = "stale landmark rejection";
            tracking.debug.tracker_state = "stale_rejected";
            tracking.debug.status_detail = tracking.failure_reason;
        }
        last_tracking_result_ = tracking;
        if (!tracking.inference_ok && !tracking.failure_reason.empty()) {
            context_.health_monitor.report({"tracking", false, tracking.failure_reason});
            runtime_log("tracking", tracking.failure_reason);
        } else {
            std::ostringstream detail;
            detail << "frame=" << video_frame.meta.frame_id
                   << " state=" << tracking.debug.tracker_state
                   << " hands=" << tracking.debug.raw_hand_count
                   << " conf=" << tracking.debug.top_hand_confidence
                   << " infer_ms=" << inference_latency_ms;
            runtime_log("tracking", detail.str());
        }
        if (context_.mode == RuntimeMode::kTrackerSmoke) {
            std::cout << "tracker-smoke frame=" << video_frame.meta.frame_id
                      << " hands=" << tracking.debug.raw_hand_count
                      << " conf=" << tracking.debug.top_hand_confidence
                      << " infer_ms=" << inference_latency_ms
                      << " state=" << tracking.debug.tracker_state << '\n';
        }

        source_frame.timestamp_us = video_frame.meta.capture_us;
        source_frame.frame_id = video_frame.meta.frame_id;
        source_frame.fps = video_frame.meta.fps > 0.0 ? video_frame.meta.fps : (dt_seconds > 0.0 ? 1.0 / dt_seconds : context_.config.target_fps);
        source_frame.latency_ms = camera_latency_ms + inference_latency_ms;
        source_frame.hands = std::move(tracking.frame.hands);
        source_frame.face = std::move(tracking.frame.face);
        last_live_frame_ = video_frame;
        return true;
    }

    source_frame = make_live_frame(dt_seconds);
    source_frame.timestamp_us = video_frame.meta.capture_us;
    source_frame.frame_id = video_frame.meta.frame_id;
    source_frame.fps = video_frame.meta.fps;
    last_live_frame_ = video_frame;
    return true;
}

bool Application::run_tracker_smoke_image() {
    if (landmark_provider_ == nullptr) {
        landmark_provider_ = vision::landmarks::make_landmark_provider(context_.config);
        (void)landmark_provider_->initialize();
    }

    const auto image = cv::imread(context_.config.tracking.image_path.string(), cv::IMREAD_COLOR);
    if (image.empty()) {
        runtime_log("tracking", "failed to load image: " + context_.config.tracking.image_path.string());
        return false;
    }

    vision::tracking::VideoFrame frame;
    frame.bgr = image.clone();
    frame.render = image.clone();
    frame.meta.frame_id = 1;
    frame.meta.capture_us = vision::now_us();
    frame.meta.width = image.cols;
    frame.meta.height = image.rows;
    frame.meta.fps = 1.0;
    if (live_renderer_ == nullptr) {
        live_renderer_ = std::make_unique<ar::renderer::RenderEngine>();
        live_renderer_->init(image.cols, image.rows);
    }

    auto tracking = landmark_provider_->process(frame);
    last_live_frame_ = frame;
    last_tracking_result_ = tracking;
    std::cout << "tracker-smoke image hands=" << tracking.debug.raw_hand_count
              << " faces=" << tracking.debug.raw_face_count
              << " conf=" << tracking.debug.top_hand_confidence
              << " infer_ms=" << (tracking.hand_inference_ms + tracking.face_inference_ms)
              << " state=" << tracking.debug.tracker_state << '\n';

    std::filesystem::create_directories("output");
    vision::GestureEngine::Output output;
    output.smoothed = tracking.frame;
    output.gesture = {};
    double render_latency_ms = 0.0;
    const auto event_name = fusion_event_name(output);
    auto fusion_frame = fusion_compositor_.compose(
        tracking,
        output,
        event_name,
        0.0,
        tracking.hand_inference_ms + tracking.face_inference_ms,
        0.0,
        0.0,
        render_latency_ms,
        0);
    render_live_frame(frame, tracking, output, fusion_frame, render_latency_ms);

    cv::Mat composed = frame.render.clone();
    live_renderer_->draw_hand_points(composed, tracking.frame.hands, cv::Scalar(0, 255, 255), 2);
    live_renderer_->draw_hand_skeleton(composed, tracking.frame.hands);
    live_renderer_->draw_hand_labels(composed, tracking.frame.hands);
    live_renderer_->draw_face_mesh(composed, tracking.frame.face);
    live_renderer_->draw_hud_overlay(composed,
        frame.meta,
        "none",
        tracking.debug.tracker_state,
        tracking.hand_inference_ms + tracking.face_inference_ms,
        tracking.debug.raw_hand_count,
        tracking.debug.top_hand_confidence,
        tracking.debug.hand_model_loaded || tracking.debug.face_model_loaded);
    return cv::imwrite("output/tracker_smoke_overlay.png", composed);
}

void Application::render_live_frame(const vision::tracking::VideoFrame& frame,
                                    const vision::landmarks::LandmarkProviderOutput& tracking,
                                    const vision::GestureEngine::Output& output,
                                    const ar::fusion::ARFusionFrame& fusion_frame,
                                    double& render_latency_ms) {
    if (context_.config.headless || live_renderer_ == nullptr || frame.render.empty()) {
        return;
    }

    const auto start = std::chrono::steady_clock::now();
    cv::Mat composed = frame.render.clone();
    live_renderer_->draw_hand_points(composed, output.smoothed.hands, cv::Scalar(0, 255, 255), 2);
    live_renderer_->draw_hand_skeleton(composed, output.smoothed.hands);
    live_renderer_->draw_hand_labels(composed, output.smoothed.hands);
    live_renderer_->draw_face_mesh(composed, output.smoothed.face.has_value() ? output.smoothed.face : tracking.frame.face);
    live_renderer_->draw_ar_cube(composed, output.smoothed.hands);
    live_renderer_->draw_interaction_circles(composed, output.smoothed.hands);
    live_renderer_->draw_fps_counter(composed, frame.meta.fps);
    live_renderer_->draw_fusion_overlay(composed, fusion_frame);
    live_renderer_->draw_hud_overlay(composed,
        frame.meta,
        output.gesture.label,
        tracking.debug.tracker_state,
        tracking.hand_inference_ms + tracking.face_inference_ms,
        tracking.debug.raw_hand_count,
        tracking.debug.top_hand_confidence,
        tracking.debug.hand_model_loaded || tracking.debug.face_model_loaded);
    cv::imshow("ARX Platform v3 Runtime", composed);
    if (cv::waitKey(1) == 27) {
        request_shutdown();
        context_.health_monitor.report({"runtime", true, "operator requested shutdown"});
    }

    render_latency_ms = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - start).count();
    context_.metrics.record_latency("render.overlay_ms", render_latency_ms);
}
#endif

}  // namespace arx::engine
