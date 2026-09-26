# TPC

**Text Presence Core** — a Presence runtime/framework for target detection, TUI rendering, and application RPC.

```text
Application
    ↓
TPC Target Detection
    ↓
Target Provider
    ↓
PresenceData
    ↓
target.json
    ├─ tpc_title
    ├─ TPC_RPC
    └─ APP_RPC
         ↓
    TPC TUI / App RPC connector
```

## v0.1

Native C++17 Windows executable.

Current runtime includes:

- foreground process, PID, and window-title detection
- provider registry with `generic_window` and `fl_studio`
- normalized `PresenceData`
- target JSON loading
- `{variable}` expansion
- dedicated TPC TUI with a customizable `tpc_title`
- change-driven live updates
- `APP_RPC` configuration loading for future connectors
- UPC APP_RPC payload resolution with recursive variable expansion
- application RPC connector interface
- optional Discord Social SDK connector

ASIO is not part of the TPC communication path. The current FL Studio provider uses stable Windows-native process/window detection only.

## Run

Start the TPC TUI:

```bat
build\Release\tpc.exe
```

Live mode with the default 500 ms detector interval:

```bat
build\Release\tpc.exe --watch
```

Custom detector interval:

```bat
build\Release\tpc.exe --watch 1000
```

Use a specific target configuration:

```bat
build\Release\tpc.exe --target targets\fl_studio.json
```

Print one raw `PresenceData` snapshot for debugging:

```bat
build\Release\tpc.exe --json
```

Show command-line help:

```bat
build\Release\tpc.exe --help
```

## Target configuration

Target files live under `targets/` and are selected automatically from the provider id.

The provider id is the canonical `{target}` key. For example, the focused `FL64.exe` target is detected by `fl_studio`, so TPC loads `targets/fl_studio.json` automatically. A separate target/preset identity layer is not required.

Example:

```json
{
  "tpc_title": "running Text Presence.",
  "TPC_RPC": {
    "title": "FL Studio",
    "lines": [
      "{application}",
      "Provider: {provider}",
      "Process: {process}",
      "PID: {process_id}",
      "Window: {window}"
    ]
  },
  "APP_RPC": {
    "title": "You're using FL Studio",
    "details": "{window}",
    "state": "{provider} • {process}"
  }
}
```

`TPC_RPC.lines` is limited to six lines. `APP_RPC` is loaded into the target model; UPC resolves it into a payload and the selected application connector can publish it.

UPC resolves every string value inside `APP_RPC` using the same `{variable}` namespace as `TPC_RPC`. The resolved payload remains a JSON document until an `AppRpcConnector` consumes it, so connector transport is kept separate from TPC detection and configuration.

### Discord connector

The Discord connector is built against Discord's current Social SDK rather than a custom bridge. Discord distributes the SDK through the Developer Portal, and its C++ wrapper is header-only over the shared C library. The desktop SDK can publish Rich Presence without connecting to the Discord gateway.

When using `--launch upc --app discord`, `APP_RPC` must contain an `application_id` string containing your Discord application ID:

```json
"APP_RPC": {
  "application_id": "YOUR_DISCORD_APPLICATION_ID",
  "details": "{window}",
  "state": "{provider}"
}
```

The Discord adapter maps `title` to the Rich Presence application name, `details` to the details line, and `state` to the state line. Current Discord Social SDK releases support customizing the displayed application name through `activity.name`.

Build the adapter explicitly by supplying the SDK header directory, import library, and runtime DLL:

```bat
cmake -S . -B build -DTPC_ENABLE_DISCORD_SDK=ON ^
  -DTPC_DISCORD_SDK_INCLUDE_DIR=C:\path\to\discord_social_sdk\include ^
  -DTPC_DISCORD_SDK_LIBRARY=C:\path\to\discord_social_sdk\lib\release\discord_partner_sdk.lib ^
  -DTPC_DISCORD_SDK_BIN=C:\path\to\discord_social_sdk\bin\release\discord_partner_sdk.dll

cmake --build build --config Release
```

The Discord adapter is compiled as a separate C++20 target so the rest of TPC can remain C++17. TPC also copies discord_partner_sdk.dll beside tpc.exe after an SDK-enabled build. Discord's standalone C++ SDK guide requires C++20 or greater and documents the Windows discord_partner_sdk.lib link library and discord_partner_sdk.dll runtime dependency.

`tpc_title` is user-customizable. It is rendered as the first line of the TPC TUI box and is also used as the Windows console title bar while that target is active.

Unknown `{variable}` placeholders are preserved instead of being silently removed.

## Build

```bat
cmake -S . -B build
cmake --build build --config Release
```

Run the UPC regression test:

```bat
ctest --test-dir build -C Release --output-on-failure
```
