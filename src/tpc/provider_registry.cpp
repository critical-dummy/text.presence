#include "provider_registry.hpp"

#include "targets/fl_studio.hpp"
#include "targets/generic_window.hpp"

namespace tpc {

ProviderRegistry::ProviderRegistry() {
    static const FlStudioProvider fl_studio;
    static const GenericWindowProvider generic_window;

    providers_.push_back(&fl_studio);
    providers_.push_back(&generic_window);
}

const TargetProvider& ProviderRegistry::detect() const {
    for (const TargetProvider* provider : providers_) {
        if (provider != nullptr && provider->matches()) {
            return *provider;
        }
    }

    return *providers_.back();
}

} // namespace tpc
