#include "upc.hpp"

#include <iomanip>
#include <sstream>

namespace {

std::string json_escape(const std::string& value) {
    std::string result;
    result.reserve(value.size() + 8);

    static constexpr char hex[] = "0123456789abcdef";

    for (const unsigned char ch : value) {
        switch (ch) {
        case '\\':
            result += "\\\\";
            break;
        case '"':
            result += "\\"";
            break;
        case '\b':
            result += "\\b";
            break;
        case '\f':
            result += "\\f";
            break;
        case '\n':
            result += "\\n";
            break;
        case '\r':
            result += "\\r";
            break;
        case '\t':
            result += "\\t";
            break;
        default:
            if (ch < 0x20) {
                result += "\\u00";
                result += hex[(ch >> 4) & 0x0f];
                result += hex[ch & 0x0f];
            } else {
                result += static_cast<char>(ch);
            }
            break;
        }
    }

    return result;
}

std::string serialize_resolved(
    const tpc::JsonValue& value,
    const tpc::PresenceData& data
) {
    switch (value.type()) {
    case tpc::JsonValue::Type::Null:
        return "null";

    case tpc::JsonValue::Type::Boolean:
        return value.boolean_value() ? "true" : "false";

    case tpc::JsonValue::Type::Number:
        return value.string_value();

    case tpc::JsonValue::Type::String:
        return """ +
            json_escape(
                tpc::expand_variables(value.string_value(), data)
            ) +
            """;

    case tpc::JsonValue::Type::Array: {
        std::string result = "[";

        bool first = true;

        for (const auto& item : value.array_value()) {
            if (!first) {
                result += ",";
            }

            first = false;
            result += serialize_resolved(item, data);
        }

        result += "]";
        return result;
    }

    case tpc::JsonValue::Type::Object: {
        std::string result = "{";

        bool first = true;

        for (const auto& [key, item] : value.object_value()) {
            if (!first) {
                result += ",";
            }

            first = false;

            result += """;
            result += json_escape(key);
            result += "":";
            result += serialize_resolved(item, data);
        }

        result += "}";
        return result;
    }
    }

    return "null";
}

} // namespace

namespace tpc {

UpcPayload build_app_rpc_payload(
    const TargetConfig& config,
    const PresenceData& data
) {
    if (!config.has_app_rpc) {
        return {};
    }

    UpcPayload payload;
    payload.available = true;
    payload.json = serialize_resolved(config.app_rpc, data);
    return payload;
}

} // namespace tpc
