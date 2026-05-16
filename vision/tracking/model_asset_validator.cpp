#include "vision/tracking/model_asset_validator.hpp"

#include <fstream>
#include <sstream>

namespace arx::vision::tracking {

namespace {

std::filesystem::path resolve_model_path(const std::filesystem::path& configured_path,
                                         const std::filesystem::path& models_dir) {
    if (configured_path.empty()) {
        return {};
    }
    if (configured_path.is_absolute()) {
        return configured_path.lexically_normal();
    }
    if (!models_dir.empty()) {
        return std::filesystem::absolute(models_dir / configured_path.filename()).lexically_normal();
    }
    return std::filesystem::absolute(configured_path).lexically_normal();
}

}

bool ModelValidationReport::all_valid() const noexcept {
    for (const auto& asset : assets) {
        if (!asset.exists || !asset.readable) {
            return false;
        }
    }
    return true;
}

std::string ModelValidationReport::summary() const {
    std::ostringstream out;
    out << "Models dir: " << (models_dir.empty() ? "<not set>" : models_dir.string()) << '\n';
    for (const auto& asset : assets) {
        out << asset.name << ": " << asset.status
            << " | configured=" << asset.configured_path.string()
            << " | resolved=" << asset.resolved_path.string();
        if (!asset.recommended_fix.empty()) {
            out << " | fix=" << asset.recommended_fix;
        }
        out << '\n';
    }
    return out.str();
}

ModelAssetStatus validate_model_asset(std::string name,
                                      const std::filesystem::path& configured_path,
                                      const std::filesystem::path& models_dir) {
    ModelAssetStatus status;
    status.name = std::move(name);
    status.configured_path = configured_path;
    status.resolved_path = resolve_model_path(configured_path, models_dir);
    status.exists = !status.resolved_path.empty() && std::filesystem::exists(status.resolved_path);
    if (status.exists) {
        std::ifstream in(status.resolved_path, std::ios::binary);
        status.readable = in.good();
    }

    if (!status.exists) {
        status.status = "missing";
        status.recommended_fix = "Place the .task file at the resolved path or update ARX_MEDIAPIPE_MODELS_DIR/runtime config.";
    } else if (!status.readable) {
        status.status = "unreadable";
        status.recommended_fix = "Verify file permissions and that the .task file is not locked by another process.";
    } else {
        status.status = "found";
    }

    return status;
}

ModelValidationReport validate_tracking_models(const arx::engine::config::RuntimeConfig& config) {
    ModelValidationReport report;
    report.models_dir = config.tracking.models_dir.empty()
        ? std::filesystem::absolute("models")
        : std::filesystem::absolute(config.tracking.models_dir);
    report.assets.push_back(validate_model_asset("hand model", config.tracking.hand_model_path, report.models_dir));
    report.assets.push_back(validate_model_asset("face model", config.tracking.face_model_path, report.models_dir));
    return report;
}

}  // namespace arx::vision::tracking
