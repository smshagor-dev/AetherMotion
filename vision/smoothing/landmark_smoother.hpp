#pragma once

#include <vector>

#include "vision/landmarks/landmark_types.hpp"
#include "vision/smoothing/one_euro_filter.hpp"

namespace arx::vision {

class LandmarkSmoother {
public:
    LandmarkSmoother(std::size_t n_landmarks = kHandLandmarkCount, std::size_t dims = 3,
        double freq = 30.0, double min_cutoff = 1.0, double beta = 0.007);

    FrameLandmarks smooth(const FrameLandmarks& current, double timestamp_seconds);
    void reset();

private:
    std::size_t dims_;
    std::vector<std::vector<OneEuroFilter>> left_filters_;
    std::vector<std::vector<OneEuroFilter>> right_filters_;
};

}  // namespace arx::vision
