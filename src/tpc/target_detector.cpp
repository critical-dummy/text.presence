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

unsigned long TargetDetector::foreground_process_id() {
    const HWND hwnd = GetForegroundWindow();
    if (!hwnd) return 0;

    DWORD process_id = 0;
    GetWindowThreadProcessId(hwnd, &process_id);
    return static_cast<unsigned long>(process_id);
}

std::wstring TargetDetector::foreground_process_path() {
    const DWORD process_id =
        static_cast<DWORD>(foreground_process_id());

    if (process_id == 0) return {};

    HANDLE process =
        OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, process_id);

    if (!process) return {};

    std::wstring path(32768, L'\0');
    DWORD path_size = static_cast<DWORD>(path.size());

    const BOOL ok = QueryFullProcessImageNameW(
        process,
        0,
        path.data(),
        &path_size
    );

    CloseHandle(process);

    if (!ok || path_size == 0) return {};

    path.resize(path_size);
    return path;
}

std::wstring TargetDetector::foreground_process_name() {
    std::wstring path = foreground_process_path();

    if (path.empty()) return {};

    const std::wstring::size_type separator =
        path.find_last_of(L"\\/");

    if (separator != std::wstring::npos) {
        return path.substr(separator + 1);
    }

    return path;
}

std::wstring TargetDetector::foreground_window_title() {
    const HWND hwnd = GetForegroundWindow();
    if (!hwnd) return {};

    const int length = GetWindowTextLengthW(hwnd);
    if (length <= 0) return {};

    std::wstring title(static_cast<size_t>(length) + 1, L'\0');
    const int copied = GetWindowTextW(
        hwnd,
        title.data(),
        static_cast<int>(title.size())
    );

    if (copied <= 0) return {};

    title.resize(static_cast<size_t>(copied));
    return title;
}

} // namespace tpc
#else

namespace tpc {

bool TargetDetector::process_exists(const std::wstring&) {
    return false;
}

unsigned long TargetDetector::foreground_process_id() {
    return 0;
}

std::wstring TargetDetector::foreground_process_path() {
    return {};
}

std::wstring TargetDetector::foreground_process_name() {
    return {};
}

std::wstring TargetDetector::foreground_window_title() {
    return {};
}

} // namespace tpc
#endif
