#pragma once

#include <cmath>
#include <optional>
#include <vector>

#include "vision/landmarks/landmark_types.hpp"

namespace arx::vision {

class OneEuroFilter {
public:
    OneEuroFilter(double freq = 30.0, double min_cutoff = 1.0, double beta = 0.007, double d_cutoff = 1.0);

    double filter(double value, double timestamp_seconds);
    void reset();

private:
    static double alpha(double cutoff, double frequency);
    static double low_pass(double alpha, double value, double previous);

    double freq_;
    double min_cutoff_;
    double beta_;
    double d_cutoff_;
    std::optional<double> last_timestamp_;
    std::optional<double> x_prev_;
    std::optional<double> dx_prev_;
};

class VelocityEstimator {
public:
    explicit VelocityEstimator(std::size_t window = 6);

    VelocityFrame update(float x, float y, double timestamp_seconds);
    void reset();

private:
    std::size_t window_;
    std::vector<float> xs_;
    std::vector<float> ys_;
    std::vector<double> ts_;
};

}  // namespace arx::vision
