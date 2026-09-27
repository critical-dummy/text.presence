#include "icon_server.hpp"

#include "tpc/json.hpp"

#ifdef _WIN32
#include <windows.h>
#include <gdiplus.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <shobjidl.h>
#include <objbase.h>
#include <winhttp.h>
#endif

#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

#ifdef _WIN32

namespace {

constexpr wchar_t kIconServerHost[] =
    L"icon-server.presence-client.workers.dev";

bool find_png_encoder(CLSID& clsid) {
    using namespace Gdiplus;

    UINT encoder_count = 0;
    UINT encoder_size = 0;

    if (GetImageEncodersSize(&encoder_count, &encoder_size) != Ok ||
        encoder_size == 0) {
        return false;
    }

    std::vector<BYTE> buffer(encoder_size);
    auto* encoders =
        reinterpret_cast<ImageCodecInfo*>(buffer.data());

    if (GetImageEncoders(
            encoder_count,
            encoder_size,
            encoders
        ) != Ok) {
        return false;
    }

    for (UINT i = 0; i < encoder_count; ++i) {
        if (std::wstring(encoders[i].MimeType) == L"image/png") {
            clsid = encoders[i].Clsid;
            return true;
        }
    }

    return false;
}

struct IconResourceCandidate {
    WORD group_id = 0;
    WORD icon_id = 0;
    UINT width = 0;
    UINT height = 0;
    WORD bit_count = 0;
    DWORD image_bytes = 0;
};

struct EnumIconContext {
    HMODULE module = nullptr;
    IconResourceCandidate best;
    bool found = false;
};

#pragma pack(push, 1)
struct GroupIconDirectory {
    WORD reserved;
    WORD type;
    WORD count;
};

struct GroupIconEntry {
    BYTE width;
    BYTE height;
    BYTE color_count;
    BYTE reserved;
    WORD planes;
    WORD bit_count;
    DWORD bytes_in_res;
    WORD id;
};
#pragma pack(pop)

BOOL CALLBACK enum_icon_groups(
    HMODULE module,
    LPCWSTR,
    LPWSTR name,
    LONG_PTR parameter
) {
    auto* context =
        reinterpret_cast<EnumIconContext*>(parameter);

    const HRSRC group_resource =
        FindResourceW(
            module,
            name,
            RT_GROUP_ICON
        );

    if (group_resource == nullptr) {
        return TRUE;
    }

    const HGLOBAL loaded =
        LoadResource(module, group_resource);

    if (loaded == nullptr) {
        return TRUE;
    }

    const auto* directory =
        static_cast<const GroupIconDirectory*>(
            LockResource(loaded)
        );

    if (directory == nullptr ||
        directory->type != 1 ||
        directory->count == 0) {
        return TRUE;
    }

    const auto* entries =
        reinterpret_cast<const GroupIconEntry*>(
            reinterpret_cast<const BYTE*>(directory) +
            sizeof(GroupIconDirectory)
        );

    for (WORD i = 0; i < directory->count; ++i) {
        const auto& entry = entries[i];

        const UINT width =
            entry.width == 0 ? 256u : entry.width;

        const UINT height =
            entry.height == 0 ? 256u : entry.height;

        const std::uint64_t area =
            static_cast<std::uint64_t>(width) *
            static_cast<std::uint64_t>(height);

        const std::uint64_t best_area =
            static_cast<std::uint64_t>(context->best.width) *
            static_cast<std::uint64_t>(context->best.height);

        const bool better =
            !context->found ||
            area > best_area ||
            (area == best_area &&
             entry.bit_count > context->best.bit_count) ||
            (area == best_area &&
             entry.bit_count == context->best.bit_count &&
             entry.bytes_in_res > context->best.image_bytes);

        if (!better) {
            continue;
        }

        context->best.group_id =
            LOWORD(reinterpret_cast<ULONG_PTR>(name));
        context->best.icon_id = entry.id;
        context->best.width = width;
        context->best.height = height;
        context->best.bit_count = entry.bit_count;
        context->best.image_bytes = entry.bytes_in_res;
        context->found = true;
    }

    return TRUE;
}

bool extract_png(
    const std::wstring& executable_path,
    std::vector<std::uint8_t>& output
) {
    if (executable_path.empty()) {
        std::cerr << "Icon server: executable path is empty\n";
        return false;
    }

    HMODULE module = LoadLibraryExW(
        executable_path.c_str(),
        nullptr,
        LOAD_LIBRARY_AS_DATAFILE | LOAD_LIBRARY_AS_IMAGE_RESOURCE
    );

    if (module == nullptr) {
        return false;
    }

    EnumIconContext context;
    context.module = module;

    EnumResourceNamesW(
        module,
        RT_GROUP_ICON,
        enum_icon_groups,
        reinterpret_cast<LONG_PTR>(&context)
    );

    if (!context.found) {
        FreeLibrary(module);
        return false;
    }

    const HRSRC icon_resource =
        FindResourceW(
            module,
            MAKEINTRESOURCEW(context.best.icon_id),
            RT_ICON
        );

    if (icon_resource == nullptr) {
        FreeLibrary(module);
        return false;
    }

    const DWORD icon_size =
        SizeofResource(module, icon_resource);

    if (icon_size == 0) {
        FreeLibrary(module);
        return false;
    }

    const HGLOBAL icon_data =
        LoadResource(module, icon_resource);

    const void* icon_bytes =
        icon_data != nullptr
            ? LockResource(icon_data)
            : nullptr;

    if (icon_bytes == nullptr) {
        FreeLibrary(module);
        return false;
    }

    HICON icon = CreateIconFromResourceEx(
        static_cast<PBYTE>(
            const_cast<void*>(icon_bytes)
        ),
        icon_size,
        TRUE,
        0x00030000,
        static_cast<int>(context.best.width),
        static_cast<int>(context.best.height),
        LR_DEFAULTCOLOR
    );

    if (icon == nullptr) {
        FreeLibrary(module);
        return false;
    }

    Gdiplus::GdiplusStartupInput startup_input{};
    ULONG_PTR token = 0;

    if (Gdiplus::GdiplusStartup(
            &token,
            &startup_input,
            nullptr
        ) != Gdiplus::Ok) {
        DestroyIcon(icon);
        FreeLibrary(module);
        return false;
    }

    bool success = false;

    do {
        Gdiplus::Bitmap bitmap(icon);

        if (bitmap.GetLastStatus() != Gdiplus::Ok) {
            break;
        }

        CLSID png_clsid{};

        if (!find_png_encoder(png_clsid)) {
            break;
        }

        IStream* stream = SHCreateMemStream(nullptr, 0);

        if (stream == nullptr) {
            break;
        }

        const Gdiplus::Status save_status =
            bitmap.Save(stream, &png_clsid, nullptr);

        if (save_status == Gdiplus::Ok) {
            STATSTG stat{};

            if (SUCCEEDED(
                    stream->Stat(&stat, STATFLAG_NONAME)
                ) &&
                stat.cbSize.QuadPart > 0 &&
                stat.cbSize.QuadPart <=
                    static_cast<LONGLONG>(1024 * 1024) &&
                SUCCEEDED(
                    stream->Seek(
                        LARGE_INTEGER{},
                        STREAM_SEEK_SET,
                        nullptr
                    )
                )) {
                output.resize(
                    static_cast<std::size_t>(
                        stat.cbSize.QuadPart
                    )
                );

                ULONG read = 0;

                if (SUCCEEDED(
                        stream->Read(
                            output.data(),
                            static_cast<ULONG>(output.size()),
                            &read
                        )
                    ) &&
                    read == output.size()) {
                    success = true;
                }
            }
        }

        stream->Release();
    } while (false);

    Gdiplus::GdiplusShutdown(token);
    DestroyIcon(icon);
    FreeLibrary(module);

    return success && !output.empty();
}

bool http_request(
    const wchar_t* method,
    const std::wstring& path,
    const std::vector<std::uint8_t>* body,
    std::string& response,
    DWORD& status_code
) {
    response.clear();
    status_code = 0;

    HINTERNET session = WinHttpOpen(
        L"TextPresence/0.1",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0
    );

    if (!session) {
        return false;
    }

    HINTERNET connection = WinHttpConnect(
        session,
        kIconServerHost,
        INTERNET_DEFAULT_HTTPS_PORT,
        0
    );

    if (!connection) {
        WinHttpCloseHandle(session);
        return false;
    }

    HINTERNET request = WinHttpOpenRequest(
        connection,
        method,
        path.c_str(),
        nullptr,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        WINHTTP_FLAG_SECURE
    );

    if (!request) {
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        return false;
    }

    const bool is_post =
        std::wstring(method) == L"POST";

    const wchar_t* headers =
        is_post
            ? L"Content-Type: image/png\r\n"
            : nullptr;

    const DWORD header_length =
        headers != nullptr
            ? static_cast<DWORD>(-1L)
            : 0;

    const void* optional_data =
        body != nullptr && !body->empty()
            ? body->data()
            : nullptr;

    const DWORD optional_length =
        body != nullptr
            ? static_cast<DWORD>(body->size())
            : 0;

    const BOOL sent = WinHttpSendRequest(
        request,
        headers,
        header_length,
        const_cast<void*>(optional_data),
        optional_length,
        optional_length,
        0
    );

    if (!sent || !WinHttpReceiveResponse(request, nullptr)) {
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        return false;
    }

    DWORD status_size = sizeof(status_code);

    if (!WinHttpQueryHeaders(
            request,
            WINHTTP_QUERY_STATUS_CODE |
                WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX,
            &status_code,
            &status_size,
            WINHTTP_NO_HEADER_INDEX
        )) {
        status_code = 0;
    }

    for (;;) {
        DWORD available = 0;

        if (!WinHttpQueryDataAvailable(
                request,
                &available
            ) ||
            available == 0) {
            break;
        }

        std::vector<char> buffer(available);
        DWORD read = 0;

        if (!WinHttpReadData(
                request,
                buffer.data(),
                available,
                &read
            ) ||
            read == 0) {
            break;
        }

        response.append(buffer.data(), read);
    }

    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connection);
    WinHttpCloseHandle(session);

