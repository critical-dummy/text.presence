#include "target_detector.hpp"

#ifdef _WIN32
#include <windows.h>
#include <tlhelp32.h>

#include <vector>

namespace {

using NtStatus = LONG;
using NtQueryInformationProcessFn = NtStatus (NTAPI*)(
    HANDLE,
    ULONG,
    PVOID,
    ULONG,
    PULONG
);

struct UnicodeString {
    USHORT Length;
    USHORT MaximumLength;
    PWSTR Buffer;
};

constexpr ULONG kProcessCommandLineInformation = 60;
constexpr NtStatus kStatusInfoLengthMismatch = static_cast<NtStatus>(0xC0000004L);

NtQueryInformationProcessFn load_nt_query_information_process() {
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    if (!ntdll) {
        ntdll = LoadLibraryW(L"ntdll.dll");
    }

    if (!ntdll) {
        return nullptr;
    }

    return reinterpret_cast<NtQueryInformationProcessFn>(
        GetProcAddress(ntdll, "NtQueryInformationProcess")
    );
}

std::wstring query_process_command_line(HANDLE process) {
    const auto nt_query = load_nt_query_information_process();
    if (!nt_query) {
        return {};
    }

    ULONG required_size = 0;
    NtStatus status = nt_query(
        process,
        kProcessCommandLineInformation,
        nullptr,
        0,
        &required_size
    );

    if (status != kStatusInfoLengthMismatch || required_size == 0) {
        return {};
    }

    std::vector<unsigned char> buffer(required_size);

    status = nt_query(
        process,
        kProcessCommandLineInformation,
        buffer.data(),
        static_cast<ULONG>(buffer.size()),
        &required_size
    );

    if (status < 0 || buffer.size() < sizeof(UnicodeString)) {
        return {};
    }

    const auto* command_line =
        reinterpret_cast<const UnicodeString*>(buffer.data());

    if (command_line->Length == 0 ||
        command_line->Buffer == nullptr ||
        command_line->Length > command_line->MaximumLength) {
        return {};
    }

    const auto* buffer_begin = buffer.data();
    const auto* buffer_end = buffer_begin + buffer.size();
    const auto* string_begin =
        reinterpret_cast<const unsigned char*>(command_line->Buffer);

    if (string_begin < buffer_begin ||
        string_begin + command_line->Length > buffer_end) {
        return {};
    }

    return std::wstring(
        command_line->Buffer,
        command_line->Length / sizeof(wchar_t)
    );
}

} // namespace

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

std::wstring TargetDetector::foreground_process_name() {
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

    const std::wstring::size_type separator =
        path.find_last_of(L"\/");

    if (separator != std::wstring::npos) {
        return path.substr(separator + 1);
    }

    return path;
}

std::wstring TargetDetector::foreground_process_command_line() {
    const DWORD process_id =
        static_cast<DWORD>(foreground_process_id());

    if (process_id == 0) return {};

    HANDLE process = OpenProcess(
        PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ,
        FALSE,
        process_id
    );

    if (!process) return {};

    const std::wstring result = query_process_command_line(process);

    CloseHandle(process);
    return result;
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

std::wstring TargetDetector::foreground_process_name() {
    return {};
}

std::wstring TargetDetector::foreground_process_command_line() {
    return {};
}

std::wstring TargetDetector::foreground_window_title() {
    return {};
}

} // namespace tpc
#endif
