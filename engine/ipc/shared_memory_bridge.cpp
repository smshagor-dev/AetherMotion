#include "engine/ipc/shared_memory_bridge.hpp"

#ifdef __linux__
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#include <cstring>
#include <regex>
#include <stdexcept>
#include <string>

namespace arx::engine::ipc {

namespace {
constexpr std::size_t kMagicOff = 0;
constexpr std::size_t kVersionOff = 4;
constexpr std::size_t kWSeqOff = 8;
constexpr std::size_t kRSeqOff = 16;
constexpr std::size_t kWidthOff = 24;
constexpr std::size_t kHeightOff = 28;
constexpr std::size_t kStrideOff = 32;
constexpr std::size_t kPixelOff = 68;

std::optional<float> extract_number(const std::string& source, const std::string& key) {
    const std::regex re("\"" + key + "\"\\s*:\\s*(-?[0-9]+(?:\\.[0-9]+)?)");
    std::smatch match;
    if (std::regex_search(source, match, re)) {
        return std::stof(match[1].str());
    }
    return std::nullopt;
}
}

SharedMemoryBridge::SharedMemoryBridge(int shm_key) : shm_key_(shm_key) {}

SharedMemoryBridge::~SharedMemoryBridge() {
    cleanup();
}

void SharedMemoryBridge::init(std::size_t pixel_bytes) {
#ifdef __linux__
    pixel_bytes_ = pixel_bytes;
    landmark_bytes_ = 65536;
    total_bytes_ = kPixelOff + pixel_bytes_ + landmark_bytes_;

    const std::string name = "/arx_shm_" + std::to_string(shm_key_);
    int fd = shm_open(name.c_str(), O_CREAT | O_RDWR, 0600);
    if (fd < 0) {
        throw std::runtime_error("shm_open failed");
    }
    if (ftruncate(fd, static_cast<off_t>(total_bytes_)) < 0) {
        ::close(fd);
        throw std::runtime_error("ftruncate failed");
    }

    ptr_ = mmap(nullptr, total_bytes_, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    ::close(fd);
    if (ptr_ == MAP_FAILED) {
        ptr_ = nullptr;
        throw std::runtime_error("mmap failed");
    }

    std::memcpy(static_cast<std::uint8_t*>(ptr_) + kMagicOff, "ARX", 4);
    const std::uint32_t version = 1;
    std::memcpy(static_cast<std::uint8_t*>(ptr_) + kVersionOff, &version, 4);
#else
    (void)pixel_bytes;
#endif
}

#ifdef ARX_HAS_OPENCV
void SharedMemoryBridge::write_frame(const cv::Mat& rgb, const vision::FrameMetadata& meta) {
#ifdef __linux__
    if (ptr_ == nullptr) {
        return;
    }
    auto* base = static_cast<std::uint8_t*>(ptr_);
    const std::uint32_t width = meta.width;
    const std::uint32_t height = meta.height;
    const std::uint32_t stride = static_cast<std::uint32_t>(rgb.step[0]);
    std::memcpy(base + kWidthOff, &width, 4);
    std::memcpy(base + kHeightOff, &height, 4);
    std::memcpy(base + kStrideOff, &stride, 4);

    const std::size_t bytes = std::min(static_cast<std::size_t>(stride * height), pixel_bytes_);
    std::memcpy(base + kPixelOff, rgb.data, bytes);

    std::uint64_t sequence = 0;
    std::memcpy(&sequence, base + kWSeqOff, 8);
    ++sequence;
    std::memcpy(base + kWSeqOff, &sequence, 8);
#else
    (void)rgb;
    (void)meta;
#endif
}
#endif

bool SharedMemoryBridge::read_landmarks(std::vector<vision::HandLandmarks>& hands, std::optional<vision::FaceLandmarks>& face) {
#ifdef __linux__
    if (ptr_ == nullptr) {
        return false;
    }
    auto* base = static_cast<std::uint8_t*>(ptr_);
    std::uint64_t wseq = 0;
    std::uint64_t rseq = 0;
    std::memcpy(&wseq, base + kWSeqOff, 8);
    std::memcpy(&rseq, base + kRSeqOff, 8);
    if (wseq == rseq) {
        return false;
    }

    const char* json_ptr = reinterpret_cast<const char*>(base + kPixelOff + pixel_bytes_);
    const std::string raw(json_ptr);
    if (raw.empty()) {
        return false;
    }

    hands.clear();
    vision::HandLandmarks hand;
    hand.confidence = extract_number(raw, "confidence").value_or(0.0f);
    if (raw.find("\"is_left\":true") != std::string::npos) {
        hand.is_left = true;
    }
    std::regex point_re("\\[(-?[0-9]+(?:\\.[0-9]+)?),(-?[0-9]+(?:\\.[0-9]+)?),(-?[0-9]+(?:\\.[0-9]+)?)\\]");
    auto begin = std::sregex_iterator(raw.begin(), raw.end(), point_re);
    auto end = std::sregex_iterator();
    std::size_t index = 0;
    for (auto it = begin; it != end && index < vision::kHandLandmarkCount; ++it, ++index) {
        hand.points[index] = {std::stof((*it)[1].str()), std::stof((*it)[2].str()), std::stof((*it)[3].str())};
    }
    if (index == vision::kHandLandmarkCount) {
        hands.push_back(hand);
    }

    std::memcpy(base + kRSeqOff, &wseq, 8);
    face.reset();
    return !hands.empty();
#else
    (void)hands;
    (void)face;
    return false;
#endif
}

void SharedMemoryBridge::cleanup() {
#ifdef __linux__
    if (ptr_ != nullptr) {
        munmap(ptr_, total_bytes_);
        ptr_ = nullptr;
    }
#endif
}

}  // namespace arx::engine::ipc
