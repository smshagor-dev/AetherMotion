#pragma once

#include <filesystem>

namespace arx::replay {

class CsvExporter {
public:
    static bool export_from_jsonl(const std::filesystem::path& source, const std::filesystem::path& destination);
};

}  // namespace arx::replay
