#include "vision/smoothing/landmark_smoother.hpp"

namespace arx::vision {

LandmarkSmoother::LandmarkSmoother(std::size_t n_landmarks, std::size_t dims, double freq, double min_cutoff, double beta)
    : dims_(dims) {
    left_filters_.reserve(n_landmarks);
    right_filters_.reserve(n_landmarks);
    for (std::size_t i = 0; i < n_landmarks; ++i) {
        std::vector<OneEuroFilter> left_row;
        std::vector<OneEuroFilter> right_row;
        for (std::size_t d = 0; d < dims; ++d) {
            left_row.emplace_back(freq, min_cutoff, beta, 1.0);
            right_row.emplace_back(freq, min_cutoff, beta, 1.0);
        }
        left_filters_.push_back(std::move(left_row));
        right_filters_.push_back(std::move(right_row));
    }
}

FrameLandmarks LandmarkSmoother::smooth(const FrameLandmarks& current, double timestamp_seconds) {
    FrameLandmarks out = current;
    for (auto& hand : out.hands) {
        auto& filters = hand.is_left ? left_filters_ : right_filters_;
        for (std::size_t i = 0; i < kHandLandmarkCount; ++i) {
            hand.points[i].x = static_cast<float>(filters[i][0].filter(hand.points[i].x, timestamp_seconds));
            hand.points[i].y = static_cast<float>(filters[i][1].filter(hand.points[i].y, timestamp_seconds));
            if (dims_ > 2) {
                hand.points[i].z = static_cast<float>(filters[i][2].filter(hand.points[i].z, timestamp_seconds));
            }
        }
    }
    return out;
}

void LandmarkSmoother::reset() {
    for (auto& row : left_filters_) {
        for (auto& filter : row) {
            filter.reset();
        }
    }
    for (auto& row : right_filters_) {
        for (auto& filter : row) {
            filter.reset();
        }
    }
}

}  // namespace arx::vision
