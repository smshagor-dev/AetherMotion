#include "vision/camera/camera_manager.hpp"

#ifdef ARX_HAS_OPENCV

#include <algorithm>
#include <chrono>

namespace arx::vision::camera {

CameraDevice::CameraDevice(int camera_id, CameraConfig config)
    : id_(camera_id), config_(config) {}

bool CameraDevice::open() {
    int backend = cv::CAP_ANY;
#ifdef _WIN32
    backend = cv::CAP_DSHOW;
#elif defined(__linux__)
    backend = cv::CAP_V4L2;
#endif
    capture_.open(id_, backend);
    if (!capture_.isOpened()) {
        return false;
    }
    capture_.set(cv::CAP_PROP_FRAME_WIDTH, config_.width);
    capture_.set(cv::CAP_PROP_FRAME_HEIGHT, config_.height);
    capture_.set(cv::CAP_PROP_FPS, config_.fps);
    capture_.set(cv::CAP_PROP_BUFFERSIZE, 2);
    capture_.set(cv::CAP_PROP_FOURCC, cv::VideoWriter::fourcc('M', 'J', 'P', 'G'));
    is_open_.store(true);
    return true;
}

void CameraDevice::close() {
    is_open_.store(false);
    if (capture_.isOpened()) {
        capture_.release();
    }
}

bool CameraDevice::grab_frame(cv::Mat& out, FrameMetadata& meta) {
    if (!capture_.grab()) {
        ++meta.dropped_frames;
        return false;
    }
    capture_.retrieve(out);
    if (out.empty()) {
        return false;
    }

    meta.camera_id = id_;
    meta.width = out.cols;
    meta.height = out.rows;
    meta.capture_us = now_us();
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
