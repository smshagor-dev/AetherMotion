#include "ar/interactions/interaction_system.hpp"

namespace arx::ar {

InteractionResult InteractionSystem::update(SceneGraph& scene, const vision::SpatialInteraction& interaction) const {
    InteractionResult result;
    if (scene.objects().empty()) {
        return result;
    }

    auto& first = scene.objects().front();
    if (interaction.mode == "select") {
        result.selected_object_id = first.id;
    } else if (interaction.mode == "scale") {
        first.transform.sx += 0.01f;
        first.transform.sy += 0.01f;
        first.transform.sz += 0.01f;
        result.manipulated = true;
    }
    return result;
}

}  // namespace arx::ar
