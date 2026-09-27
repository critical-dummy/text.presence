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
        message(FATAL_ERROR "TPC_DISCORD_SDK_LIBRARY must point to discord_partner_sdk.lib")
    endif()

    set(TPC_DISCORD_SDK_BIN "" CACHE FILEPATH
        "Discord Social SDK runtime DLL, usually discord_partner_sdk.dll")

    if (NOT EXISTS "${TPC_DISCORD_SDK_BIN}")
        message(FATAL_ERROR "TPC_DISCORD_SDK_BIN must point to discord_partner_sdk.dll")
    endif()

    add_library(tpc_discord_sdk OBJECT
        src/connectors/discord_sdk.cpp
        src/connectors/icon_server.cpp
    )

    target_compile_features(tpc_discord_sdk PRIVATE cxx_std_20)

    target_include_directories(tpc_discord_sdk PRIVATE
        src
        ${TPC_DISCORD_SDK_INCLUDE_DIR}
    )

    target_compile_definitions(tpc_discord_sdk PRIVATE
        TPC_ENABLE_DISCORD_SDK
    )

    target_sources(tpc PRIVATE
        $<TARGET_OBJECTS:tpc_discord_sdk>
    )

    target_link_libraries(tpc PRIVATE
        "${TPC_DISCORD_SDK_LIBRARY}"
        shell32
        shlwapi
        gdiplus
        winhttp
    )

    target_compile_definitions(tpc PRIVATE
        TPC_ENABLE_DISCORD_SDK
    )

    add_custom_command(TARGET tpc POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${TPC_DISCORD_SDK_BIN}"
        "$<TARGET_FILE_DIR:tpc>"
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
