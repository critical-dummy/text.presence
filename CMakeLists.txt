cmake_minimum_required(VERSION 3.20)
project(tpc VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(tpc
    src/main.cpp
    src/tpc/json.cpp
    src/tpc/presence_data.cpp
    src/tpc/target_config.cpp
    src/tpc/target_detector.cpp
    src/tpc/provider_registry.cpp
    src/tpc/tui.cpp
    src/targets/generic_window.cpp
    src/targets/fl_studio.cpp
)

target_include_directories(tpc PRIVATE src)

if (WIN32)
    target_compile_definitions(tpc PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
endif()
