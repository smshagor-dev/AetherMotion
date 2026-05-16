#pragma once

#include <memory>
#include <string>
#include <unordered_map>

#include "engine/core/module.hpp"

namespace arx::engine {

class PluginRegistry {
public:
    void register_module(std::unique_ptr<Module> module) {
        modules_.emplace(module->name(), std::move(module));
    }

    [[nodiscard]] Module* find(const std::string& name) const {
        auto it = modules_.find(name);
        return it == modules_.end() ? nullptr : it->second.get();
    }

    [[nodiscard]] const auto& modules() const noexcept {
        return modules_;
    }

private:
    std::unordered_map<std::string, std::unique_ptr<Module>> modules_;
};

}  // namespace arx::engine
