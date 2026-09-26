#pragma once

#include <memory>
#include <string>

#include "upc.hpp"

namespace tpc {

std::unique_ptr<AppRpcConnector> create_app_rpc_connector(
    const std::string& id
);

} // namespace tpc
