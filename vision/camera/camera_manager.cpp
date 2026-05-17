#include "vision/camera/camera_manager.hpp"

#ifdef ARX_HAS_OPENCV

#include <algorithm>
#include <chrono>

namespace arx::vision::camera {

CameraDevice::CameraDevice(int camera_id, CameraConfig config)
    : id_(camera_id), config_(config) {}

bool CameraDevice::open() {
    engine::config::CameraRuntimeConfig runtime_config;
    runtime_config.id = id_;
    runtime_config.source = std::to_string(id_);
    runtime_config.width = config_.width;
    runtime_config.height = config_.height;
    runtime_config.fps = config_.fps;
    source_ = std::make_unique<OpenCVVideoCaptureSource>(id_, runtime_config);
    const bool ok = source_->open();
    is_open_.store(ok);
    return ok;
}

void CameraDevice::close() {
    is_open_.store(false);
    if (source_ != nullptr) {
        source_->close();
    }
}

bool CameraDevice::grab_frame(cv::Mat& out, FrameMetadata& meta) {
    if (source_ == nullptr) {
        return false;
    }
    CameraFrame frame;
    if (!source_->read(frame)) {
        ++meta.dropped_frames;
        return false;
    }
    out = frame.bgr;
    meta.camera_id = id_;
    meta.width = frame.width;
    meta.height = frame.height;
    meta.capture_ns = frame.timestamp_ns;
    meta.process_ns = frame.timestamp_ns;
    meta.capture_us = frame.timestamp_ns / 1000;
    meta.process_us = frame.timestamp_ns / 1000;
    meta.pixel_format = "bgr24";
    meta.source_id = frame.source_id;
    meta.dropped_frames = static_cast<std::uint32_t>(frame.dropped_frames);
    return true;
}

int CameraDevice::id() const noexcept {
    return id_;
}

double CameraDevice::fps() const noexcept {
    return config_.fps;
}

CameraManager::CameraManager(FrameCallback on_frame) : on_frame_(std::move(on_frame)) {}

CameraManager::~CameraManager() {
    stop();
}

void CameraManager::add_camera(int id, const CameraConfig& cfg) {
    cameras_.push_back(std::make_unique<CameraDevice>(id, cfg));
}

bool CameraManager::start(int preferred_camera_id) {
    if (running_.exchange(true)) {
        return true;
    }

    bool opened_any = false;
    for (auto& camera : cameras_) {
        if (camera->id() != preferred_camera_id) {
            continue;
        }
        if (camera->open()) {
            opened_any = true;
            threads_.emplace_back([this, &camera]() { capture_loop(*camera); });
        } else {
            last_error_ = "failed to open camera " + std::to_string(camera->id());
        }
    }
    if (!opened_any && last_error_.empty()) {
        last_error_ = "no matching camera configured for runtime";
    }
    return opened_any;
}

void CameraManager::stop() {
    running_.store(false);
    for (auto& thread : threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    threads_.clear();
    for (auto& camera : cameras_) {
        camera->close();
    }
}

void CameraManager::capture_loop(CameraDevice& camera) {
    using namespace std::chrono;
    const auto frame_duration = microseconds(static_cast<long long>(1e6 / std::max(1.0, camera.fps())));
    std::uint64_t frame_id = 0;
    cv::Mat frame;
    FrameMetadata meta{};

    while (running_.load()) {
        const auto start = steady_clock::now();
        if (camera.grab_frame(frame, meta)) {
            meta.frame_id = ++frame_id;
            meta.process_us = now_us();
            meta.fps = camera.fps();
            on_frame_(frame, meta);
        }
        const auto elapsed = steady_clock::now() - start;
        if (elapsed < frame_duration) {
            std::this_thread::sleep_for(frame_duration - elapsed);
        }
    }
}

const std::string& CameraManager::last_error() const noexcept {
    return last_error_;
}

}  // namespace arx::vision::camera

#endif
