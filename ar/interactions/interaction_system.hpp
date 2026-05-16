#pragma once

#include <optional>

#include "ar/scene/scene_graph.hpp"
#include "vision/spatial_analysis/spatial_interaction_engine.hpp"

namespace arx::ar {

struct InteractionResult {
    std::optional<std::uint64_t> selected_object_id;
    bool manipulated{false};
};

class InteractionSystem {
public:
    InteractionResult update(SceneGraph& scene, const vision::SpatialInteraction& interaction) const;
};

}  // namespace arx::ar
