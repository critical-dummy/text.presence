#include <windows.h>

#include <algorithm>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>

#include "tpc/presence_data.hpp"
#include "tpc/provider_registry.hpp"

namespace {

constexpr unsigned int kDefaultWatchIntervalMs = 500;

#ifdef _WIN32
HANDLE g_console_output = INVALID_HANDLE_VALUE;
COORD g_presence_origin{};
bool g_presence_origin_valid = false;
#endif

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

bool save_presence_cursor() {
#ifdef _WIN32
    g_console_output = GetStdHandle(STD_OUTPUT_HANDLE);

    if (g_console_output == INVALID_HANDLE_VALUE ||
        g_console_output == nullptr) {
        return false;
    }

    CONSOLE_SCREEN_BUFFER_INFO info{};

    if (!GetConsoleScreenBufferInfo(g_console_output, &info)) {
        return false;
    }

    g_presence_origin = info.dwCursorPosition;
    g_presence_origin_valid = true;
    return true;
#else
    return false;
#endif
}

void clear_presence_line() {
#ifdef _WIN32
    if (!g_presence_origin_valid ||
        g_console_output == INVALID_HANDLE_VALUE) {
        return;
    }

    CONSOLE_SCREEN_BUFFER_INFO info{};

    if (!GetConsoleScreenBufferInfo(g_console_output, &info)) {
        return;
    }

    DWORD written = 0;

    FillConsoleOutputCharacterW(
        g_console_output,
        L' ',
        static_cast<DWORD>(info.dwSize.X),
        g_presence_origin,
        &written
    );

    FillConsoleOutputAttribute(
        g_console_output,
        info.wAttributes,
        static_cast<DWORD>(info.dwSize.X),
        g_presence_origin,
        &written
    );

    SetConsoleCursorPosition(g_console_output, g_presence_origin);
#endif
}

std::string compact_watch_status(const tpc::PresenceData& data) {
    const auto get = [&data](const char* key) -> std::string {
        const auto it = data.variables.find(key);
        return it == data.variables.end() ? std::string{} : it->second;
    };

    std::string status =
        "[TPC] " + data.application +
        " | provider=" + get("provider") +
        " | process=" + get("process") +
        " | title=" + data.title;

    return status;
}

void print_watch_presence(const tpc::PresenceData& data) {
#ifdef _WIN32
    if (!g_presence_origin_valid ||
        g_console_output == INVALID_HANDLE_VALUE) {
        return;
    }

    CONSOLE_SCREEN_BUFFER_INFO info{};

    if (!GetConsoleScreenBufferInfo(g_console_output, &info)) {
        return;
    }

    clear_presence_line();

    const std::string status = compact_watch_status(data);

    COORD origin = g_presence_origin;
    const DWORD max_width =
        info.dwSize.X > 0 ? static_cast<DWORD>(info.dwSize.X - 1) : 0;

    const DWORD write_count =
        static_cast<DWORD>(
            std::min<std::size_t>(status.size(), max_width)
        );

    if (write_count > 0) {
        DWORD written = 0;
        WriteConsoleOutputCharacterA(
            g_console_output,
            status.data(),
            write_count,
            origin,
            &written
        );

        COORD after_status = origin;
        after_status.X = static_cast<SHORT>(
            origin.X + static_cast<SHORT>(write_count)
        );

        SetConsoleCursorPosition(g_console_output, after_status);
    } else {
        SetConsoleCursorPosition(g_console_output, origin);
    }
#endif
}

void print_presence(const tpc::PresenceData& data) {
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

    enable_virtual_terminal();

    // Keep the watch renderer on its own line, below the shell prompt.
    std::cout << "\n";
    std::cout.flush();

    save_presence_cursor();

    tpc::PresenceData previous;
    bool has_previous = false;

    while (true) {
        const tpc::TargetProvider& provider = registry.detect();
        const tpc::PresenceData current = provider.capture();

        if (!has_previous || current.application != previous.application ||
            current.title != previous.title ||
            current.variables != previous.variables) {
            print_watch_presence(current);
            previous = current;
            has_previous = true;
        }

        std::this_thread::sleep_for(
            std::chrono::milliseconds(interval_ms)
        );
    }
}
