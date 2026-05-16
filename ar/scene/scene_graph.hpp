#pragma once

#include <vector>

#include "ar/objects/ar_object.hpp"

namespace arx::ar {

class SceneGraph {
public:
    void add(ARObject object);
    [[nodiscard]] std::vector<ARObject>& objects() noexcept;
    [[nodiscard]] const std::vector<ARObject>& objects() const noexcept;

private:
    std::vector<ARObject> objects_;
};

}  // namespace arx::ar
