#include <windows.h>

#include <chrono>
#include <iostream>
#include <string>
#include <thread>

#include "tpc/presence_data.hpp"
#include "tpc/provider_registry.hpp"

namespace {

constexpr unsigned int kDefaultWatchIntervalMs = 500;

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

void clear_previous_presence(std::size_t line_count) {
#ifdef _WIN32
    std::cout << "\x1b[" << line_count << "A";
    std::cout << "\x1b[0J";
#endif
}

std::size_t print_presence(const tpc::PresenceData& data) {
    std::cout << "{\n";
    std::cout << "  \"application\": \""
              << json_escape(data.application) << "\",\n";
    std::cout << "  \"title\": \""
              << json_escape(data.title) << "\",\n";
    std::cout << "  \"variables\": {\n";

    bool first = true;
    for (const auto& [key, value] : data.variables) {
        if (!first) {
            std::cout << ",\n";
        }
        first = false;
        std::cout << "    \"" << json_escape(key) << "\": \""
                  << json_escape(value) << "\"";
    }

    std::cout << "\n  }\n";
    std::cout << "}\n";
    std::cout.flush();

    // { + application + title + variables + N variable lines + } + }
    return 6 + data.variables.size();
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

    tpc::ProviderRegistry registry;

    if (!watch) {
        print_presence(registry.detect().capture());
        return 0;
    }

    const bool virtual_terminal = enable_virtual_terminal();
    tpc::PresenceData previous;
    std::size_t previous_line_count = 0;
    bool has_previous = false;

    while (true) {
        const tpc::TargetProvider& provider = registry.detect();
        const tpc::PresenceData current = provider.capture();

        if (!has_previous || current.application != previous.application ||
            current.title != previous.title ||
            current.variables != previous.variables) {
            if (has_previous && virtual_terminal) {
                clear_previous_presence(previous_line_count);
            }

            const std::size_t current_line_count = print_presence(current);
            previous_line_count = current_line_count;
            previous = current;
            has_previous = true;
        }

        std::this_thread::sleep_for(
            std::chrono::milliseconds(interval_ms)
        );
    }
}
