#include "vision/preprocessing/frame_pipeline.hpp"

#ifdef ARX_HAS_OPENCV

#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>

namespace arx::vision::preprocessing {

FramePipeline::FramePipeline(PipelineConfig config)
    : config_(config),
      renderer_(std::make_unique<ar::renderer::RenderEngine>()),
      bridge_(std::make_unique<engine::ipc::SharedMemoryBridge>(config.shm_key)),
      telemetry_(std::make_unique<engine::telemetry::TelemetryEncoder>()) {}

FramePipeline::~FramePipeline() {
    stop();
}

void FramePipeline::start() {
    if (running_.exchange(true)) {
        return;
    }
    renderer_->init(config_.display_width, config_.display_height);
    bridge_->init(static_cast<std::size_t>(config_.ai_width * config_.ai_height * 3));
    workers_.emplace_back([this]() { preprocess_worker(); });
    workers_.emplace_back([this]() { bridge_worker(); });
    workers_.emplace_back([this]() { compose_worker(); });
    workers_.emplace_back([this]() { display_worker(); });
}

void FramePipeline::stop() {
    running_.store(false);
    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    workers_.clear();
}

void FramePipeline::push_raw(const cv::Mat& frame, const FrameMetadata& meta) {
    raw_queue_.push_back(StagedFrame{frame.clone(), {}, {}, meta});
}

void FramePipeline::preprocess_worker() {
    while (running_.load()) {
        if (raw_queue_.empty()) {
            std::this_thread::yield();
            continue;
        }
        auto frame = std::move(raw_queue_.front());
        raw_queue_.erase(raw_queue_.begin());
        cv::Mat resized;
        cv::resize(frame.raw, resized, cv::Size(config_.ai_width, config_.ai_height), 0, 0, cv::INTER_LINEAR);
        cv::cvtColor(resized, frame.ai_rgb, cv::COLOR_BGR2RGB);
        frame.render = frame.raw.clone();
        frame.meta.process_us = now_us();
        ai_queue_.push_back(std::move(frame));
    }
}

void FramePipeline::bridge_worker() {
    while (running_.load()) {
        if (ai_queue_.empty()) {
            std::this_thread::yield();
            continue;
        }
        auto frame = std::move(ai_queue_.front());
        ai_queue_.erase(ai_queue_.begin());
        bridge_->write_frame(frame.ai_rgb, frame.meta);
        frame.has_landmarks = bridge_->read_landmarks(frame.hands, frame.face);
        compose_queue_.push_back(std::move(frame));
    }
}

void FramePipeline::compose_worker() {
    while (running_.load()) {
        if (compose_queue_.empty()) {
            std::this_thread::yield();
            continue;
        }
        auto frame = std::move(compose_queue_.front());
        compose_queue_.erase(compose_queue_.begin());
        if (frame.has_landmarks) {
            renderer_->draw_hand_skeleton(frame.render, frame.hands);
            renderer_->draw_face_mesh(frame.render, frame.face);
            renderer_->draw_ar_cube(frame.render, frame.hands);
            renderer_->draw_interaction_circles(frame.render, frame.hands);
        }
        renderer_->draw_fps_counter(frame.render, frame.meta.fps);
        renderer_->draw_hud_overlay(frame.render,
            frame.meta,
            frame.has_landmarks ? "tracked" : "none",
            frame.has_landmarks ? "tracking" : "no_landmarks",
            0.0,
            frame.hands.size(),
            frame.hands.empty() ? 0.0f : frame.hands.front().confidence,
            frame.has_landmarks);
        display_queue_.push_back(std::move(frame));
    }
}

void FramePipeline::display_worker() {
    while (running_.load()) {
        if (display_queue_.empty()) {
            std::this_thread::yield();
            continue;
        }
        auto frame = std::move(display_queue_.front());
        display_queue_.erase(display_queue_.begin());
        if (config_.show_window) {
            cv::imshow("ARX v3 Pipeline", frame.render);
            if (cv::waitKey(1) == 27) {
                running_.store(false);
            }
        }
        engine::telemetry::RuntimeTelemetryFrame sample;
        sample.frame_meta = frame.meta;
        sample.num_hands = frame.hands.size();
        sample.gesture = frame.has_landmarks ? "tracked" : "none";
        sample.latency_ms = static_cast<double>(now_us() - frame.meta.capture_us) / 1000.0;
        telemetry_->push_timeline(telemetry_->encode_frame(sample));
    }
}

}  // namespace arx::vision::preprocessing

#endif
