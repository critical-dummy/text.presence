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
    src/tpc/upc.cpp
    src/tpc/app_connector_registry.cpp
    src/targets/generic_window.cpp
    src/targets/fl_studio.cpp
)

target_include_directories(tpc PRIVATE src)

if (WIN32)
    target_compile_definitions(tpc PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
endif()

option(TPC_ENABLE_DISCORD_SDK "Build the Discord Social SDK connector" OFF)
set(TPC_DISCORD_SDK_INCLUDE_DIR "" CACHE PATH "Directory containing discordpp.h")
set(TPC_DISCORD_SDK_LIBRARY "" CACHE FILEPATH "Discord Social SDK C library/import library")

if (TPC_ENABLE_DISCORD_SDK)
    if (NOT WIN32)
        message(FATAL_ERROR "TPC_ENABLE_DISCORD_SDK currently supports Windows only")
    endif()

    if (NOT EXISTS "${TPC_DISCORD_SDK_INCLUDE_DIR}/discordpp.h")
        message(FATAL_ERROR "TPC_DISCORD_SDK_INCLUDE_DIR must contain discordpp.h")
    endif()

    if (NOT EXISTS "${TPC_DISCORD_SDK_LIBRARY}")
        message(FATAL_ERROR "TPC_DISCORD_SDK_LIBRARY must point to the Discord Social SDK library")
    endif()

    target_sources(tpc PRIVATE
        src/connectors/discord_sdk.cpp
    )

    target_include_directories(tpc PRIVATE
        ${TPC_DISCORD_SDK_INCLUDE_DIR}
    )

    target_link_libraries(tpc PRIVATE
        "${TPC_DISCORD_SDK_LIBRARY}"
    )

    target_compile_definitions(tpc PRIVATE
        TPC_ENABLE_DISCORD_SDK
    )
endif()

include(CTest)

if (BUILD_TESTING)
    add_executable(tpc_upc_tests
        tests/upc_tests.cpp
        src/tpc/json.cpp
        src/tpc/presence_data.cpp
        src/tpc/target_config.cpp
        src/tpc/upc.cpp
    )

    target_include_directories(tpc_upc_tests PRIVATE src)

    if (WIN32)
        target_compile_definitions(tpc_upc_tests PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
    endif()

    add_test(NAME tpc_upc_tests COMMAND tpc_upc_tests)
endif()
