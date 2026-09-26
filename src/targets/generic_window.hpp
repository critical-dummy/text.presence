#pragma once

#include "tpc/target_provider.hpp"

namespace tpc {

class GenericWindowProvider final : public TargetProvider {
public:
    bool matches() const override;
    PresenceData capture() const override;
};

} // namespace tpc
