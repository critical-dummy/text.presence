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

bool has_flp_extension(const std::wstring& value) {
    if (value.size() < 4) return false;

    const std::wstring extension = value.substr(value.size() - 4);
#ifdef _WIN32
    return _wcsicmp(extension.c_str(), L".flp") == 0;
#else
    return false;
#endif
}

std::wstring trim_quotes(const std::wstring& value) {
    if (value.size() >= 2 &&
        value.front() == L'"' &&
        value.back() == L'"') {
        return value.substr(1, value.size() - 2);
    }

    return value;
}

std::wstring extract_flp_argument(const std::wstring& command_line) {
    std::size_t position = 0;

    while (position < command_line.size()) {
        while (position < command_line.size() &&
               (command_line[position] == L' ' ||
                command_line[position] == L'\t')) {
            ++position;
        }

        if (position >= command_line.size()) {
            break;
        }

        std::wstring argument;

        if (command_line[position] == L'"') {
            const std::size_t start = position++;
            while (position < command_line.size()) {
                if (command_line[position] == L'"') {
                    ++position;
                    break;
                }
                ++position;
            }
            argument = command_line.substr(start, position - start);
        } else {
            const std::size_t start = position;
            while (position < command_line.size() &&
                   command_line[position] != L' ' &&
                   command_line[position] != L'\t') {
                ++position;
            }
            argument = command_line.substr(start, position - start);
        }

        argument = trim_quotes(argument);

        if (has_flp_extension(argument)) {
            return argument;
        }
    }

    return {};
}

std::wstring filename_without_extension(const std::wstring& path) {
    const std::wstring::size_type separator =
        path.find_last_of(L"\/");

    const std::wstring filename =
        separator == std::wstring::npos
            ? path
            : path.substr(separator + 1);

    const std::wstring::size_type dot =
        filename.rfind(L'.');

    if (dot == std::wstring::npos) {
        return filename;
    }

    return filename.substr(0, dot);
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

    const std::wstring command_line =
        TargetDetector::foreground_process_command_line();

    PresenceData data;
    data.application = id();
    data.title = window_title.empty() ? "FL Studio" : window_title;

    data.variables["process"] = process_name;
    data.variables["window"] = window_title;
    data.variables["process_id"] = std::to_string(process_id);
    data.variables["provider"] = id();
    data.variables["source"] = "windows";

    const std::wstring project_path =
        extract_flp_argument(command_line);

    if (!project_path.empty()) {
        data.variables["project_path"] =
            utf8_from_wide(project_path);
        data.variables["project"] =
            utf8_from_wide(filename_without_extension(project_path));
    }

    return data;
}

} // namespace tpc
