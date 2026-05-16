#include "replay/exporters/jsonl_exporter.hpp"

#include <filesystem>

namespace arx::replay {

bool JsonlExporter::copy_session(const std::filesystem::path& source, const std::filesystem::path& destination) {
    std::error_code ec;
    std::filesystem::copy_file(source, destination, std::filesystem::copy_options::overwrite_existing, ec);
    return !ec;
}

}  // namespace arx::replay
