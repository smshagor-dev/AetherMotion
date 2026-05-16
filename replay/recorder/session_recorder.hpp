#pragma once

#include <filesystem>
#include <fstream>
#include <string>

#include "replay/sessions/session_schema.hpp"

namespace arx::replay {

class SessionRecorder {
public:
    explicit SessionRecorder(std::filesystem::path session_dir);
    ~SessionRecorder();

    bool begin_session(const std::string& session_id);
    void record_frame(const SessionFrame& frame);
    void end_session();
    [[nodiscard]] bool recording() const noexcept;
    [[nodiscard]] const std::filesystem::path& current_path() const noexcept;

private:
    std::filesystem::path session_dir_;
    std::filesystem::path current_path_;
    std::ofstream out_;
};

}  // namespace arx::replay
