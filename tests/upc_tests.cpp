#include <cstdlib>
#include <iostream>
#include <string>

#include "tpc/json.hpp"
#include "tpc/upc.hpp"

int main() {
    tpc::TargetConfig config;
    config.has_app_rpc = true;
    config.app_rpc = tpc::parse_json(R"json(
{
  "title": "You're using {application}",
  "details": {
    "window": "{window}",
    "process": "{process}"
  },
  "items": [
    "{provider}",
    "{process_id}"
  ]
}
)json");

    tpc::PresenceData data;
    data.application = "fl_studio";
    data.title = "FL Studio 2026";
    data.variables["provider"] = "fl_studio";
    data.variables["window"] = "FL Studio 2026";
    data.variables["process"] = "FL64.exe";
    data.variables["process_id"] = "20640";

    const tpc::UpcPayload payload =
        tpc::build_app_rpc_payload(config, data);

    const std::string expected =
        R"({"details":{"process":"FL64.exe","window":"FL Studio 2026"},"items":["fl_studio","20640"],"title":"You're using fl_studio"})";

    if (!payload.available) {
        std::cerr << "UPC payload was not available\n";
        return EXIT_FAILURE;
    }

    if (payload.json != expected) {
        std::cerr << "Unexpected UPC payload:\n"
                  << payload.json << "\n";
        return EXIT_FAILURE;
    }

    std::cout << "UPC payload test passed\n";
    return EXIT_SUCCESS;
}
