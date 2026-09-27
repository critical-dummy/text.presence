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

bool extract_png(
    const std::wstring& executable_path,
    std::vector<std::uint8_t>& output
) {
    if (executable_path.empty()) {
        return false;
    }

    IShellItem* item = nullptr;

    const HRESULT item_result =
        SHCreateItemFromParsingName(
            executable_path.c_str(),
            nullptr,
            IID_PPV_ARGS(&item)
        );

    if (FAILED(item_result) || item == nullptr) {
        return false;
    }

    IShellItemImageFactory* image_factory = nullptr;

    const HRESULT factory_result =
        item->QueryInterface(
            IID_PPV_ARGS(&image_factory)
        );

    item->Release();

    if (FAILED(factory_result) || image_factory == nullptr) {
        return false;
    }

    HBITMAP bitmap_handle = nullptr;

    SIZE requested_size{};
    requested_size.cx = 256;
    requested_size.cy = 256;

    const HRESULT image_result =
        image_factory->GetImage(
            requested_size,
            SIIGBF_ICONONLY | SIIGBF_BIGGERSIZEOK,
            &bitmap_handle
        );

    image_factory->Release();

    if (FAILED(image_result) || bitmap_handle == nullptr) {
        return false;
    }

    Gdiplus::GdiplusStartupInput startup_input{};
    ULONG_PTR token = 0;

    if (Gdiplus::GdiplusStartup(
            &token,
            &startup_input,
            nullptr
        ) != Gdiplus::Ok) {
        DeleteObject(bitmap_handle);
        return false;
    }

    bool success = false;

    do {
        Gdiplus::Bitmap bitmap(bitmap_handle, nullptr);

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
    DeleteObject(bitmap_handle);

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
        return false;
    }

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
