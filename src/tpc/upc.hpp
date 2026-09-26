#pragma once

#include <string>

#include "presence_data.hpp"
#include "target_config.hpp"

namespace tpc {

struct UpcPayload {
    bool available = false;
    std::string json;
};

UpcPayload build_app_rpc_payload(
    const TargetConfig& config,
    const PresenceData& data
);

class AppRpcConnector {
public:
    virtual ~AppRpcConnector() = default;

    virtual const char* id() const = 0;
    virtual bool publish(const std::string& payload) = 0;
    virtual void clear() = 0;
};

} // namespace tpc
