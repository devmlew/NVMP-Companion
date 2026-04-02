# NV:MP Companion Plugin

NVSE plugin for Fallout New Vegas Multiplayer (NV:MP) co-op servers. Adds quality-of-life features not available in the base co-op mode.

## Features

- **Mode Toggle** (configurable hotkey, default F9) — Cycle between Multiplayer, Hybrid, and Singleplayer modes in real-time
  - **MP**: Full multiplayer sync (default)
  - **Hybrid**: Only player characters sync; NPCs/references stay private
  - **Singleplayer**: All sync disabled; play solo while staying connected
- **NV:MP Overlay Integration** — Current mode displayed on the NV:MP connection overlay
- **Ghost NPC Damage Fix** — Runtime patch allowing players to damage ghost NPCs (other players' synced NPCs)
- **MCM Support** — Keybind configurable through Mod Configuration Menu
- **Name-based Signaling** — Mode state synced to server via player name markers (`[SP]`/`[HB]`)

## Installation

### Client (each player)
1. Requires [xNVSE](https://github.com/xNVSE/NVSE)
2. Copy `nvmp_companion.dll` to `Data\NVSE\Plugins\`
3. Copy `NVMPCompanion.json` to `Data\MCM\` (for MCM hotkey settings)
4. Launch NV:MP normally

### Server
1. Start `nvmp_storyserver.exe` normally
2. Run `nvmp_inject.exe` to inject `nvmp_server_hook.dll` into the running server
3. The hook applies runtime patches for ownership bypass and mode-aware sync blocking

No permanent binary modifications are made to `client.dll` or `nvmp_storyserver.exe`.

## Building

Requires Visual Studio 2022 and CMake:

```
# Client plugin
cd nvmp_companion
mkdir build && cd build
cmake .. -G "Visual Studio 17 2022" -A Win32
cmake --build . --config Release
# Output: build/bin/Release/nvmp_companion.dll

# Server hook + injector
cd nvmp_server_hook
mkdir build && cd build
cmake .. -G "Visual Studio 17 2022" -A Win32
cmake --build . --config Release
# Output: build/bin/Release/nvmp_server_hook.dll, nvmp_inject.exe
```

## How It Works

### Client Plugin (nvmp_companion.dll)
- Loaded by xNVSE into FalloutNV.exe alongside NV:MP's client.dll
- Reads client.dll memory for connection state and encounter flags
- Applies runtime patches: locality bypass (damage receiver) and overlay text hook
- Mode changes append `[SP]`/`[HB]` markers to the player name, which sync to the server via NV:MP's built-in name replication

### Server Hook (nvmp_server_hook.dll)
- Injected into nvmp_storyserver.exe at runtime
- Hooks `ShouldSynchronise` to block sync for SP/HB mode players
- Background thread scans the GameNetCharacter linked list every 2 seconds for name markers
- Applies ownership bypass in the attack handler so ghost NPCs can take damage

### Offsets
All offsets are for NV:MP version 8.1. See `nvmp_companion/include/nvmp_offsets.h` and `PATCHES.md` for the complete reference.
