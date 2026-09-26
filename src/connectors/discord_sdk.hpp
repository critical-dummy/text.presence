#pragma once

#include "tpc/upc.hpp"

namespace tpc {

class DiscordSdkConnector final : public AppRpcConnector {
public:
    DiscordSdkConnector();
    ~DiscordSdkConnector() override;

    const char* id() const override;

    bool publish(const std::string& payload) override;
    void clear() override;
    void tick() override;

private:
    struct Impl;
    Impl* impl_ = nullptr;
};

} // namespace tpc
