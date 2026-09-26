#include <iostream>
#include "tpc/presence_data.hpp"

int main() {
    tpc::PresenceData data;
    data.application = "tpc";
    data.title = "Text Presence Core";
    std::cout << "{\n  \"application\": \"" << data.application << "\",\n  \"title\": \"" << data.title << "\"\n}\n";
    return 0;
}