    return status_code >= 200 && status_code < 300;
}

bool read_string(
    const tpc::JsonValue& object,
    const char* key,
    std::string& output
) {
    const tpc::JsonValue* value = object.find(key);

    if (value == nullptr || !value->is_string()) {
        return false;
    }

    output = value->string_value();
    return true;
}

} // namespace

#endif

namespace tpc {

bool IconServerClient::sync(
    const std::wstring& executable_path,
    std::string& url
) {
#ifdef _WIN32
    url.clear();

    if (executable_path.empty()) {
        return false;
    }

    if (executable_path == current_executable_path_ &&
        !current_url_.empty()) {
        url = current_url_;
        return true;
    }

    std::vector<std::uint8_t> png;

    if (!extract_png(executable_path, png)) {
        std::cerr
            << "Icon server: icon extraction failed: "
            << std::string(executable_path.begin(), executable_path.end())
            << "\n";
        return false;
    }

    std::cerr
        << "Icon server: extracted PNG bytes="
        << png.size()
        << "\n";

    std::wstring path = L"/api/icons";

    if (!current_id_.empty()) {
        path += L"?previous=";
        path += std::wstring(
            current_id_.begin(),
            current_id_.end()
        );
    }

    std::string response;
    DWORD status_code = 0;

    if (!http_request(
            L"POST",
            path,
            &png,
            response,
            status_code
        )) {
        std::cerr
            << "Icon server: upload failed, HTTP status="
            << status_code
            << "\n";
        return false;
    }

    try {
        const JsonValue root = parse_json(response);

        if (!root.is_object()) {
            return false;
        }

        std::string id;
        std::string uploaded_url;

        if (!read_string(root, "id", id) ||
            !read_string(root, "url", uploaded_url) ||
            id.empty() ||
            uploaded_url.empty()) {
            return false;
        }

        current_executable_path_ = executable_path;
        current_id_ = id;
        current_url_ = uploaded_url;

        std::cerr
            << "Icon server: uploaded "
            << id
            << " -> "
            << uploaded_url
            << "\n";

        url = current_url_;
        return true;
    } catch (...) {
        return false;
    }
#else
    (void)executable_path;
    url.clear();
    return false;
#endif
}

void IconServerClient::clear() {
#ifdef _WIN32
    if (!current_id_.empty()) {
        const std::wstring path =
            L"/api/icons?id=" +
            std::wstring(
                current_id_.begin(),
                current_id_.end()
            );

        std::string response;
        DWORD status_code = 0;

        http_request(
            L"DELETE",
            path,
            nullptr,
            response,
            status_code
        );
    }

    current_executable_path_.clear();
    current_id_.clear();
    current_url_.clear();
#endif
}

} // namespace tpc
