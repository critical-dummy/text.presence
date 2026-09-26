#pragma once

#include <vector>

#include "target_provider.hpp"

namespace tpc {

class ProviderRegistry {
public:
    ProviderRegistry();

    const TargetProvider& detect() const;

private:
    std::vector<const TargetProvider*> providers_;
};

} // namespace tpc
