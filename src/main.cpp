#include <windows.h>

#include <chrono>
#include <iostream>
#include <string>
#include <thread>

#include "tpc/presence_data.hpp"
#include "tpc/target_detector.hpp"

namespace {

constexpr unsigned int kDefaultWatchIntervalMs = 500;
constexpr unsigned int kPresenceLineCount = 8;

std::string utf8_from_wide(const std::wstring& value) {
    if (value.empty()) return {};

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
}

std::string json_escape(const std::string& value) {
    std::string result;
    result.reserve(value.size() + 8);

    for (const unsigned char ch : value) {
        switch (ch) {
        case '\\':
            result += "\\\\";
            break;
        case '"':
            result += "\\\"";
            break;
        case '\b':
            result += "\\b";
            break;
        case '\f':
            result += "\\f";
            break;
        case '\n':
            result += "\\n";
            break;
        case '\r':
            result += "\\r";
            break;
        case '\t':
            result += "\\t";
            break;
        default:
            if (ch < 0x20) {
                static constexpr char hex[] = "0123456789abcdef";
                result += "\\u00";
                result += hex[(ch >> 4) & 0x0f];
                result += hex[ch & 0x0f];
            } else {
                result += static_cast<char>(ch);
            }
            break;
        }
    }

    return result;
}

tpc::PresenceData capture_foreground() {
    const unsigned long process_id =
        tpc::TargetDetector::foreground_process_id();

    const std::string process_name =
        utf8_from_wide(tpc::TargetDetector::foreground_process_name());

    const std::string window_title =
        utf8_from_wide(tpc::TargetDetector::foreground_window_title());

    tpc::PresenceData data;
    data.application = process_name.empty() ? "unknown" : process_name;
    data.title = window_title.empty() ? process_name : window_title;
    data.variables["process"] = process_name;
    data.variables["window"] = window_title;
    data.variables["process_id"] = std::to_string(process_id);

    return data;
}

void clear_previous_presence() {
#ifdef _WIN32
    // The presence block is always 8 lines tall. In watch mode the cursor
    // sits on the line immediately after that block.
    std::cout << "\x1b[" << kPresenceLineCount << "A";
    std::cout << "\x1b[0J";
#endif
}

void print_presence(const tpc::PresenceData& data) {
    std::cout << "{\n";
    std::cout << "  \"application\": \""
              << json_escape(data.application) << "\",\n";
    std::cout << "  \"title\": \""
              << json_escape(data.title) << "\",\n";
    std::cout << "  \"variables\": {\n";
    std::cout << "    \"process\": \""
              << json_escape(data.variables.at("process")) << "\",\n";
    std::cout << "    \"window\": \""
              << json_escape(data.variables.at("window")) << "\",\n";
    std::cout << "    \"process_id\": \""
              << json_escape(data.variables.at("process_id")) << "\"\n";
    std::cout << "  }\n";
    std::cout << "}\n";
    std::cout.flush();
}

bool enable_virtual_terminal() {
#ifdef _WIN32
    HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
    if (output == INVALID_HANDLE_VALUE || output == nullptr) {
        return false;
    }

    DWORD mode = 0;
    if (!GetConsoleMode(output, &mode)) {
        return false;
    }

    mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    return SetConsoleMode(output, mode) != FALSE;
#else
    return false;
#endif
}

} // namespace

int main(int argc, char* argv[]) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    bool watch = false;
    unsigned int interval_ms = kDefaultWatchIntervalMs;

    if (argc >= 2 && std::string(argv[1]) == "--watch") {
        watch = true;
    }

    if (argc >= 3) {
        try {
            interval_ms = static_cast<unsigned int>(std::stoul(argv[2]));
            if (interval_ms == 0) {
                interval_ms = kDefaultWatchIntervalMs;
            }
        } catch (...) {
            std::cerr << "Invalid interval: " << argv[2] << "\n";
            return 2;
        }
    }

    if (!watch) {
        print_presence(capture_foreground());
        return 0;
    }

    const bool virtual_terminal = enable_virtual_terminal();
    tpc::PresenceData previous;
    bool has_previous = false;

    while (true) {
        const tpc::PresenceData current = capture_foreground();

        if (!has_previous || current.application != previous.application ||
            current.title != previous.title ||
            current.variables != previous.variables) {
            if (has_previous && virtual_terminal) {
                clear_previous_presence();
            }

            print_presence(current);
            previous = current;
            has_previous = true;
        }

        std::this_thread::sleep_for(
            std::chrono::milliseconds(interval_ms)
        );
    }
}
