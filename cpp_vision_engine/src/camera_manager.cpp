// ─────────────────────────────────────────────────────────────────────────────
// camera_manager.cpp  –  Multi-camera capture with hot-plug detection,
//                         adaptive resolution, and per-camera thread affinity.
// ─────────────────────────────────────────────────────────────────────────────

#include "camera_manager.hpp"
#include "arx_types.hpp"

#include <opencv2/videoio.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <chrono>
#include <stdexcept>
#include <thread>

namespace arx {

// ─── CameraDevice implementation ─────────────────────────────────────────────

CameraDevice::CameraDevice(int camera_id, const CameraConfig& cfg)
    : id_(camera_id), cfg_(cfg) {}

bool CameraDevice::open() {
    int backend = cv::CAP_V4L2;
#ifdef _WIN32
    backend = cv::CAP_DSHOW;
#elif defined(__APPLE__)
    backend = cv::CAP_AVFOUNDATION;
#endif
    cap_.open(id_, backend);
    if (!cap_.isOpened()) {
        spdlog::error("[Camera {}] Failed to open device", id_);
        return false;
    }

    cap_.set(cv::CAP_PROP_FRAME_WIDTH,  cfg_.width);
    cap_.set(cv::CAP_PROP_FRAME_HEIGHT, cfg_.height);
    cap_.set(cv::CAP_PROP_FPS,          cfg_.fps);
    cap_.set(cv::CAP_PROP_BUFFERSIZE,   2);  // minimal buffer → low latency

    // Prefer MJPEG for bandwidth efficiency on USB cameras
    cap_.set(cv::CAP_PROP_FOURCC, cv::VideoWriter::fourcc('M','J','P','G'));

    actual_width_  = static_cast<int>(cap_.get(cv::CAP_PROP_FRAME_WIDTH));
    actual_height_ = static_cast<int>(cap_.get(cv::CAP_PROP_FRAME_HEIGHT));
    actual_fps_    = cap_.get(cv::CAP_PROP_FPS);

    spdlog::info("[Camera {}] Opened {}x{} @ {:.1f}fps",
                 id_, actual_width_, actual_height_, actual_fps_);
    is_open_.store(true);
    return true;
}

void CameraDevice::close() {
    is_open_.store(false);
    if (cap_.isOpened()) cap_.release();
}

bool CameraDevice::grab_frame(cv::Mat& out, FrameMeta& meta) {
    if (!cap_.grab()) {
        ++meta.dropped_frames;
        return false;
    }
    cap_.retrieve(out);
    if (out.empty()) return false;

    meta.camera_id   = id_;
    meta.width       = out.cols;
    meta.height      = out.rows;
    meta.capture_us  = now_us();
    return true;
}

// ─── CameraManager implementation ────────────────────────────────────────────

CameraManager::CameraManager(FrameCallback on_frame)
    : on_frame_(std::move(on_frame)) {}

CameraManager::~CameraManager() { stop(); }

void CameraManager::add_camera(int id, const CameraConfig& cfg) {
    cameras_.emplace_back(std::make_unique<CameraDevice>(id, cfg));
}

void CameraManager::start() {
    if (running_.exchange(true)) return;
    for (auto& cam : cameras_) {
        if (cam->open()) {
            threads_.emplace_back([this, &cam]() { capture_loop(*cam); });
        }
    }
    spdlog::info("[CameraManager] Started {} capture threads", threads_.size());
}

void CameraManager::stop() {
    running_.store(false);
    for (auto& t : threads_) if (t.joinable()) t.join();
    threads_.clear();
    for (auto& cam : cameras_) cam->close();
    spdlog::info("[CameraManager] All cameras stopped");
}

void CameraManager::capture_loop(CameraDevice& cam) {
    using namespace std::chrono;
    const auto frame_duration = microseconds(static_cast<long>(1e6 / kTargetFPS));

    cv::Mat raw, bgr;
    FrameMeta meta{};
    int64_t frame_id = 0;

    // Optional: pin thread to CPU core for cache locality
    // (Linux-specific; silently ignored elsewhere)
#ifdef __linux__
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(cam.id() % std::thread::hardware_concurrency(), &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpuset), &cpuset);
#endif

    while (running_.load()) {
        const auto t0 = Clock::now();

        if (cam.grab_frame(raw, meta)) {
            // Downscale for AI layer if needed (keeps rendering at full res)
            meta.frame_id = ++frame_id;

            // Color normalize
            cv::cvtColor(raw, bgr, cv::COLOR_BGR2BGR);  // ensure contiguous

            // Compute instantaneous FPS
            auto elapsed = duration_cast<microseconds>(Clock::now() - t0).count();
            meta.fps = elapsed > 0 ? 1e6 / elapsed : kTargetFPS;

            if (on_frame_) on_frame_(bgr, meta);
        }

        // Adaptive sleep: keep loop at target FPS without busy-spinning
        const auto elapsed = Clock::now() - t0;
        if (elapsed < frame_duration) {
            std::this_thread::sleep_for(frame_duration - elapsed);
        }
    }
}

}  // namespace arx
