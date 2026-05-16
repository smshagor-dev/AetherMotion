#include "vision/smoothing/one_euro_filter.hpp"

#include <algorithm>

#include "vision/landmarks/landmark_types.hpp"

namespace arx::vision {

OneEuroFilter::OneEuroFilter(double freq, double min_cutoff, double beta, double d_cutoff)
    : freq_(freq), min_cutoff_(min_cutoff), beta_(beta), d_cutoff_(d_cutoff) {}

double OneEuroFilter::alpha(double cutoff, double frequency) {
    const double tau = 1.0 / (2.0 * 3.14159265358979323846 * cutoff);
    const double te = 1.0 / frequency;
    return 1.0 / (1.0 + tau / te);
}

double OneEuroFilter::low_pass(double alpha_value, double value, double previous) {
    return alpha_value * value + (1.0 - alpha_value) * previous;
}

double OneEuroFilter::filter(double value, double timestamp_seconds) {
    if (last_timestamp_.has_value()) {
        const double dt = std::max(timestamp_seconds - *last_timestamp_, 1e-6);
        freq_ = 1.0 / dt;
    }
    last_timestamp_ = timestamp_seconds;

    const double previous_x = x_prev_.value_or(value);
    const double dx = x_prev_.has_value() ? (value - previous_x) * freq_ : 0.0;

    const double d_alpha = alpha(d_cutoff_, freq_);
    const double dx_hat = dx_prev_.has_value() ? low_pass(d_alpha, dx, *dx_prev_) : dx;
    dx_prev_ = dx_hat;

    const double cutoff = min_cutoff_ + beta_ * std::abs(dx_hat);
    const double x_alpha = alpha(cutoff, freq_);
    const double x_hat = x_prev_.has_value() ? low_pass(x_alpha, value, *x_prev_) : value;
    x_prev_ = x_hat;
    return x_hat;
}

void OneEuroFilter::reset() {
    last_timestamp_.reset();
    x_prev_.reset();
    dx_prev_.reset();
}

VelocityEstimator::VelocityEstimator(std::size_t window) : window_(window) {}

VelocityFrame VelocityEstimator::update(float x, float y, double timestamp_seconds) {
    xs_.push_back(x);
    ys_.push_back(y);
    ts_.push_back(timestamp_seconds);

    if (xs_.size() > window_) {
        xs_.erase(xs_.begin());
        ys_.erase(ys_.begin());
        ts_.erase(ts_.begin());
    }

    if (xs_.size() < 2) {
        return {};
    }

    const double dt = std::max(ts_.back() - ts_.front(), 1e-6);
    const float vx = static_cast<float>((xs_.back() - xs_.front()) / dt);
    const float vy = static_cast<float>((ys_.back() - ys_.front()) / dt);
    return {vx, vy, std::sqrt(vx * vx + vy * vy)};
}

void VelocityEstimator::reset() {
    xs_.clear();
    ys_.clear();
    ts_.clear();
}

}  // namespace arx::vision
