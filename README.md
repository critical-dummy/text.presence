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
- dedicated TPC TUI
- change-driven live updates
- `APP_RPC` configuration loading for future connectors

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

`TPC_RPC.lines` is limited to six lines. `APP_RPC` is loaded into the target model but is not sent anywhere yet; connectors will consume it later.

`tpc_title` is used for both the TPC TUI heading and the Windows console title bar while that target is active.

Unknown `{variable}` placeholders are preserved instead of being silently removed.

## Build

```bat
cmake -S . -B build
cmake --build build --config Release
```
