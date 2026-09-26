#include "target_detector.hpp"

#ifdef _WIN32
#include <windows.h>
#include <tlhelp32.h>

namespace tpc {

bool TargetDetector::process_exists(const std::wstring& process_name) {
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return false;

    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);

    bool found = false;
    if (Process32FirstW(snapshot, &entry)) {
        do {
            if (process_name == entry.szExeFile) {
                found = true;
                break;
            }
        } while (Process32NextW(snapshot, &entry));
    }

    CloseHandle(snapshot);
    return found;
}

std::wstring TargetDetector::foreground_window_title() {
    HWND hwnd = GetForegroundWindow();
    if (!hwnd) return {};

    int length = GetWindowTextLengthW(hwnd);
    if (length <= 0) return {};

    std::wstring title(static_cast<size_t>(length), L'\0');
    GetWindowTextW(hwnd, title.data(), length + 1);
    return title;
}

} // namespace tpc
#else

namespace tpc {
bool TargetDetector::process_exists(const std::wstring&) { return false; }
std::wstring TargetDetector::foreground_window_title() { return {}; }
}

#endif
