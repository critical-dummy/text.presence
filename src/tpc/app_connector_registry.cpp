#include "app_connector_registry.hpp"

#include <string>

#ifdef TPC_ENABLE_DISCORD_SDK
#include "connectors/discord_sdk.hpp"
#endif

namespace tpc {

std::unique_ptr<AppRpcConnector> create_app_rpc_connector(
    const std::string& id
) {
#ifdef TPC_ENABLE_DISCORD_SDK
    if (id == "discord") {
        return std::make_unique<DiscordSdkConnector>();
    }
#else
    (void)id;
#endif

    return nullptr;
}

} // namespace tpc
