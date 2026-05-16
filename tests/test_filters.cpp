#include <cmath>
#include <iostream>

#include "vision/smoothing/one_euro_filter.hpp"

namespace {

bool expect_true(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << '\n';
        return false;
    }
    return true;
}

}

int main() {
    using namespace arx::vision;

    OneEuroFilter filter(30.0, 1.0, 0.01, 1.0);
    const double a = filter.filter(0.0, 0.0);
    const double b = filter.filter(1.0, 1.0 / 30.0);
    const double c = filter.filter(1.0, 2.0 / 30.0);

    VelocityEstimator velocity(4);
    auto v0 = velocity.update(0.0f, 0.0f, 0.0);
    auto v1 = velocity.update(0.4f, 0.0f, 0.2);

    bool ok = true;
    ok &= expect_true(std::abs(a) < 1e-6, "Filter should keep initial value");
    ok &= expect_true(b > 0.0 && b < 1.0, "Filter should smooth toward the target");
    ok &= expect_true(c >= b, "Filter should continue approaching the target");
    ok &= expect_true(v0.speed == 0.0f, "Velocity should be zero with one sample");
    ok &= expect_true(v1.vx > 1.5f, "Velocity estimator should detect horizontal motion");

    return ok ? 0 : 1;
}
