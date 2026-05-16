#pragma once

#include <cstdint>
#include <string>

namespace arx::ar {

enum class ObjectType : std::uint8_t {
    kCube,
    kSphere,
    kPanel,
    kHudCard,
    kGestureIndicator,
    kInteractionAnchor,
};

struct Transform {
    float x{0.0f};
    float y{0.0f};
    float z{0.0f};
    float sx{1.0f};
    float sy{1.0f};
    float sz{1.0f};
};

struct ARObject {
    std::uint64_t id{0};
    ObjectType type{ObjectType::kCube};
    std::string name;
    Transform transform;
    bool selectable{true};
};

}  // namespace arx::ar
