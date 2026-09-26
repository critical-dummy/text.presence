#pragma once
#include "presence_data.hpp"

namespace tpc {

class TargetProvider {
public:
    virtual ~TargetProvider() = default;
    virtual bool matches() const = 0;
    virtual PresenceData capture() const = 0;
};

} // namespace tpc
