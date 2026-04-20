# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

ECLYPSE is a Windows system cleaner and disk management utility. It's a single-file Win32 GUI application with a custom-drawn dark theme (ECLYPSE theme: dark blue-black background with purple `#8C66E6` accent), matching the style from the MinecraftCheatDLL project.

## Build

```bash
# Build with MSBuild (x64 Debug)
"/c/Program Files/Microsoft Visual Studio/18/Community/MSBuild/Current/Bin/MSBuild.exe" FNCLEANER_.vcxproj -p:Configuration=Debug -p:Platform=x64

# Output executable
x64/Debug/FNCLEANER_.exe
```

- MSVC v145 toolset, C++20, Windows subsystem (not Console)
- No external dependencies — pure Win32 API + GDI + shell APIs
- Linked libs: `dwmapi.lib`, `shlwapi.lib`, `shell32.lib` (via `#pragma comment`)

## Architecture

Everything lives in `main.cpp` (~1900 lines). The structure follows this pattern:

1. **Colors namespace** — ECLYPSE theme constants (all `COLORREF`)
2. **Data types** — `ButtonState`, `DriveInfo`, tab/option enums
3. **Global state** — fonts, active tab, button states, drive selection state
4. **Drawing functions** — `DrawSidebar()`, `DrawDriveSelectPanel()`, `DrawOthersPanel()`, `DrawPartitionsPanel()`, `DrawContentPanel()` (router)
5. **Cleaning functions** — `CleanTempFiles()`, `CleanLogs()`, `CleanCache()`, `CleanRegistry()`, `CleanPrefetch()`, `CleanDNS()`, `CleanGameFolders()`
6. **Disk functions** — `GetPhysicalDiskNumber()`, `GetPhysicalDiskSize()`, `CreatePartitions()`, `MergeAllPartitions()`, `WipeDrive()`
7. **WindowProc** — all input handling (hover, click, tab switching) in `WM_MOUSEMOVE`, `WM_LBUTTONDOWN`, `WM_LBUTTONUP`
8. **wWinMain** — UAC elevation check, window creation, message loop

### UI Pattern

All UI is owner-drawn via GDI into a double-buffered DC. There are no Win32 controls — buttons, tabs, and drive list items are all custom-painted rectangles with hover/press states tracked via `ButtonState` structs and `RECT` hit-testing.

### Tab Structure

- **Cleaner** — drive selection list + Clean button (wipes drive, then optionally creates 100GB partitions via `diskpart`)
- **Partitions** — Windows 10 install guide + merge all partitions into single volume
- **Others** — 7 cleaning options in a 2-column button grid (temp files, logs, cache, registry, prefetch, DNS, game folders)

### Adding a New Cleaning Option

1. Add entry to `OthersOption` enum and increment `OPT_COUNT`
2. Add name to `g_optionNames[]` and description to `g_optionDescs[]`
3. Write a `CleanXxx()` function returning `std::wstring` with result message
4. Add `case OPT_XXX:` to the switch in `WM_LBUTTONUP`

### Adding a New Tab

1. Add entry to `Tab` enum (before `TAB_COUNT`)
2. Add name/description to `g_tabNames[]`/`g_tabDescriptions[]`
3. Create `DrawXxxPanel(HDC, RECT)` function
4. Add `else if` in `DrawContentPanel()`
5. Add hover/click handling in `WM_MOUSEMOVE`, `WM_LBUTTONDOWN`, `WM_LBUTTONUP`

## Key Details

- App auto-elevates to admin on startup via `ShellExecuteEx` with `runas`
- Disk operations use `diskpart` scripts written to `%TEMP%` and executed via `CreateProcess`
- Drive wipe has double confirmation dialogs before proceeding
- All cleaning functions that touch `C:\Windows\*` paths need admin privileges
