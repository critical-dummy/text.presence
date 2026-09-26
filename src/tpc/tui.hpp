#pragma once

#include <string>
#include <vector>

#include "presence_data.hpp"
#include "target_config.hpp"

namespace tpc {

class Tui {
public:
    ~Tui();

    bool start();
    void stop();

    bool active() const;

    void render(
        const TargetConfig& config,
        const PresenceData& data
    );

private:
    void clear();
    void write_line(
        short row,
        const std::wstring& text,
        short width
    );

#ifdef _WIN32
    void* original_buffer_ = nullptr;
    void* tui_buffer_ = nullptr;
#endif

    bool active_ = false;
};

} // namespace tpc
