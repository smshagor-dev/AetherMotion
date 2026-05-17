#pragma once

#include <memory>

#include "vision/landmarks/landmark_provider.hpp"

namespace arx::vision::landmarks {

class ProductionMediaPipeLandmarkProvider final : public LandmarkProvider {
public:
    explicit ProductionMediaPipeLandmarkProvider(const engine::config::RuntimeConfig& config);
    ~ProductionMediaPipeLandmarkProvider() override;

    [[nodiscard]] bool initialize() override;
    [[nodiscard]] LandmarkProviderOutput process(tracking::VideoFrame& frame) override;
    [[nodiscard]] bool available() const noexcept override;
    [[nodiscard]] const std::string& last_error() const noexcept override;
    [[nodiscard]] const char* name() const noexcept override;

private:
    engine::config::RuntimeConfig config_;
    std::unique_ptr<tracking::LandmarkRuntimeBridge> bridge_;
    bool available_{false};
    std::string last_error_;
};

using ExternalLandmarkProvider = ProductionMediaPipeLandmarkProvider;

}  // namespace arx::vision::landmarks
