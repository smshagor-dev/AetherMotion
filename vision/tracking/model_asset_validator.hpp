#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "engine/config/runtime_config.hpp"

namespace arx::vision::tracking {

struct ModelAssetStatus {
    std::string name;
    std::filesystem::path configured_path;
    std::filesystem::path resolved_path;
    bool exists{false};
    bool readable{false};
    std::string status;
    std::string recommended_fix;
};

struct ModelValidationReport {
    std::filesystem::path models_dir;
    std::vector<ModelAssetStatus> assets;

    [[nodiscard]] bool all_valid() const noexcept;
    [[nodiscard]] std::string summary() const;
};

[[nodiscard]] ModelAssetStatus validate_model_asset(std::string name,
                                                    const std::filesystem::path& configured_path,
                                                    const std::filesystem::path& models_dir = {});

[[nodiscard]] ModelValidationReport validate_tracking_models(const arx::engine::config::RuntimeConfig& config);

}  // namespace arx::vision::tracking
