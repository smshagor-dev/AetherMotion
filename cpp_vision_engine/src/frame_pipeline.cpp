// ─────────────────────────────────────────────────────────────────────────────
// frame_pipeline.cpp  –  Multi-stage frame processing pipeline.
//
//  Stage 0: Camera capture          (CameraManager thread)
//  Stage 1: Pre-processing          (resize, color-correct, undistort)
//  Stage 2: AI bridge publish       (ZeroMQ push to Python AI layer)
//  Stage 3: AR overlay compose      (RenderEngine on received landmarks)
//  Stage 4: Display / encode        (cv::imshow or RTMP output)
//
//  Each stage runs on its own thread and communicates via lock-free RingBuffers.
// ─────────────────────────────────────────────────────────────────────────────

#include "frame_pipeline.hpp"
#include "arx_types.hpp"
#include "render_engine.hpp"
#include "shared_memory_bridge.hpp"
#include "telemetry_encoder.hpp"

#include <opencv2/imgproc.hpp>
#include <spdlog/spdlog.h>

#include <thread>
#include <chrono>

namespace arx {

FramePipeline::FramePipeline(const PipelineConfig& cfg)
    : cfg_(cfg)
    , render_engine_(std::make_unique<RenderEngine>())
    , shm_bridge_(std::make_unique<SharedMemoryBridge>(cfg.shm_key))
    , telemetry_(std::make_unique<TelemetryEncoder>(cfg.zmq_telemetry_port))
{}

FramePipeline::~FramePipeline() { stop(); }

void FramePipeline::start() {
    if (running_.exchange(true)) return;

    render_engine_->init(cfg_.display_width, cfg_.display_height);
    shm_bridge_->init(cfg_.display_width * cfg_.display_height * 3);
    telemetry_->init();

    // Stage 1: Pre-process
    workers_.emplace_back([this]() { preprocess_worker(); });
    // Stage 2: AI publish
    workers_.emplace_back([this]() { ai_publish_worker(); });
    // Stage 3: AR compose
    workers_.emplace_back([this]() { compose_worker(); });
    // Stage 4: Display
    workers_.emplace_back([this]() { display_worker(); });

    spdlog::info("[FramePipeline] Pipeline started ({} stages)", workers_.size());
}

void FramePipeline::stop() {
    running_.store(false);
    for (auto& t : workers_) if (t.joinable()) t.join();
    workers_.clear();
}

// Called by CameraManager callback – enqueue raw frame
void FramePipeline::push_raw(const cv::Mat& frame, const FrameMeta& meta) {
    StagedFrame sf;
    sf.raw  = frame.clone();
    sf.meta = meta;
    if (!raw_queue_.push(std::move(sf))) {
        ++stats_.raw_drops;
    }
}

// ─── Stage 1: Pre-processing ─────────────────────────────────────────────────
void FramePipeline::preprocess_worker() {
    StagedFrame sf;
    while (running_.load()) {
        if (!raw_queue_.pop(sf)) { std::this_thread::yield(); continue; }

        // Resize to AI inference resolution (e.g. 640×480)
        cv::Mat ai_frame;
        cv::resize(sf.raw, ai_frame,
                   cv::Size(cfg_.ai_width, cfg_.ai_height),
                   0, 0, cv::INTER_LINEAR);

        // Convert to RGB for MediaPipe/TF (Python AI layer)
        cv::cvtColor(ai_frame, sf.ai_rgb, cv::COLOR_BGR2RGB);

        // Full-res render copy (avoids extra alloc in compose stage)
        sf.render = sf.raw.clone();

        sf.meta.process_us = now_us();
        if (!ai_queue_.push(std::move(sf))) ++stats_.ai_drops;
    }
}

// ─── Stage 2: AI bridge publish ───────────────────────────────────────────────
void FramePipeline::ai_publish_worker() {
    StagedFrame sf;
    while (running_.load()) {
        if (!ai_queue_.pop(sf)) { std::this_thread::yield(); continue; }

        // Write RGB frame to shared memory → Python AI layer reads it
        shm_bridge_->write_frame(sf.ai_rgb, sf.meta);

        // Non-blocking: read back any landmarks published by AI layer
        if (shm_bridge_->read_landmarks(sf.hands, sf.face)) {
            sf.has_landmarks = true;
        }

        if (!compose_queue_.push(std::move(sf))) ++stats_.compose_drops;
    }
}

// ─── Stage 3: AR overlay compose ─────────────────────────────────────────────
void FramePipeline::compose_worker() {
    StagedFrame sf;
    while (running_.load()) {
        if (!compose_queue_.pop(sf)) { std::this_thread::yield(); continue; }

        if (sf.has_landmarks) {
            render_engine_->draw_hand_skeleton(sf.render, sf.hands);
            render_engine_->draw_face_mesh(sf.render, sf.face);
            render_engine_->draw_ar_cube(sf.render, sf.hands);
            render_engine_->draw_interaction_circles(sf.render, sf.hands);
        }
        render_engine_->draw_fps_counter(sf.render, sf.meta.fps);
        render_engine_->draw_hud_overlay(sf.render, sf.meta);

        // Build and send telemetry packet to Go control plane
        TelemetryPacket pkt;
        pkt.frame_meta              = sf.meta;
        pkt.hands                   = sf.hands;
        pkt.face                    = sf.face;
        pkt.pipeline_latency_ms     = static_cast<float>(
            (now_us() - sf.meta.capture_us) / 1000.0);
        telemetry_->encode_and_send(pkt);

        if (!display_queue_.push(std::move(sf))) ++stats_.display_drops;
    }
}

// ─── Stage 4: Display ─────────────────────────────────────────────────────────
void FramePipeline::display_worker() {
    StagedFrame sf;
    while (running_.load()) {
        if (!display_queue_.pop(sf)) { std::this_thread::yield(); continue; }
        if (cfg_.show_window) {
            cv::imshow("ARX Vision Engine", sf.render);
            if (cv::waitKey(1) == 27) { running_.store(false); }
        }
        ++stats_.frames_displayed;
    }
}

PipelineStats FramePipeline::stats() const noexcept { return stats_; }

}  // namespace arx
