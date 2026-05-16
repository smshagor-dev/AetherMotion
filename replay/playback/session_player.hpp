#pragma once

#include <filesystem>
#include <optional>
#include <vector>

#include "replay/sessions/session_schema.hpp"

namespace arx::replay {

class SessionPlayer {
public:
    bool load(const std::filesystem::path& path);
    std::optional<SessionFrame> next();
    void reset();
    [[nodiscard]] bool loaded() const noexcept;

private:
    static std::optional<SessionFrame> parse_line(const std::string& line);

    std::vector<SessionFrame> frames_;
    std::size_t index_{0};
};

}  // namespace arx::replay
