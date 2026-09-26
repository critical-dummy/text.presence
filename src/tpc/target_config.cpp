#include "target_config.hpp"

#include <stdexcept>

namespace {

std::string string_or(
    const tpc::JsonValue* value,
    const std::string& fallback
) {
    if (value == nullptr || !value->is_string()) {
        return fallback;
    }

    return value->string_value();
}

}

namespace tpc {

bool load_target_config(
    const std::string& path,
    TargetConfig& config,
    std::string& error
) {
    std::string source;

    if (!read_text_file(path, source)) {
        error = "cannot read target config: " + path;
        return false;
    }

    try {
        const JsonValue root = parse_json(source);

        if (!root.is_object()) {
            error = "target config root must be an object";
            return false;
        }

        config = TargetConfig{};

        config.tpc_title = string_or(
            root.find("tpc_title"),
            "Text Presence"
        );

        const JsonValue* tpc_rpc = root.find("TPC_RPC");

        if (tpc_rpc != nullptr && tpc_rpc->is_object()) {
            config.tpc_rpc.title = string_or(
                tpc_rpc->find("title"),
                ""
            );

            const JsonValue* lines = tpc_rpc->find("lines");

            if (lines != nullptr && lines->is_array()) {
                for (const JsonValue& line : lines->array_value()) {
                    if (line.is_string()) {
                        config.tpc_rpc.lines.push_back(
                            line.string_value()
                        );
                    }
                }
            }

            // Keep the runtime TUI limited to six configured Presence lines.
            if (config.tpc_rpc.lines.size() > 6) {
                config.tpc_rpc.lines.resize(6);
            }
        }

        const JsonValue* app_rpc = root.find("APP_RPC");

        if (app_rpc != nullptr) {
            config.app_rpc = *app_rpc;
            config.has_app_rpc = true;
        }

        return true;
    } catch (const std::exception& exception) {
        error = "invalid target config: ";
        error += exception.what();
        return false;
    }
}

std::string expand_variables(
    const std::string& text,
    const PresenceData& data
) {
    std::string result;
    result.reserve(text.size());

    std::size_t position = 0;

    while (position < text.size()) {
        const std::size_t open = text.find('{', position);

        if (open == std::string::npos) {
            result.append(text, position, std::string::npos);
            break;
        }

        result.append(text, position, open - position);

        const std::size_t close = text.find('}', open + 1);

        if (close == std::string::npos) {
            result.append(text, open, std::string::npos);
            break;
        }

        const std::string key =
            text.substr(open + 1, close - open - 1);

        std::string value;

        if (key == "application") {
            value = data.application;
        } else if (key == "title") {
            value = data.title;
        } else {
            const auto iterator = data.variables.find(key);

            if (iterator != data.variables.end()) {
                value = iterator->second;
            } else {
                // Preserve an unknown placeholder rather than silently
                // destroying user-authored target.json text.
                value = text.substr(open, close - open + 1);
            }
        }

        result += value;
        position = close + 1;
    }

    return result;
}

} // namespace tpc
