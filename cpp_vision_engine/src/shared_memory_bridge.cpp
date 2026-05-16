// ─────────────────────────────────────────────────────────────────────────────
// shared_memory_bridge.cpp  –  Zero-copy IPC between C++ vision engine and
//                               the Python AI layer via POSIX shared memory.
//
//  Layout of shared memory region:
//
//   [0..3]           – Magic "ARX\0"
//   [4..7]           – Protocol version (uint32)
//   [8..15]          – Write sequence counter (uint64, writer increments)
//   [16..23]         – Read sequence counter  (uint64, reader increments)
//   [24..27]         – Frame width  (uint32)
//   [28..31]         – Frame height (uint32)
//   [32..35]         – Frame stride (uint32, bytes per row)
//   [36..67]         – FrameMeta serialised (32 bytes, fixed layout)
//   [68..68+W*H*3-1] – RGB frame pixels
//   [... + N]        – Landmark JSON (variable, null-terminated)
//
// ─────────────────────────────────────────────────────────────────────────────

#include "shared_memory_bridge.hpp"
#include "arx_types.hpp"

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#ifdef __linux__
#  include <fcntl.h>
#  include <sys/mman.h>
#  include <sys/stat.h>
#  include <unistd.h>
#endif

#include <cstring>
#include <stdexcept>

namespace arx {

using json = nlohmann::json;

// ─── Fixed offsets ────────────────────────────────────────────────────────
static constexpr std::size_t kMagicOff    = 0;
static constexpr std::size_t kVersionOff  = 4;
static constexpr std::size_t kWSeqOff     = 8;
static constexpr std::size_t kRSeqOff     = 16;
static constexpr std::size_t kWidthOff    = 24;
static constexpr std::size_t kHeightOff   = 28;
static constexpr std::size_t kStrideOff   = 32;
static constexpr std::size_t kMetaOff     = 36;
static constexpr std::size_t kPixelOff    = 68;

SharedMemoryBridge::SharedMemoryBridge(int shm_key)
    : shm_key_(shm_key) {}

SharedMemoryBridge::~SharedMemoryBridge() { cleanup(); }

void SharedMemoryBridge::init(std::size_t pixel_bytes) {
#ifdef __linux__
    pixel_bytes_ = pixel_bytes;
    landmark_bytes_ = 65536;  // 64 KB for JSON landmarks
    total_bytes_ = kPixelOff + pixel_bytes_ + landmark_bytes_;

    const std::string name = "/arx_shm_" + std::to_string(shm_key_);

    int fd = shm_open(name.c_str(), O_CREAT | O_RDWR, 0600);
    if (fd < 0) throw std::runtime_error("shm_open failed");

    if (ftruncate(fd, static_cast<off_t>(total_bytes_)) < 0)
        throw std::runtime_error("ftruncate failed");

    ptr_ = mmap(nullptr, total_bytes_, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    ::close(fd);

    if (ptr_ == MAP_FAILED) throw std::runtime_error("mmap failed");

    // Write magic + version
    std::memcpy(static_cast<uint8_t*>(ptr_) + kMagicOff,   "ARX", 4);
    uint32_t ver = 1;
    std::memcpy(static_cast<uint8_t*>(ptr_) + kVersionOff, &ver, 4);

    spdlog::info("[SHM] Shared memory bridge initialised ({} bytes)", total_bytes_);
#else
    spdlog::warn("[SHM] POSIX shared memory not available on this platform");
#endif
}

void SharedMemoryBridge::write_frame(const cv::Mat& rgb, const FrameMeta& meta) {
#ifdef __linux__
    if (!ptr_) return;
    auto* base = static_cast<uint8_t*>(ptr_);

    uint32_t w = meta.width, h = meta.height;
    uint32_t stride = rgb.step[0];
    std::memcpy(base + kWidthOff,  &w,      4);
    std::memcpy(base + kHeightOff, &h,      4);
    std::memcpy(base + kStrideOff, &stride, 4);

    // Copy pixel data
    const std::size_t nbytes = std::min(static_cast<std::size_t>(stride * h),
                                         pixel_bytes_);
    std::memcpy(base + kPixelOff, rgb.data, nbytes);

    // Atomic increment of write sequence so Python knows a new frame arrived
    uint64_t seq;
    std::memcpy(&seq, base + kWSeqOff, 8);
    ++seq;
    std::memcpy(base + kWSeqOff, &seq, 8);
#endif
}

bool SharedMemoryBridge::read_landmarks(std::vector<HandLandmark>& hands,
                                         std::optional<FaceLandmark>& face)
{
#ifdef __linux__
    if (!ptr_) return false;
    auto* base = static_cast<uint8_t*>(ptr_);

    // Python increments read-seq after writing landmarks
    uint64_t wseq, rseq;
    std::memcpy(&wseq, base + kWSeqOff, 8);
    std::memcpy(&rseq, base + kRSeqOff, 8);
    if (rseq == wseq) return false;  // no new landmarks

    const char* json_ptr = reinterpret_cast<const char*>(
        base + kPixelOff + pixel_bytes_);

    try {
        auto j = json::parse(json_ptr);
        hands.clear();

        for (const auto& h : j.value("hands", json::array())) {
            HandLandmark lm;
            lm.confidence = h.value("confidence", 0.f);
            lm.is_left    = h.value("is_left", false);
            auto& pts = h.at("points");
            for (int i = 0; i < 21 && i < (int)pts.size(); ++i) {
                lm.points[i] = {pts[i][0].get<float>(),
                                pts[i][1].get<float>(),
                                pts[i][2].get<float>()};
            }
            hands.push_back(lm);
        }

        if (j.contains("face")) {
            FaceLandmark flm;
            auto& pts = j["face"]["points"];
            for (int i = 0; i < 468 && i < (int)pts.size(); ++i) {
                flm.points[i] = {pts[i][0].get<float>(),
                                  pts[i][1].get<float>(),
                                  pts[i][2].get<float>()};
            }
            flm.confidence = j["face"].value("confidence", 0.f);
            face = flm;
        }

        // Acknowledge
        std::memcpy(base + kRSeqOff, &wseq, 8);
        return true;

    } catch (const std::exception& e) {
        spdlog::warn("[SHM] Landmark JSON parse error: {}", e.what());
        return false;
    }
#else
    return false;
#endif
}

void SharedMemoryBridge::cleanup() {
#ifdef __linux__
    if (ptr_ && ptr_ != MAP_FAILED) {
        munmap(ptr_, total_bytes_);
        ptr_ = nullptr;
    }
#endif
}

}  // namespace arx
