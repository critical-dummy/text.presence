#include "tui.hpp"

#ifdef _WIN32
#include <windows.h>
#endif

#include <algorithm>
#include <string>
#include <vector>

namespace {

#ifdef _WIN32

HANDLE as_handle(void* value) {
    return static_cast<HANDLE>(value);
}

std::wstring wide_from_utf8(const std::string& value) {
    if (value.empty()) return {};

    const int size = MultiByteToWideChar(
        CP_UTF8,
        0,
        value.data(),
        static_cast<int>(value.size()),
        nullptr,
        0
    );

    if (size <= 0) return {};

    std::wstring result(static_cast<std::size_t>(size), L'\0');

    MultiByteToWideChar(
        CP_UTF8,
        0,
        value.data(),
        static_cast<int>(value.size()),
        result.data(),
        size
    );

    return result;
}

#endif

}

namespace tpc {

Tui::~Tui() {
    stop();
}

bool Tui::start() {
#ifdef _WIN32
    if (active_) return true;

    HANDLE original = GetStdHandle(STD_OUTPUT_HANDLE);

    if (original == INVALID_HANDLE_VALUE || original == nullptr) {
        return false;
    }

    HANDLE buffer = CreateConsoleScreenBuffer(
        GENERIC_READ | GENERIC_WRITE,
        0,
        nullptr,
        CONSOLE_TEXTMODE_BUFFER,
        nullptr
    );

    if (buffer == INVALID_HANDLE_VALUE) {
        return false;
    }

    if (!SetConsoleActiveScreenBuffer(buffer)) {
        CloseHandle(buffer);
        return false;
    }

    original_buffer_ = original;
    tui_buffer_ = buffer;
    active_ = true;

    CONSOLE_CURSOR_INFO cursor{};
    cursor.dwSize = 1;
    cursor.bVisible = FALSE;
    SetConsoleCursorInfo(buffer, &cursor);

    clear();
    return true;
#else
    return false;
#endif
}

void Tui::stop() {
#ifdef _WIN32
    if (!active_) return;

    HANDLE original = as_handle(original_buffer_);
    HANDLE buffer = as_handle(tui_buffer_);

    if (original != INVALID_HANDLE_VALUE && original != nullptr) {
        SetConsoleActiveScreenBuffer(original);
    }

    if (buffer != INVALID_HANDLE_VALUE && buffer != nullptr) {
        CloseHandle(buffer);
    }

    original_buffer_ = nullptr;
    tui_buffer_ = nullptr;
#endif

    active_ = false;
}

bool Tui::active() const {
    return active_;
}

void Tui::clear() {
#ifdef _WIN32
    HANDLE buffer = as_handle(tui_buffer_);
    if (buffer == INVALID_HANDLE_VALUE || buffer == nullptr) return;

    CONSOLE_SCREEN_BUFFER_INFO info{};

    if (!GetConsoleScreenBufferInfo(buffer, &info)) return;

    const DWORD cells =
        static_cast<DWORD>(info.dwSize.X) *
        static_cast<DWORD>(info.dwSize.Y);

    DWORD written = 0;

    FillConsoleOutputCharacterW(
        buffer,
        L' ',
        cells,
        COORD{0, 0},
        &written
    );

    FillConsoleOutputAttribute(
        buffer,
        info.wAttributes,
        cells,
        COORD{0, 0},
        &written
    );

    SetConsoleCursorPosition(buffer, COORD{0, 0});
#endif
}

void Tui::write_line(
    short row,
    const std::wstring& text,
    short width
) {
#ifdef _WIN32
    HANDLE buffer = as_handle(tui_buffer_);
    if (buffer == INVALID_HANDLE_VALUE || buffer == nullptr) return;

    if (row < 0 || width <= 0) return;

    std::wstring output = text;

    const std::size_t max_length =
        static_cast<std::size_t>(std::max<short>(width - 1, 0));

    if (output.size() > max_length) {
        output.resize(max_length);
    }

    DWORD written = 0;

    WriteConsoleOutputCharacterW(
        buffer,
        output.c_str(),
        static_cast<DWORD>(output.size()),
        COORD{0, row},
        &written
    );
#endif
}

void Tui::render(
    const TargetConfig& config,
    const PresenceData& data
) {
#ifdef _WIN32
    if (!active_) return;

    HANDLE buffer = as_handle(tui_buffer_);

    CONSOLE_SCREEN_BUFFER_INFO info{};

    if (!GetConsoleScreenBufferInfo(buffer, &info)) return;

    clear();

    const short screen_width = info.dwSize.X;
    const short screen_height = info.dwSize.Y;

    if (screen_width < 10 || screen_height < 6) {
        return;
    }

    const std::string rpc_title =
        config.tpc_rpc.title.empty()
            ? data.application
            : expand_variables(config.tpc_rpc.title, data);

    std::vector<std::string> raw_lines = config.tpc_rpc.lines;

    if (raw_lines.empty()) {
        raw_lines = {
            "Application: {application}",
            "Provider: {provider}",
            "Process: {process}",
            "PID: {process_id}",
            "Window: {window}"
        };
    }

    std::vector<std::wstring> body;
    body.reserve(raw_lines.size());

    for (const std::string& line : raw_lines) {
        body.push_back(
            wide_from_utf8(expand_variables(line, data))
        );
    }

    std::wstring title = wide_from_utf8(
        config.tpc_title.empty()
            ? "Text Presence"
            : config.tpc_title
    );

    std::wstring title_line = L"  ";
    title_line += title;

    const std::size_t max_body_length = [&body]() {
        std::size_t value = 0;
        for (const auto& line : body) {
            value = std::max(value, line.size());
        }
        return value;
    }();

    const std::size_t title_length = title_line.size();
    const std::size_t rpc_title_length = wide_from_utf8(rpc_title).size();

    std::size_t box_width = std::max({
        std::size_t(42),
        title_length + 4,
        rpc_title_length + 4,
        max_body_length + 4
    });

    box_width = std::min(
        box_width,
        static_cast<std::size_t>(screen_width - 2)
    );

    const std::wstring border =
        L"+" + std::wstring(box_width - 2, L'-') + L"+";

    const std::wstring empty =
        L"|" + std::wstring(box_width - 2, L' ') + L"|";

    const auto boxed = [box_width](const std::wstring& value) {
        std::wstring line = value;

        if (line.size() > box_width - 4) {
            line.resize(box_width - 4);
        }

        line =
            L"| " + line +
            std::wstring(
                box_width - 3 - line.size(),
                L' '
            ) +
            L"|";

        return line;
    };

    short row = 0;

    write_line(row++, L" TPC", screen_width);
    write_line(row++, border, screen_width);
    write_line(row++, boxed(title), screen_width);
    write_line(row++, empty, screen_width);
    write_line(
        row++,
        boxed(wide_from_utf8(rpc_title)),
        screen_width
    );

    for (const auto& line : body) {
        if (row >= screen_height - 1) break;
        write_line(row++, boxed(line), screen_width);
    }

    if (row < screen_height - 1) {
        write_line(row++, empty, screen_width);
    }

    if (row < screen_height) {
        write_line(row++, border, screen_width);
    }

    SetConsoleCursorPosition(buffer, COORD{0, 0});
#else
    (void)config;
    (void)data;
#endif
}

} // namespace tpc
