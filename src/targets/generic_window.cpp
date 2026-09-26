#include "generic_window.hpp"

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

bool GenericWindowProvider::matches() const {
    return true;
}

PresenceData GenericWindowProvider::capture() const {
    const unsigned long process_id =
        TargetDetector::foreground_process_id();

    const std::string process_name =
        utf8_from_wide(TargetDetector::foreground_process_name());

    const std::string window_title =
        utf8_from_wide(TargetDetector::foreground_window_title());

    PresenceData data;
    data.application = process_name.empty() ? "unknown" : process_name;
    data.title = window_title.empty() ? process_name : window_title;
    data.variables["process"] = process_name;
    data.variables["window"] = window_title;
    data.variables["process_id"] = std::to_string(process_id);
    data.variables["provider"] = "generic_window";

    return data;
}

} // namespace tpc
