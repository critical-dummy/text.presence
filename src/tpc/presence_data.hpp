#pragma once
#include <map>
#include <string>
namespace tpc {
struct PresenceData {
    std::string application;
    std::string title;
    std::map<std::string, std::string> variables;
};
}
