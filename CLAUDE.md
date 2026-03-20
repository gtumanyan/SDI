# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Snappy Driver Installer (SDI) is a Windows driver installation and management tool written in C++. The application scans hardware using Windows Setup APIs, indexes driver packages from 7z archives, matches hardware IDs to drivers using a hash-table lookup system, and installs drivers via Windows Driver Installation APIs. It includes a BitTorrent-based update system for peer-to-peer distribution of driver packs. Licensed under GPL 3-Clause.

## Build Commands

```powershell
# MSBuild directly (used by CI)
msbuild SDI.slnx /m /p:Configuration=Release /p:Platform=x64
```

Default configuration is Release. The solution (`SDI.slnx`) contains 5 projects: SDI, install64, torrent-rasterbar, SevenZip, and libwebp.

### Prerequisites

- Visual Studio 2022 or later (VS 2026 recommended)
- Git submodules initialized (`git submodule update --init --recursive`)
- Boost headers generated (`cd ext/boost && bootstrap && b2 headers`)

### Versioning

Run `Version.cmd` (wraps `Version.ps1`) before building to generate `src\VersionEx.h` from `Versions\VersionEx.h.tpl`. Format: `YY.MM.dd.Build` (build number persisted in `Versions\build.txt`, days since epoch in `Versions\day.txt`, commit hash in `Versions\commit_id.txt`).

### Tests

```cmd
cd tests
bench_index.bat       # Counts index build time
test_indexer.bat      # Creates indices in current and previous build
test_matcher.bat      # Compares drivers matching in current and previous build
```

### CI

GitHub Actions (`.github/workflows/msbuild.yml`) builds x64 in Release on `windows-latest` runners, triggered on push/PR to `dev`. The workflow selectively initializes only required Boost submodules (~47 out of 200+) to reduce checkout time.

## Architecture

### Core Directories

