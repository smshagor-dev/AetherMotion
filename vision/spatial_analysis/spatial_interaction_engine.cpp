#include "vision/spatial_analysis/spatial_interaction_engine.hpp"

namespace arx::vision {

SpatialInteraction SpatialInteractionEngine::update(const GestureResult& gesture) const {
    SpatialInteraction interaction;
    interaction.anchor = gesture.focus;

    switch (gesture.gesture) {
    case GestureType::kPinch:
        interaction.mode = "select";
        interaction.depth_bias = -0.1f;
        interaction.selected = true;
        break;
    case GestureType::kPointUp:
    case GestureType::kHoverSelect:
        interaction.mode = "hover";
        interaction.depth_bias = 0.0f;
        break;
    case GestureType::kZoom:
        interaction.mode = "zoom";
        interaction.depth_bias = -0.2f;
        break;
    case GestureType::kRotate:
        interaction.mode = "rotate";
        interaction.depth_bias = -0.15f;
        break;
    case GestureType::kGrab:
        interaction.mode = "grab";
        interaction.depth_bias = -0.05f;
        interaction.selected = true;
        break;
    default:
        interaction.mode = "idle";
        break;
    }
    return interaction;
}

}  // namespace arx::vision
