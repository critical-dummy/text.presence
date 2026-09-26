#include "fl_studio.hpp"

#include "tpc/target_detector.hpp"

#ifdef _WIN32
#include <windows.h>
#endif

#include <string>

namespace {

std::string utf8_from_wide(const std::wstring& value) {
    if (value.empty()) return {};

#ifdef _WIN32
    const int size = WideCharToMultiByte(
        CP_UTF8,
        0,
        value.data(),
        static_cast<int>(value.size()),
        nullptr,
        0,
        nullptr,
        nullptr
    );

    if (size <= 0) return {};

    std::string result(static_cast<size_t>(size), '\0');

    WideCharToMultiByte(
        CP_UTF8,
        0,
        value.data(),
        static_cast<int>(value.size()),
        result.data(),
        size,
        nullptr,
        nullptr
    );

    return result;
#else
    return {};
#endif
}

std::string project_hint_from_title(const std::string& title) {
    if (title.empty()) return {};

    // Current FL Studio window titles can include the current project name.
    // Keep this deliberately conservative: do not guess when only the
    // application/version title is visible.
    static constexpr const char* suffixes[] = {
        " - FL Studio 2026",
        " - FL Studio 2025",
        " - FL Studio 24",
        " - FL Studio 21"
    };

    for (const char* suffix : suffixes) {
        const std::string suffix_string(suffix);
        if (title.size() > suffix_string.size() &&
            title.compare(
                title.size() - suffix_string.size(),
                suffix_string.size(),
                suffix_string
            ) == 0) {
            return title.substr(0, title.size() - suffix_string.size());
        }
    }

    return {};
}

} // namespace

namespace tpc {

bool FlStudioProvider::matches() const {
    const std::wstring process =
        TargetDetector::foreground_process_name();

#ifdef _WIN32
    return _wcsicmp(process.c_str(), L"FL64.exe") == 0 ||
           _wcsicmp(process.c_str(), L"FL.exe") == 0;
#else
    return false;
#endif
}

PresenceData FlStudioProvider::capture() const {
    const unsigned long process_id =
        TargetDetector::foreground_process_id();

    const std::string process_name =
        utf8_from_wide(TargetDetector::foreground_process_name());

    const std::string window_title =
        utf8_from_wide(TargetDetector::foreground_window_title());

    PresenceData data;
    data.application = "fl_studio";
    data.title = window_title.empty() ? "FL Studio" : window_title;

    data.variables["process"] = process_name;
    data.variables["window"] = window_title;
    data.variables["process_id"] = std::to_string(process_id);
    data.variables["provider"] = "fl_studio";
    data.variables["source"] = "windows";

    const std::string project_hint = project_hint_from_title(window_title);
    if (!project_hint.empty()) {
        data.variables["project_hint"] = project_hint;
    }

    return data;
}

} // namespace tpc
