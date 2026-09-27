#define DISCORDPP_IMPLEMENTATION
#include <discordpp.h>

#include "discord_sdk.hpp"

#include "tpc/json.hpp"

#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <string>
#include <utility>

namespace {

bool read_string(
    const tpc::JsonValue& object,
    const char* key,
    std::string& output
) {
    const tpc::JsonValue* value = object.find(key);

    if (value == nullptr || !value->is_string()) {
        return false;
    }

    output = value->string_value();
    return true;
}

bool read_uint64_string(
    const tpc::JsonValue& object,
    const char* key,
    std::uint64_t& output
) {
    std::string value;

    if (!read_string(object, key, value) || value.empty()) {
        return false;
    }

    char* end = nullptr;

    errno = 0;
    const unsigned long long parsed =
        std::strtoull(value.c_str(), &end, 10);

    if (errno != 0 ||
        end == value.c_str() ||
        *end != '\0' ||
        parsed > std::numeric_limits<std::uint64_t>::max()) {
        return false;
    }

    output = static_cast<std::uint64_t>(parsed);
    return true;
}

} // namespace

namespace tpc {

struct DiscordSdkConnector::Impl {
    discordpp::Client client;
    std::uint64_t application_id = 0;
    bool configured = false;
};

DiscordSdkConnector::DiscordSdkConnector()
    : impl_(new Impl()) {}

DiscordSdkConnector::~DiscordSdkConnector() {
    if (impl_ != nullptr) {
        impl_->client.ClearRichPresence();
        delete impl_;
        impl_ = nullptr;
    }
}

const char* DiscordSdkConnector::id() const {
    return "discord";
}

bool DiscordSdkConnector::publish(const std::string& payload) {
    if (impl_ == nullptr) {
        return false;
    }

    try {
        const JsonValue root = parse_json(payload);

        if (!root.is_object()) {
            return false;
        }

        std::uint64_t application_id = 0;

        if (!read_uint64_string(root, "application_id", application_id)) {
            return false;
        }

        if (!impl_->configured ||
            impl_->application_id != application_id) {
            impl_->client.SetApplicationId(application_id);
            impl_->application_id = application_id;
            impl_->configured = true;
        }

        discordpp::Activity activity;
        activity.SetType(discordpp::ActivityTypes::Playing);

        std::string value;

        if (read_string(root, "title", value) && !value.empty()) {
            activity.SetName(value);
        }

        if (read_string(root, "details", value) && !value.empty()) {
            activity.SetDetails(value);
        }

        if (read_string(root, "state", value) && !value.empty()) {
            activity.SetState(value);
        }

        impl_->client.UpdateRichPresence(
            std::move(activity),
            [](discordpp::ClientResult) {}
        );

        return true;
    } catch (...) {
        return false;
    }
}

void DiscordSdkConnector::clear() {
    if (impl_ == nullptr) {
        return;
    }

    impl_->client.ClearRichPresence();
}

void DiscordSdkConnector::tick() {
    discordpp::RunCallbacks();
}

} // namespace tpc
