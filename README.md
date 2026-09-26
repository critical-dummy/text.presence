# TPC

**Text Presence Core** — runtime presence data acquisition for UPC.

```text
Target application → TPC → normalized presence data → UPC → rendered presence
```

TPC detects targets and collects runtime state. UPC formats that state into user-defined Presence text.

## v0.1

Native C++17 Windows executable. Target providers are independent from the core data model.

The current detector captures the foreground window process, PID, and title. Detection is routed through target providers; FL Studio is recognized as `fl_studio` before the generic-window fallback. The FL Studio provider currently uses Windows-native detection and may expose a conservative `project_hint` when the project name is present in the window title.

## Run

One snapshot:

```bat
build\\Release\\tpc.exe
```

Continuous snapshots every 500 ms:

```bat
build\\Release\\tpc.exe --watch
```

Custom watch interval in milliseconds:

```bat
build\\Release\\tpc.exe --watch 1000
```

Watch mode polls the detector, but re-renders the presence block only when the detected state changes. The previous block is cleared before the new report is generated.

## Build

```bat
cmake -S . -B build
cmake --build build --config Release
```
