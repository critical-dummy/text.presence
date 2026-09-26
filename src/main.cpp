#include <windows.h>

#include <chrono>
#include <iostream>
#include <string>
#include <thread>

#include "tpc/presence_data.hpp"
#include "tpc/target_detector.hpp"

namespace {

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
}

} // namespace

int main(int argc, char* argv[]) {
    bool watch = false;
    unsigned int interval_ms = 500;

    if (argc >= 2 && std::string(argv[1]) == "--watch") {
        watch = true;
    }

    if (argc >= 3) {
        try {
            interval_ms = static_cast<unsigned int>(std::stoul(argv[2]));
            if (interval_ms == 0) interval_ms = 500;
        } catch (...) {
            std::cerr << "Invalid interval: " << argv[2] << "\n";
            return 2;
        }
    }

    do {
        print_presence(capture_foreground());

        if (!watch) {
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms));
    } while (true);

    return 0;
}
