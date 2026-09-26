#include <windows.h>

#include <atomic>
#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

#include "tpc/app_connector_registry.hpp"
#include "tpc/presence_data.hpp"
#include "tpc/provider_registry.hpp"
#include "tpc/target_config.hpp"
#include "tpc/tui.hpp"
#include "tpc/upc.hpp"

namespace {

constexpr unsigned int kDefaultWatchIntervalMs = 500;

std::atomic_bool g_stop_requested = false;

BOOL WINAPI console_handler(DWORD event) {
    switch (event) {
    case CTRL_C_EVENT:
    case CTRL_CLOSE_EVENT:
    case CTRL_BREAK_EVENT:
        g_stop_requested.store(true);
        return TRUE;
    default:
        return FALSE;
    }
}

std::string json_escape(const std::string& value) {
    std::string result;
    result.reserve(value.size() + 8);

    for (const unsigned char ch : value) {
        switch (ch) {
        case '\\':
            result += "\\\\";
            break;
        case '"':
            result += "\\\"";
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
                static constexpr char hex[] = "0123456789abcdef";
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

void print_json(const tpc::PresenceData& data) {
    std::cout << "{\n";
    std::cout << "  \"application\": \""
              << json_escape(data.application) << "\",\n";
    std::cout << "  \"title\": \""
              << json_escape(data.title) << "\",\n";
    std::cout << "  \"variables\": {\n";

    bool first = true;

    for (const auto& [key, value] : data.variables) {
        if (!first) {
            std::cout << ",\n";
        }

        first = false;

        std::cout << "    \"" << json_escape(key) << "\": \""
                  << json_escape(value) << "\"";
    }

    std::cout << "\n  }\n";
    std::cout << "}\n";
}

tpc::TargetConfig fallback_config(const tpc::PresenceData& data) {
    tpc::TargetConfig config;

    config.tpc_title = "Text Presence";
    config.tpc_rpc.title = data.application;
    config.tpc_rpc.lines = {
        "{application}",
        "Provider: {provider}",
        "Process: {process}",
        "PID: {process_id}",
        "Window: {window}"
    };

    return config;
}

tpc::TargetConfig load_for_provider(
    const tpc::TargetProvider& provider,
    const std::string& override_path,
    const tpc::PresenceData& data
) {
    const std::string path = override_path.empty()
        ? ("targets/" + std::string(provider.id()) + ".json")
        : override_path;

    tpc::TargetConfig config;
    std::string error;

    if (!tpc::load_target_config(path, config, error)) {
        return fallback_config(data);
    }

    return config;
}

void print_help() {
    std::cout
        << "Text Presence Core\n\n"
        << "Usage:\n"
        << "  tpc.exe                    Start the TPC TUI\n"
        << "  tpc.exe --watch [ms]       Start the live TPC TUI\n"
        << "  tpc.exe --json                   Print one raw PresenceData snapshot\n"
        << "  tpc.exe --target <path>          Use a specific target preset\n"
        << "  text.presence --launch upc --app discord\n"
        << "                                  Launch UPC with the Discord connector\n"
        << "  tpc.exe --help                   Show this help\n";
}

} // namespace

int main(int argc, char* argv[]) {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    bool raw_json = false;
    unsigned int interval_ms = kDefaultWatchIntervalMs;
    std::string target_override;
    std::string launch_mode;
    std::string app_connector;

    for (int i = 1; i < argc; ++i) {
        const std::string argument = argv[i];

        if (argument == "--help" || argument == "-h") {
            print_help();
            return 0;
        }

        if (argument == "--json") {
            raw_json = true;
            continue;
        }

        if (argument == "--launch") {
            if (i + 1 >= argc) {
                std::cerr << "--launch requires a mode\n";
                return 2;
            }

            launch_mode = argv[++i];

            if (launch_mode != "upc") {
                std::cerr << "Unknown launch mode: " << launch_mode << "\n";
                return 2;
            }

            continue;
        }

        if (argument == "--app") {
            if (i + 1 >= argc) {
                std::cerr << "--app requires a connector name\n";
                return 2;
            }

            app_connector = argv[++i];
            continue;
        }

        if (argument == "--watch") {
            continue;
        }

        if (argument == "--target") {
            if (i + 1 >= argc) {
                std::cerr << "--target requires a path\n";
                return 2;
            }

            target_override = argv[++i];
            continue;
        }

        if (i == argc - 1 && argument.find_first_not_of("0123456789") == std::string::npos) {
            try {
                interval_ms = static_cast<unsigned int>(
                    std::stoul(argument)
                );
            } catch (...) {
                std::cerr << "Invalid interval: " << argument << "\n";
                return 2;
            }

            if (interval_ms == 0) {
                interval_ms = kDefaultWatchIntervalMs;
            }

            continue;
        }

        std::cerr << "Unknown argument: " << argument << "\n";
        return 2;
    }

    if (!app_connector.empty() && launch_mode != "upc") {
        std::cerr << "--app requires --launch upc\n";
        return 2;
    }

    if (launch_mode == "upc" && app_connector.empty()) {
        std::cerr << "--launch upc requires --app <connector>\n";
        return 2;
    }

    tpc::ProviderRegistry registry;
    const tpc::TargetProvider& initial_provider = registry.detect();
    const tpc::PresenceData initial_data = initial_provider.capture();

    if (raw_json) {
        print_json(initial_data);
        return 0;
    }

    if (!SetConsoleCtrlHandler(console_handler, TRUE)) {
        std::cerr << "Warning: could not install console handler\n";
    }

    tpc::Tui tui;

    if (!tui.start()) {
        std::cerr << "Could not start TPC TUI. Use --json for raw output.\n";
        return 1;
    }

    std::unique_ptr<tpc::AppRpcConnector> app_rpc_connector;

    if (launch_mode == "upc") {
        app_rpc_connector =
            tpc::create_app_rpc_connector(app_connector);

        if (!app_rpc_connector) {
            tui.stop();

            std::cerr
                << "App RPC connector unavailable: "
                << app_connector << "\n";

#ifdef TPC_ENABLE_DISCORD_SDK
            std::cerr
                << "For Discord, verify the Social SDK connector configuration.\n";
#else
            if (app_connector == "discord") {
                std::cerr
                    << "Reconfigure with TPC_ENABLE_DISCORD_SDK=ON and the Discord Social SDK paths.\n";
            }
#endif

            SetConsoleCtrlHandler(console_handler, FALSE);
            return 1;
        }
    }

    tpc::PresenceData previous;
    bool has_previous = false;
    std::string previous_provider;

    while (!g_stop_requested.load()) {
        const tpc::TargetProvider& provider = registry.detect();
        const tpc::PresenceData current = provider.capture();

        const bool changed =
            !has_previous ||
            provider.id() != previous_provider ||
            current.application != previous.application ||
            current.title != previous.title ||
            current.variables != previous.variables;

        if (changed) {
            const tpc::TargetConfig config =
                load_for_provider(provider, target_override, current);

            tui.render(config, current);

            if (app_rpc_connector) {
                const tpc::UpcPayload payload =
                    tpc::build_app_rpc_payload(config, current);

                if (payload.available) {
                    if (!app_rpc_connector->publish(payload.json)) {
                        std::cerr
                            << "APP_RPC publish failed for connector: "
                            << app_rpc_connector->id() << "\n";
                    }
                } else {
                    app_rpc_connector->clear();
                }
            }

            previous = current;
            previous_provider = provider.id();
            has_previous = true;
        }

        if (app_rpc_connector) {
            app_rpc_connector->tick();
        }

        std::this_thread::sleep_for(
            std::chrono::milliseconds(interval_ms)
        );
    }

    if (app_rpc_connector) {
        app_rpc_connector->clear();
    }

    tui.stop();
    SetConsoleCtrlHandler(console_handler, FALSE);

    return 0;
}
