#include "fl_studio.hpp"

#include "tpc/target_detector.hpp"

#ifdef _WIN32
#include <windows.h>
#endif

#include <filesystem>
#include <fstream>
#include <map>
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

std::string unescape_value(const std::string& value) {
    std::string result;
    result.reserve(value.size());

    bool escaped = false;

    for (const char ch : value) {
        if (escaped) {
            switch (ch) {
            case 't':
                result.push_back('\t');
                break;
            case 'r':
                result.push_back('\r');
                break;
            case 'n':
                result.push_back('\n');
                break;
            case '\\':
                result.push_back('\\');
                break;
            default:
                result.push_back(ch);
                break;
            }
            escaped = false;
            continue;
        }

        if (ch == '\\') {
            escaped = true;
        } else {
            result.push_back(ch);
        }
    }

    if (escaped) {
        result.push_back('\\');
    }

    return result;
}

std::map<std::string, std::string> read_bridge_report() {
    std::map<std::string, std::string> values;

#ifdef _WIN32
    const wchar_t* local_appdata = _wgetenv(L"LOCALAPPDATA");
    if (local_appdata == nullptr || *local_appdata == L'\0') {
        return values;
    }

    const std::filesystem::path report_path =
        std::filesystem::path(local_appdata) /
        L"TPC" /
        L"fl_studio.report";

    std::ifstream input(report_path);
    if (!input) {
        return values;
    }
#else
    return values;
#endif

    std::string line;
    if (!std::getline(input, line) || line != "TPC1") {
        return {};
    }

    while (std::getline(input, line)) {
        const std::size_t separator = line.find('=');
        if (separator == std::string::npos) {
            continue;
        }

        const std::string key = line.substr(0, separator);
        const std::string value = line.substr(separator + 1);

        values[key] = unescape_value(value);
    }

    return values;
}

std::string get_value(
    const std::map<std::string, std::string>& values,
    const std::string& key
) {
    const auto iterator = values.find(key);
    if (iterator == values.end()) {
        return {};
    }

    return iterator->second;
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

    const std::map<std::string, std::string> report =
        read_bridge_report();

    PresenceData data;
    data.application = "fl_studio";
    data.title = window_title.empty() ? "FL Studio" : window_title;

    data.variables["process"] = process_name;
    data.variables["window"] = window_title;
    data.variables["process_id"] = std::to_string(process_id);
    data.variables["provider"] = "fl_studio";

    const std::string bridge_process_id =
        get_value(report, "process_id");

    if (bridge_process_id == std::to_string(process_id)) {
        data.variables["bridge"] = "connected";
        data.variables["project"] =
            get_value(report, "project_title");
        data.variables["author"] =
            get_value(report, "project_author");
        data.variables["genre"] =
            get_value(report, "project_genre");
        data.variables["tempo"] =
            get_value(report, "tempo");
        data.variables["playing"] =
            get_value(report, "playing");
        data.variables["recording"] =
            get_value(report, "recording");
        data.variables["song_pos"] =
            get_value(report, "song_pos");
        data.variables["song_length"] =
            get_value(report, "song_length");
        data.variables["progress"] =
            get_value(report, "progress");
        data.variables["pattern_number"] =
            get_value(report, "pattern_number");
        data.variables["pattern_name"] =
            get_value(report, "pattern_name");
    } else {
        data.variables["bridge"] = "disconnected";
    }

    return data;
}

} // namespace tpc
