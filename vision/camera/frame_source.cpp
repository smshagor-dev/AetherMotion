#include "vision/camera/frame_source.hpp"

#include <chrono>
#include <thread>

#include "vision/landmarks/landmark_types.hpp"

namespace arx::vision::camera {

#ifdef ARX_HAS_OPENCV

namespace {

bool looks_like_index(const std::string& value) {
    if (value.empty()) {
        return false;
    }
    for (char c : value) {
        if (c < '0' || c > '9') {
            return false;
        }
    }
    return true;
}

}  // namespace

OpenCVVideoCaptureSource::OpenCVVideoCaptureSource(int camera_id, engine::config::CameraRuntimeConfig config)
    : camera_id_(camera_id), config_(std::move(config)) {}

bool OpenCVVideoCaptureSource::open() {
    return open_capture();
}

bool OpenCVVideoCaptureSource::read(CameraFrame& frame) {
    if (!reopen_if_needed()) {
        ++dropped_frames_;
        return false;
    }
    cv::Mat image;
    if (!capture_.read(image) || image.empty()) {
        healthy_ = false;
        last_error_ = "frame decode failure";
        ++dropped_frames_;
        if (config_.allow_reconnect) {
            std::this_thread::sleep_for(std::chrono::milliseconds(config_.reconnect_delay_ms));
        }
        return false;
    }
    frame.frame_id = ++next_frame_id_;
    frame.timestamp_ns = vision::now_us() * 1000;
    frame.width = image.cols;
    frame.height = image.rows;
    frame.pixel_format = PixelFormat::kBgr24;
    frame.source_id = config_.source.empty() ? ("camera" + std::to_string(camera_id_)) : config_.source;
    frame.dropped_frames = dropped_frames_;
    frame.bgr = image;
    healthy_ = true;
    return true;
}

void OpenCVVideoCaptureSource::close() {
    healthy_ = false;
    if (capture_.isOpened()) {
        capture_.release();
    }
}

bool OpenCVVideoCaptureSource::healthy() const noexcept {
    return healthy_;
}

const std::string& OpenCVVideoCaptureSource::last_error() const noexcept {
    return last_error_;
}

const char* OpenCVVideoCaptureSource::name() const noexcept {
    return "opencv-video-capture";
}

bool OpenCVVideoCaptureSource::reopen_if_needed() {
    if (capture_.isOpened()) {
        return true;
    }
    if (!config_.allow_reconnect && next_frame_id_ > 0) {
        last_error_ = "camera source disconnected";
        return false;
    }
    return open_capture();
}

bool OpenCVVideoCaptureSource::open_capture() {
    close();
    int backend = cv::CAP_ANY;
#ifdef _WIN32
    backend = cv::CAP_DSHOW;
#elif defined(__linux__)
    backend = cv::CAP_V4L2;
#endif

    bool opened = false;
    if (looks_like_index(config_.source)) {
        opened = capture_.open(std::stoi(config_.source), backend);
    } else if (!config_.source.empty() && config_.source != "0") {
        opened = capture_.open(config_.source, backend);
    } else {
        opened = capture_.open(camera_id_, backend);
    }

    if (!opened || !capture_.isOpened()) {
        healthy_ = false;
        last_error_ = "camera open failure: " + (config_.source.empty() ? std::to_string(camera_id_) : config_.source);
        return false;
    }
    capture_.set(cv::CAP_PROP_FRAME_WIDTH, config_.width);
    capture_.set(cv::CAP_PROP_FRAME_HEIGHT, config_.height);
    capture_.set(cv::CAP_PROP_FPS, config_.fps);
    capture_.set(cv::CAP_PROP_BUFFERSIZE, 2);
    healthy_ = true;
    last_error_.clear();
    return true;
}

#endif

std::unique_ptr<CameraFrameSource> make_camera_frame_source(const engine::config::RuntimeConfig& config) {
#ifdef ARX_HAS_OPENCV
    return std::make_unique<OpenCVVideoCaptureSource>(config.camera_id, config.camera);
#else
    (void)config;
    return nullptr;
#endif
}

}  // namespace arx::vision::camera
