#pragma once

#include <cstdint>
#include <memory>
#include <string>

#ifdef ARX_HAS_OPENCV
#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>
#endif

#include "engine/config/runtime_config.hpp"

namespace arx::vision::camera {

enum class PixelFormat : std::uint8_t {
    kBgr24 = 0,
};

struct CameraFrame {
#ifdef ARX_HAS_OPENCV
    cv::Mat bgr;
#endif
    std::uint64_t frame_id{0};
    std::int64_t timestamp_ns{0};
    int width{0};
    int height{0};
    PixelFormat pixel_format{PixelFormat::kBgr24};
    std::string source_id{"camera0"};
    std::uint64_t dropped_frames{0};
};

class CameraFrameSource {
public:
    virtual ~CameraFrameSource() = default;

    [[nodiscard]] virtual bool open() = 0;
    [[nodiscard]] virtual bool read(CameraFrame& frame) = 0;
    virtual void close() = 0;
    [[nodiscard]] virtual bool healthy() const noexcept = 0;
    [[nodiscard]] virtual const std::string& last_error() const noexcept = 0;
    [[nodiscard]] virtual const char* name() const noexcept = 0;
};

#ifdef ARX_HAS_OPENCV
class OpenCVVideoCaptureSource final : public CameraFrameSource {
public:
    OpenCVVideoCaptureSource(int camera_id, engine::config::CameraRuntimeConfig config);

    [[nodiscard]] bool open() override;
    [[nodiscard]] bool read(CameraFrame& frame) override;
    void close() override;
    [[nodiscard]] bool healthy() const noexcept override;
    [[nodiscard]] const std::string& last_error() const noexcept override;
    [[nodiscard]] const char* name() const noexcept override;

private:
    bool reopen_if_needed();
    bool open_capture();

    int camera_id_{0};
    engine::config::CameraRuntimeConfig config_{};
    cv::VideoCapture capture_;
    bool healthy_{false};
    std::uint64_t next_frame_id_{0};
    std::uint64_t dropped_frames_{0};
    std::string last_error_;
};
#endif

std::unique_ptr<CameraFrameSource> make_camera_frame_source(const engine::config::RuntimeConfig& config);

}  // namespace arx::vision::camera
