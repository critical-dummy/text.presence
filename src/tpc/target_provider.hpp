#pragma once

#include <string>

#include "presence_data.hpp"

namespace tpc {

class TargetProvider {
public:
    virtual ~TargetProvider() = default;

    virtual const char* id() const = 0;
    virtual bool matches() const = 0;
    virtual PresenceData capture() const = 0;
};

} // namespace tpc
