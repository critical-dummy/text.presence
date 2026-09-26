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

} // namespace

namespace tpc {

const char* FlStudioProvider::id() const {
    return "fl_studio";
}

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
    data.application = id();
    data.title = window_title.empty() ? "FL Studio" : window_title;

    data.variables["process"] = process_name;
    data.variables["window"] = window_title;
    data.variables["process_id"] = std::to_string(process_id);
    data.variables["provider"] = id();
    data.variables["source"] = "windows";

    return data;
}

} // namespace tpc
