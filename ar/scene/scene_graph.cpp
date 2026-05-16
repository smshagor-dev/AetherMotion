#include "ar/scene/scene_graph.hpp"

namespace arx::ar {

void SceneGraph::add(ARObject object) {
    objects_.push_back(std::move(object));
}

std::vector<ARObject>& SceneGraph::objects() noexcept {
    return objects_;
}

const std::vector<ARObject>& SceneGraph::objects() const noexcept {
    return objects_;
}

}  // namespace arx::ar
