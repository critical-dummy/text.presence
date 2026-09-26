#pragma once

#include "presence_data.hpp"

namespace tpc {

class TargetProvider {
public:
    virtual ~TargetProvider() = default;

    // Canonical target key. It is also used to resolve targets/{id}.json.
    virtual const char* id() const = 0;
    virtual bool matches() const = 0;
    virtual PresenceData capture() const = 0;
};

} // namespace tpc
