#pragma once

#include "tpc/target_provider.hpp"

namespace tpc {

class GenericWindowProvider final : public TargetProvider {
public:
    const char* id() const override;
    bool matches() const override;
    PresenceData capture() const override;
};

} // namespace tpc
