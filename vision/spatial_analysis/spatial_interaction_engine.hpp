#pragma once

#include <string>

#include "vision/gesture_engine/gesture_types.hpp"

namespace arx::vision {

struct SpatialInteraction {
    std::string mode{"idle"};
    Point3f anchor{};
    float depth_bias{0.0f};
    bool selected{false};
};

class SpatialInteractionEngine {
public:
    SpatialInteraction update(const GestureResult& gesture) const;
};

}  // namespace arx::vision
