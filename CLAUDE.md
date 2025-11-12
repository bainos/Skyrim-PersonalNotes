# BainosNighteyeFix - SKSE Plugin Project

## Overview

SKSE plugin for Skyrim that integrates Night Eye spells with ENB's Khajiit Night Vision effect, with state persistence across saves. Supports multiple Night Eye variants (Khajiit, Vampire, Werewolf) via configurable INI.

## Architecture

### C++ Plugin (`plugin.cpp`)

- **Entry Point**: `SKSEPluginLoad()` - Initializes SKSE and registers for API init events
- **Initialization**: `InitializePlugin()` - Sets up logging, serialization, and messaging
- **ENB Integration**:
  - `g_ENB` - Global ENB SDK pointer (v1001)
  - `ENBCallback()` - Updates ENB parameter `ENBEFFECT.FX::KNActive` on PostLoad/EndFrame
  - Sets ENB parameter based on Night Eye equip state
- **Event Handling**:
  - `EquipHandler` - Monitors `TESEquipEvent` for Night Eye spells (from `Config::nightEyeSpellFormIDs`)
  - Uses `std::set<RE::FormID>` for O(log n) lookup
  - On equip: Sets `KNActive=true`, shows "Night Eye: ON"
  - On unequip: Sets `KNActive=false`, shows "Night Eye: OFF"
- **State Persistence**:
  - `NightEyeState` - Singleton managing `isActive` bool
  - SKSE serialization (Save/Load/Revert callbacks)
  - Restores ENB state on game load
- **Message Handler**: Responds to `kPostLoad` (ENB API), `kDataLoaded` (register events)

### Supporting Files

- `patch_includes.h` - Fixes CommonLibSSE/STL namespace issues, includes SKSE PCH
- `deploy.ps1` - Development deployment script (copies to game mod folder)
- `package-mod.ps1` - Distribution packaging script (creates RudyENB-patch and NoPatch variants)

## Dependencies (via vcpkg)

- `commonlibsse-ng-ae` - CommonLibSSE-NG fork for Anniversary Edition
- `enb-api` - ENB Series SDK v1001 (custom vcpkg port)
- `spdlog` - Logging library
- `iniparser` - INI configuration file parser
- Windows API (Psapi.h)

## Key Features

- ENB Night Vision integration via `KNActive` parameter
- Multiple Night Eye spell support (Khajiit, Vampire, Werewolf, custom)
- Automatic detection of Night Eye spell equip/unequip
- State persistence across save/load cycles
- Event-driven architecture
- Configurable via INI file with named spell entries
- Debug notifications in-game
- Comprehensive logging (`BainosNighteyeFix.log`)

## Build System

- CMake-based
- vcpkg for dependencies
- Target: x64 Windows

## Configuration

- **File**: `Data/SKSE/Plugins/ENBNighteyeFix.ini`
- **Format**: Named entries in `[General]` section

  ```ini
  [General]
  PowerKhajiitNightEye = 0x000AA01D
  VampireHunterSight = 0x000C4DE1
  WerewolfNightEye = 0x0006B10E
  ```

- Plugin enumerates all keys in `[General]` section using `iniparser_getsecnkeys()`
- Add unlimited Night Eye variants by adding more entries
- Triggers on equip/unequip (not cast)

## Packaging

- **Script**: `package-mod.ps1`
- **Variants**:
  - `ENBNighteyeFix-RudyENB-patch.zip` - Includes `Root/enbseries/` ENB effect files
  - `ENBNighteyeFix-NoPatch.zip` - Empty `Root/enbseries/` folder for other ENB presets
- **Output**: `dist/` directory

## Development Resources

- **CommonLibSSE-NG**: `/mnt/c/dev/CommonLibSSE-NG/include/` - Reference for all RE/SKSE definitions

## [**MANDATORY**] Communication with Claude

- Keep chat responses super concise. No unnecessary explanations or preambles.
- Keep it simple. If something becomes complex throw a warning
- Do not guess. You are not supposed to know everything:
  1. Check existing source code
  2. Search the web for official documentation
  3. Acknowledge you do not know and ask for help
