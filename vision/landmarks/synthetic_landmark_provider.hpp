#pragma once

#include "vision/landmarks/landmark_provider.hpp"

namespace arx::vision::landmarks {

class SyntheticLandmarkProvider final : public LandmarkProvider {
public:
    explicit SyntheticLandmarkProvider(const engine::config::RuntimeConfig& config);

    [[nodiscard]] bool initialize() override;
    [[nodiscard]] LandmarkProviderOutput process(tracking::VideoFrame& frame) override;
    [[nodiscard]] bool available() const noexcept override;
    [[nodiscard]] const std::string& last_error() const noexcept override;
    [[nodiscard]] const char* name() const noexcept override;

private:
    engine::config::RuntimeConfig config_;
    bool available_{false};
    std::string last_error_;
};

}  // namespace arx::vision::landmarks
