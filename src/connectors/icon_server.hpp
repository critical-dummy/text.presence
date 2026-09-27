#pragma once

#include <string>

namespace tpc {

class IconServerClient {
public:
    bool sync(const std::wstring& executable_path, std::string& url);
    void clear();

private:
    std::wstring current_executable_path_;
    std::string current_id_;
    std::string current_url_;
};

} // namespace tpc
