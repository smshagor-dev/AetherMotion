#include "replay/exporters/csv_exporter.hpp"

#include <fstream>
#include <regex>
#include <string>

namespace arx::replay {

bool CsvExporter::export_from_jsonl(const std::filesystem::path& source, const std::filesystem::path& destination) {
    std::ifstream in(source);
    if (!in.is_open()) {
        return false;
    }

    std::ofstream out(destination, std::ios::out | std::ios::trunc);
    if (!out.is_open()) {
        return false;
    }

    out << "timestamp_us,frame_id,fps,latency_ms,gesture,confidence\n";

    const std::regex re("\"timestamp_us\":([0-9]+).*\"frame_id\":([0-9]+).*\"fps\":([0-9.]+).*\"latency_ms\":([0-9.]+).*\"label\":\"([^\"]+)\".*\"confidence\":([0-9.]+)");
    std::smatch match;
    std::string line;
    while (std::getline(in, line)) {
        if (std::regex_search(line, match, re)) {
            out << match[1].str() << ","
                << match[2].str() << ","
                << match[3].str() << ","
                << match[4].str() << ","
                << match[5].str() << ","
                << match[6].str() << "\n";
        }
    }
    return true;
}

}  // namespace arx::replay
