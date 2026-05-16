#pragma once

#include <cstddef>
#include <optional>
#include <vector>

#ifdef ARX_HAS_OPENCV
#include <opencv2/core.hpp>
#endif

#include "vision/landmarks/landmark_types.hpp"

namespace arx::engine::ipc {

class SharedMemoryBridge {
public:
    explicit SharedMemoryBridge(int shm_key);
    ~SharedMemoryBridge();

    void init(std::size_t pixel_bytes);
#ifdef ARX_HAS_OPENCV
    void write_frame(const cv::Mat& rgb, const vision::FrameMetadata& meta);
#endif
    bool read_landmarks(std::vector<vision::HandLandmarks>& hands, std::optional<vision::FaceLandmarks>& face);

private:
    void cleanup();

    int shm_key_;
    void* ptr_{nullptr};
    std::size_t pixel_bytes_{0};
    std::size_t landmark_bytes_{0};
    std::size_t total_bytes_{0};
};

}  // namespace arx::engine::ipc
