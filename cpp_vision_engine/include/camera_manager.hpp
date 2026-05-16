#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// camera_manager.hpp
// ─────────────────────────────────────────────────────────────────────────────

#include "arx_types.hpp"

#include <atomic>
#include <functional>
#include <memory>
#include <thread>
#include <vector>

#include <opencv2/videoio.hpp>

namespace arx {

struct CameraConfig {
    int    width  = 1280;
    int    height = 720;
    double fps    = 60.0;
};

using FrameCallback = std::function<void(const cv::Mat&, const FrameMeta&)>;

// ─────────────────────────────────────────────────────────────────────────────

class CameraDevice {
public:
    CameraDevice(int camera_id, const CameraConfig& cfg);

    bool open();
    void close();
    bool grab_frame(cv::Mat& out, FrameMeta& meta);

    int  id()       const noexcept { return id_; }
    bool is_open()  const noexcept { return is_open_.load(); }
    int  width()    const noexcept { return actual_width_; }
    int  height()   const noexcept { return actual_height_; }
    double fps()    const noexcept { return actual_fps_; }

private:
    int            id_;
    CameraConfig   cfg_;
    cv::VideoCapture cap_;
    std::atomic<bool> is_open_{false};
    int    actual_width_  = 0;
    int    actual_height_ = 0;
    double actual_fps_    = 0.0;
};

// ─────────────────────────────────────────────────────────────────────────────

class CameraManager {
public:
    explicit CameraManager(FrameCallback on_frame);
    ~CameraManager();

    void add_camera(int id, const CameraConfig& cfg);
    void start();
    void stop();

private:
    void capture_loop(CameraDevice& cam);

    FrameCallback  on_frame_;
    std::vector<std::unique_ptr<CameraDevice>> cameras_;
    std::vector<std::thread>                   threads_;
    std::atomic<bool>                          running_{false};
};

}  // namespace arx