| Directory           | Purpose                              | Key Contents                                                              |
|---------------------|--------------------------------------|---------------------------------------------------------------------------|
| **src/**            | Main application source code         | C++ implementation files (.cpp), headers (.h), resource script (SDI.rc)   |
| **src/utils/**      | Utility library                      | FileUtil, StrUtil, ThreadUtil, Vec, Scoped handles, HTTP utilities        |
| **.vs/**            | Visual Studio project files          | SDI.vcxproj, torrent-rasterbar.vcxproj, SevenZip.vcxproj, libwebp.vcxproj, install64.vcxproj |
| **ext/**            | External dependencies (submodules)   | Boost, libtorrent, libwebp, 7-Zip SDK                                    |
| **res/**            | Embedded resources                   | Images (*.webp, *.bmp, *.ico), manifest (SDI.manifest), CLI help (cli.txt), install64.exe |
| **Tools/Langs/**    | Localization files                   | 42 language definition files (english.txt, russian.txt, etc.)             |
| **Tools/themes/**   | Theme files                          | 11 theme definitions (win11dark.txt, etc.) with associated image dirs     |
| **Versions/**       | Version management                   | Build number tracking (build.txt), version template (VersionEx.h.tpl)     |
| **scripts/**        | Deployment/automation scripts        | setup.ps1, autoupdate.bat, example scripts                                |
| **tests/**          | Test batch files                     | bench_index.bat, test_indexer.bat, test_matcher.bat                       |
| **Docs/**           | Documentation                        | Building guide, changelog, theming guide, manual PDF                      |

### Source File Categories

#### Driver Management Core
- `indexing.cpp/h` — Driver pack scanning, 7z extraction, INF parsing, hash table indexing
- `matcher.cpp/h` — Hardware ID to driver matching, ranking by version/date/status
- `install.cpp/h` — Driver installation via Windows Driver Installation APIs
- `enum.cpp/h` — Hardware device enumeration using SetupDi API
- `manager.cpp/h` — Orchestrates indexing, matching, and UI item management

#### Update and Distribution
- `update.cpp/h` — `Updater_t` class, libtorrent integration, torrent session management
- `usbwizard.cpp/h` — USB portable drive creation wizard
- `net.cpp` — Network utilities

#### User Interface
- `gui.cpp/h` — WidgetComposite pattern, `wPanel`, HoverVisiter, ClickVisiter
- `draw.cpp/h` — Canvas abstraction, Image class, double-buffering
- `theme.cpp/h` — Theme system, vTheme vault
- `themelist.cpp/h` — Theme definition parsing and management
- `langlist.cpp/h` — Language file parsing and management
- `stddlg.cpp` — Standard dialog utilities
- `darkmode.cpp/h` — Windows 10/11 dark mode support via subclassing

#### System Integration
- `system.cpp/h` — `SystemImp` wrapper for Windows APIs
- `baseboard.cpp` — Motherboard information via WMI
- `log.cpp/hpp` — `Log_t` diagnostics, `Timers_t` performance measurement
- `registry.h` — Registry access utilities
- `msapi_utf8.h` — UTF-8 Windows API wrappers

#### Configuration and Utilities
- `settings.cpp/h` — `Settings_t` central configuration object
- `cli.cpp/h` — Command-line argument parsing
- `script.cpp/h` — Script execution engine
- `model.cpp/h` — Data model layer
- `stdio.cpp` — I/O utilities
- `stdfn.cpp` — Standard function helpers
- `string_utils.cpp/hpp` — String manipulation utilities

#### Version Management
- `Version.h` — Static version constants
- `VersionEx.h` — Generated version file (from template, gitignored)

### Build System Structure

The solution produces one main executable and one helper executable:

```
SDI.slnx
├── SDI.vcxproj              ─ Main application (EXE)
│   ├── torrent-rasterbar.vcxproj  ─ libtorrent (static lib)
│   ├── SevenZip.vcxproj           ─ 7-Zip SDK (static lib)
│   └── libwebp.vcxproj           ─ libwebp (static lib)
└── install64.vcxproj        ─ Driver install helper (EXE, elevated privileges)
```

**Build outputs:**
- Executables: root directory (e.g., `SDI.exe`)
- Intermediate objects: `obj/$(Configuration)_$(Platform)/$(ProjectName)/`
- Static libraries: `lib/$(Configuration)_$(Platform)/`

### Build Configurations

| Configuration | Platform | Output Name | Notes |
|---------------|----------|-------------|-------|
| Release       | x64      | SDI.exe     | MaxSpeed optimized (primary) |
| Release       | x86      | SDI.exe     | MaxSpeed optimized |
| Debug         | x64      | SDI_D.exe   | For development |
| Debug         | x86      | SDI_D.exe   | For development |

### Project Dependencies

SDI.vcxproj depends on three static library projects:
- **torrent-rasterbar** — libtorrent compiled with `TORRENT_DISABLE_LOGGING`, `TORRENT_DISABLE_EXTENSIONS` for size optimization. Source: 130+ files from `ext/libtorrent/src/`
- **SevenZip** — 7-Zip SDK with `Z7_NO_CRYPTO`, `Z7_LZMA_PROB32` (Win32), `Z7_LZMA_DEC_OPT` (x64). Source: C/C++ files from `ext/SevenZip/`
- **libwebp** — WebP image decoder. Source: `ext/libwebp/src/`

**install64.vcxproj** — Small helper executable (`src/install64.c`) for driver installation with elevated privileges. Output: `res/install64.exe` (embedded as resource in main SDI).

### External Dependencies (Git Submodules)

| Path             | Repository                                   | Branch  | Purpose                          |
|------------------|----------------------------------------------|---------|----------------------------------|
| `ext/boost`      | https://github.com/boostorg/boost.git        | develop | C++ utility libraries (ASIO, etc.) |
| `ext/libtorrent` | https://github.com/arvidn/libtorrent.git     | —       | BitTorrent protocol library      |
| `ext/libwebp`    | https://github.com/webmproject/libwebp.git   | main    | WebP image format decoding       |
| `ext/SevenZip`   | (local copy)                                 | —       | 7-Zip SDK for archive handling   |

**Integration Details:**
- **Boost**: Header-only usage, included via `/external:I "../ext/boost"` to suppress warnings. Only ~47 out of 200+ modules are initialized.
- **libtorrent**: Compiled to static library, headers from `ext/libtorrent/include`. Nested submodule `deps/try_signal` required.
- **7-Zip**: Compiled to static library, headers from `ext/SevenZip/c/`
- **libwebp**: Compiled to static library, headers from `ext/libwebp/src`

### Global Objects and Manager Pattern

The application uses globally accessible objects to coordinate subsystem interactions (defined in `src/main.cpp`):

| Object        | Type              | Purpose                             |
|---------------|-------------------|-------------------------------------|
| `MainWindow`  | `MainWindow_t`    | Primary GUI window and message loop |
| `Settings`    | `Settings_t`      | Central configuration               |
| `Popup`       | `Popup_t*`        | Popup/tooltip management            |
| `Updater`     | `Updater_t*`      | BitTorrent update manager           |
| `manager_g`   | `Manager*`        | Active driver manager instance      |
| `USBWiz`      | `USBWizard*`      | USB creation wizard                 |

Two `Manager` instances are maintained (`manager_v[2]`): one for active display, one for background operations.

### Resource Organization

**Embedded Resources** (`res/`):
- Images: WebP (logo, icons, buttons), BMP (watermarks), ICO (application icons)
- `SDI.manifest` — Application manifest
- `cli.txt` — CLI help text
- `install64.exe` — Embedded driver installation helper

**Dialog Templates** (defined in `src/SDI.rc`):
- `IDD_OPTIONS` — Options/settings dialog
- `IDD_ABOUT` — About dialog
- `IDD_UPDATE` — Update manager dialog
- Referenced via `resource.h` identifiers

**Language Files** (`Tools/Langs/`):
- 42 language definition text files
- Loaded by `langlist.cpp` into language vault
- Selected via `Settings_t::lang_id`

**Theme Files** (`Tools/themes/`):
- 11 theme definitions with associated image directories
- Loaded by `theme.cpp` into vTheme vault
- Selected via `Settings_t::theme_id`

### Configuration / Portable Design

- CFG file alongside executable (`sdi2.cfg`), no registry usage for settings
- Command-line arguments can override all settings (see `cli.cpp` and `res/cli.txt`)
- Key paths configured in CFG: `-drp_dir:` (driver packs), `-index_dir:` (cached indices), `-data_dir:` (Tools), `-log_dir:` (Logs)

### Runtime Directories

Created/populated at runtime:
- `drivers/` — User's downloaded driver pack archives (*.7z files)
- `indexes/` — Cached parsed driver metadata
- `update/` — Temporary directory for torrent downloads
- `Logs/` — Application log files

### Dark Mode (`src/darkmode.cpp/h`)

Windows 10/11 dark mode support via Win32 subclassing. Subclasses buttons, groupboxes, status bars, progress bars, and static text controls. Requires Windows build 17763+ (Win10 1809).

## Code Conventions

### Formatting

- `.editorconfig` sets guideline at 80 columns
- C++ source uses CRLF line endings
- Safety macros throughout: `safe_free()`, `safe_closehandle()`, `safe_strcpy()`, `safe_sprintf()`, `static_sprintf()`

### Logging

- `uprintf()` — Printf-style logging (defined in `main.h`)
- `vuprintf()` — Verbose-only logging (only when `Log.get_verbose()` is true)
- `vvuprintf()` — Extra-verbose logging (verbose level > 1)
- `duprintf()` — Debug-only logging (compiled out in Release)

### Key Type Definitions

- `ofst` — `typedef unsigned ofst` for offsets into packed data
- `DRIVER_STATUS` — Bitmask enum for driver match status (BETTER, SAME, WORSE, MISSING, etc.)
- `STATEMODE` — Application mode (REAL, EMUL, EXIT)
- `install_mode` — Installation state machine (NONE, INSTALLING, STOPPING, SCANNING)

### Thread Synchronization

- `CRITICAL_SECTION sync` — Global critical section for thread safety
- `Event` objects for install/device update coordination (`installupdate_event`, `deviceupdate_event`)
- Exit flags (`installupdate_exitflag`, `deviceupdate_exitflag`) for thread cancellation

## Key File Paths (Quick Reference)

| Pattern | Purpose |
|---------|---------|
| `src/main.cpp` | Application entry point, global objects, message loop |
| `src/main.h` | Core macros, safety wrappers, logging declarations |
| `src/indexing.cpp` | Driver pack scanning and INF parsing |
| `src/matcher.cpp` | Hardware-to-driver matching algorithm |
| `src/manager.cpp` | UI item management, orchestration |
| `src/install.cpp` | Driver installation logic |
| `src/enum.cpp` | Hardware device enumeration (SetupDi) |
| `src/update.cpp` | BitTorrent-based driver pack updates |
| `src/gui.cpp` | Widget system and UI composition |
| `src/draw.cpp` | Canvas rendering and double-buffering |
| `src/settings.cpp` | Configuration management |
| `src/cli.cpp` | Command-line argument processing |
| `src/install64.c` | Elevated-privilege driver installation helper |
| `Docs/building-win-x64.md` | Build instructions |
| `Docs/Theming.md` | Theme system documentation |
| `Version.ps1` | Version generation script |
| `.github/workflows/msbuild.yml` | CI pipeline definition |
