#pragma once

#include "vision/gesture_engine/gesture_types.hpp"
#include "vision/landmarks/landmark_types.hpp"

namespace arx::vision {

class GestureClassifier {
public:
    GestureResult classify(const FrameLandmarks& frame, const std::optional<VelocityFrame>& velocity = std::nullopt);

private:
    GestureResult classify_single_hand(const HandLandmarks& hand);
    GestureResult classify_two_hand(const HandLandmarks& left, const HandLandmarks& right);
    std::optional<GestureResult> detect_swipe(const VelocityFrame& velocity) const;

    std::optional<float> previous_two_hand_distance_;
    std::optional<float> previous_two_hand_angle_;
};

}  // namespace arx::vision
