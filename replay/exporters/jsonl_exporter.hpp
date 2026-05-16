#pragma once

#include <filesystem>

namespace arx::replay {

class JsonlExporter {
public:
    static bool copy_session(const std::filesystem::path& source, const std::filesystem::path& destination);
};

}  // namespace arx::replay
