#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// shared_memory_bridge.hpp
// ─────────────────────────────────────────────────────────────────────────────

#include "arx_types.hpp"
#include <cstddef>
#include <optional>
#include <vector>
#include <opencv2/core.hpp>

namespace arx {

class SharedMemoryBridge {
public:
    explicit SharedMemoryBridge(int shm_key);
    ~SharedMemoryBridge();

    void init(std::size_t pixel_bytes);
    void write_frame(const cv::Mat& rgb, const FrameMeta& meta);
    bool read_landmarks(std::vector<HandLandmark>& hands,
                         std::optional<FaceLandmark>& face);

private:
    void cleanup();

    int         shm_key_;
    void*       ptr_            = nullptr;
    std::size_t pixel_bytes_    = 0;
    std::size_t landmark_bytes_ = 0;
    std::size_t total_bytes_    = 0;
};

}  // namespace arx
