#pragma once

#include <string>
#include <vector>

#include "json.hpp"
#include "presence_data.hpp"

namespace tpc {

struct TpcRpcConfig {
    std::string title;
    std::vector<std::string> lines;
};

struct TargetConfig {
    std::string tpc_title = "Text Presence";
    TpcRpcConfig tpc_rpc;
    JsonValue app_rpc;
    bool has_app_rpc = false;
};

bool load_target_config(
    const std::string& path,
    TargetConfig& config,
    std::string& error
);

std::string expand_variables(
    const std::string& text,
    const PresenceData& data
);

} // namespace tpc
