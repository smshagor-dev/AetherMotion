#pragma once

#include <cstdint>
#include <string>

#include "vision/landmarks/landmark_types.hpp"

namespace arx::vision {

enum class GestureType : std::uint8_t {
    kNone = 0,
    kOpenHand = 1,
    kClosedFist = 2,
    kPinch = 3,
    kSwipeLeft = 4,
    kSwipeRight = 5,
    kTwoHand = 6,
    kRotate = 7,
    kZoom = 8,
    kPointUp = 9,
    kVSign = 10,
    kGrab = 11,
    kRelease = 12,
    kHoverSelect = 13,
};

enum class GestureEventKind : std::uint8_t {
    kStarted = 0,
    kHeld = 1,
    kReleased = 2,
    kChanged = 3,
};

struct GestureResult {
    GestureType gesture{GestureType::kNone};
    float confidence{0.0f};
    std::string label{"none"};
    Point3f focus{};
};

inline const char* gesture_name(GestureType gesture) {
    switch (gesture) {
    case GestureType::kOpenHand: return "open_hand";
    case GestureType::kClosedFist: return "closed_fist";
    case GestureType::kPinch: return "pinch";
    case GestureType::kSwipeLeft: return "swipe_left";
    case GestureType::kSwipeRight: return "swipe_right";
    case GestureType::kTwoHand: return "two_hand";
    case GestureType::kRotate: return "rotate";
    case GestureType::kZoom: return "zoom";
    case GestureType::kPointUp: return "point_up";
    case GestureType::kVSign: return "v_sign";
    case GestureType::kGrab: return "grab";
    case GestureType::kRelease: return "release";
    case GestureType::kHoverSelect: return "hover_select";
    default: return "none";
    }
}

inline const char* gesture_event_name(GestureEventKind kind) {
    switch (kind) {
    case GestureEventKind::kStarted: return "STARTED";
    case GestureEventKind::kHeld: return "HELD";
    case GestureEventKind::kReleased: return "RELEASED";
    case GestureEventKind::kChanged: return "CHANGED";
    default: return "UNKNOWN";
    }
}

}  // namespace arx::vision
