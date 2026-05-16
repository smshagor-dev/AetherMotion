#include "vision/gesture_engine/gesture_classifier.hpp"

#include <algorithm>
#include <cmath>

namespace arx::vision {

namespace {

constexpr int kWrist = 0;
constexpr int kThumbTip = 4;
constexpr int kIndexMcp = 5;
constexpr int kIndexPip = 6;
constexpr int kIndexTip = 8;
constexpr int kMiddleMcp = 9;
constexpr int kMiddlePip = 10;
constexpr int kMiddleTip = 12;
constexpr int kRingPip = 14;
constexpr int kRingTip = 16;
constexpr int kPinkyMcp = 17;
constexpr int kPinkyPip = 18;
constexpr int kPinkyTip = 20;

float distance(const Point3f& a, const Point3f& b) {
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    const float dz = a.z - b.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

float hand_span(const HandLandmarks& hand) {
    return distance(hand.points[kWrist], hand.points[kMiddleMcp]) + 1e-6f;
}

bool finger_extended(const HandLandmarks& hand, int tip, int pip) {
    return distance(hand.points[tip], hand.points[kWrist]) >
           distance(hand.points[pip], hand.points[kWrist]) * 1.05f;
}

int count_extended(const HandLandmarks& hand) {
    int count = 0;
    count += finger_extended(hand, kIndexTip, kIndexPip) ? 1 : 0;
    count += finger_extended(hand, kMiddleTip, kMiddlePip) ? 1 : 0;
    count += finger_extended(hand, kRingTip, kRingPip) ? 1 : 0;
    count += finger_extended(hand, kPinkyTip, kPinkyPip) ? 1 : 0;
    return count;
}

GestureResult make_result(GestureType type, float confidence, const Point3f& focus) {
    return {type, confidence, gesture_name(type), focus};
}

}  // namespace

GestureResult GestureClassifier::classify(const FrameLandmarks& frame, const std::optional<VelocityFrame>& velocity) {
    if (frame.hands.empty()) {
        return {};
    }

    if (velocity.has_value()) {
        if (auto swipe = detect_swipe(*velocity); swipe.has_value()) {
            return *swipe;
        }
    }

    if (frame.hands.size() >= 2) {
        const HandLandmarks* left = nullptr;
        const HandLandmarks* right = nullptr;
        for (const auto& hand : frame.hands) {
            if (hand.is_left) {
                left = &hand;
            } else {
                right = &hand;
            }
        }
        left = left ? left : &frame.hands.front();
        right = right ? right : &frame.hands.back();
        return classify_two_hand(*left, *right);
    }

    return classify_single_hand(frame.hands.front());
}

std::optional<GestureResult> GestureClassifier::detect_swipe(const VelocityFrame& velocity) const {
    constexpr float kSpeedThreshold = 1.2f;
    constexpr float kAxisRatio = 2.0f;
    if (velocity.speed < kSpeedThreshold) {
        return std::nullopt;
    }
    if (std::abs(velocity.vx) > std::abs(velocity.vy) * kAxisRatio) {
        const auto type = velocity.vx > 0.0f ? GestureType::kSwipeRight : GestureType::kSwipeLeft;
        return make_result(type, std::min(0.98f, velocity.speed / 3.0f), {});
    }
    return std::nullopt;
}

GestureResult GestureClassifier::classify_two_hand(const HandLandmarks& left, const HandLandmarks& right) {
    const float dist = distance(left.points[kWrist], right.points[kWrist]);
    const float angle = std::atan2(right.points[kWrist].y - left.points[kWrist].y,
                                   right.points[kWrist].x - left.points[kWrist].x) * 57.2957795f;

    GestureResult result = make_result(GestureType::kTwoHand, 0.75f, left.points[kIndexTip]);
    if (previous_two_hand_distance_.has_value() && previous_two_hand_angle_.has_value()) {
        const float delta_dist = dist - *previous_two_hand_distance_;
        const float delta_angle = angle - *previous_two_hand_angle_;
        if (std::abs(delta_dist) > 0.02f) {
            result = make_result(GestureType::kZoom, std::min(0.95f, std::abs(delta_dist) * 10.0f), left.points[kIndexTip]);
        } else if (std::abs(delta_angle) > 5.0f) {
            result = make_result(GestureType::kRotate, std::min(0.92f, std::abs(delta_angle) / 30.0f), left.points[kIndexTip]);
        }
    }

    previous_two_hand_distance_ = dist;
    previous_two_hand_angle_ = angle;
    return result;
}

GestureResult GestureClassifier::classify_single_hand(const HandLandmarks& hand) {
    const float span = hand_span(hand);
    const int extended = count_extended(hand);
    const bool index_extended = finger_extended(hand, kIndexTip, kIndexPip);
    const bool middle_extended = finger_extended(hand, kMiddleTip, kMiddlePip);
    const float pinch_distance = distance(hand.points[kThumbTip], hand.points[kIndexTip]) / span;
    const float thumb_distance = distance(hand.points[kThumbTip], hand.points[kWrist]) / span;

    if (pinch_distance < 0.25f) {
        const float confidence = 1.0f - pinch_distance / 0.25f;
        return make_result(GestureType::kPinch, confidence, hand.points[kIndexTip]);
    }

    if (extended >= 4) {
        return make_result(GestureType::kOpenHand, std::min(0.97f, 0.85f + extended * 0.03f), hand.points[kIndexTip]);
    }

    if (extended == 0 && thumb_distance < 0.85f) {
        return make_result(GestureType::kClosedFist, 0.9f, hand.points[kWrist]);
    }

    if (index_extended && extended == 1 && hand.points[kIndexTip].y < hand.points[kIndexMcp].y - 0.05f) {
        return make_result(GestureType::kPointUp, 0.88f, hand.points[kIndexTip]);
    }

    if (index_extended && middle_extended && extended == 2) {
        return make_result(GestureType::kVSign, 0.82f, hand.points[kIndexTip]);
    }

    if (extended <= 1 && thumb_distance > 0.95f) {
        return make_result(GestureType::kGrab, 0.7f, hand.points[kWrist]);
    }

    if (extended >= 3 && hand.points[kIndexTip].z > -0.05f) {
        return make_result(GestureType::kHoverSelect, 0.6f, hand.points[kIndexTip]);
    }

    return make_result(GestureType::kNone, 0.0f, hand.points[kWrist]);
}

}  // namespace arx::vision
