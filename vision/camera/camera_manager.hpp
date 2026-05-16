#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#ifdef ARX_HAS_OPENCV
#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>
#endif

#include "vision/landmarks/landmark_types.hpp"

namespace arx::vision::camera {

struct CameraConfig {
    int width{1280};
    int height{720};
    double fps{60.0};
};

#ifdef ARX_HAS_OPENCV
using FrameCallback = std::function<void(const cv::Mat&, const FrameMetadata&)>;

class CameraDevice {
public:
    CameraDevice(int camera_id, CameraConfig config);

    bool open();
    void close();
    bool grab_frame(cv::Mat& out, FrameMetadata& meta);
    int id() const noexcept;
    [[nodiscard]] double fps() const noexcept;

private:
    int id_;
    CameraConfig config_;
    cv::VideoCapture capture_;
    std::atomic<bool> is_open_{false};
};

class CameraManager {
public:
    explicit CameraManager(FrameCallback on_frame);
    ~CameraManager();

    void add_camera(int id, const CameraConfig& cfg);
    bool start(int preferred_camera_id);
    void stop();
    [[nodiscard]] const std::string& last_error() const noexcept;

private:
    void capture_loop(CameraDevice& camera);

    FrameCallback on_frame_;
    std::vector<std::unique_ptr<CameraDevice>> cameras_;
    std::vector<std::thread> threads_;
    std::atomic<bool> running_{false};
    std::string last_error_;
};
#endif

}  // namespace arx::vision::camera
