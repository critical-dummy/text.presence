# TPC

**Text Presence Core** — runtime presence data acquisition for UPC.

```text
Target application → TPC → normalized presence data → UPC → rendered presence
```

TPC detects targets and collects runtime state. UPC formats that state into user-defined Presence text.

## v0.1

Native C++17 Windows executable. Target providers will be added independently from the core data model.

## Build

```powershell
cmake -S . -B build
cmake --build build --config Release
```
