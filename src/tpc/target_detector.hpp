#pragma once
#include <string>

namespace tpc {

class TargetDetector {
public:
    static bool process_exists(const std::wstring& process_name);

    static unsigned long foreground_process_id();
    static std::wstring foreground_process_name();
    static std::wstring foreground_window_title();
};

} // namespace tpc
