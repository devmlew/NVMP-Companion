# NV:MP Runtime Patches

All patches are applied at runtime by the NVSE companion plugin (client-side) and the server hook DLL (server-side). No binary files are modified on disk.

## Binaries

| Binary | Type | Architecture |
|---|---|---|
| `nvmp_storyserver.exe` | Native Win32 PE32 (C/C++) | x86 32-bit |
| `nvmp/client.dll` | Win32 DLL PE32 | x86 32-bit |

All offsets target NV:MP version 8.1.

---

## Client Patches (applied by nvmp_companion.dll)

### Ghost NPC Damage Fix

**Applied by:** `nvmp_companion.dll` on plugin load
**Target:** `client.dll` damage receiver at RVA `0x0009CFC9`

Ghost NPCs (other players' synced NPCs) can deal damage to you, but you cannot damage them. The damage receiver checks the victim's locality byte and rejects damage if the victim is non-local (ghost).

**Validation chain** (all must pass for damage to apply):
```
1. Get attacker net ref (from message offset +0x10)
2. Get victim net ref (from message offset +0xA0)
3. Attacker null check
4. Attacker PVS check: [attacker+0xF4] & 0x01
5. Victim null check
6. Victim PVS check: [victim+0xF4] & 0x01
7. Victim locality: [victim+0x1AE] != 0          <-- PATCHED
8. Victim game ref valid
9. Attacker game ref valid
10. Attacker not dead: [attacker+0x108] lifeState
11. Victim not dead: [victim+0x108] lifeState
12. Apply damage via Actor::DamageActorValue
```

Step 7 checks `[victim+0x1AE] != 0` — locality `0` = ghost/non-local, `1` = local/owned. Ghost NPCs have locality `0`, so damage is rejected.

**Patch:** Change `0F 85` (JNE) to `90 E9` (NOP + JMP) at RVA `0x0009CFC9`

The plugin verifies the original bytes before patching and logs the result.

### Overlay Text Hook

**Applied by:** `nvmp_companion.dll` on plugin load
**Target:** `client.dll` overlay renderer at RVA `0x0009AA3C`

Redirects the last `%s` argument of the "Connected" line sprintf to display the current player name and mode (MP/Hybrid/SP) on the NV:MP overlay.

**Original:** `8D 85 2C FE FF FF 50` (lea eax, [ebp-0x1D4]; push eax) — pushes dev stats string
**Patched:** `B8 [addr] 50 90` (mov eax, &modeText; push eax; nop) — pushes our mode text

---

## Server Patches (applied by nvmp_server_hook.dll)

### Ownership Bypass

**Applied by:** `nvmp_server_hook.dll` on injection
**Target:** `nvmp_storyserver.exe` attack handler at RVA `0x001098E8`

The attack handler checks if the attacking player owns the target NPC and rejects the attack if they don't. This prevents players from damaging ghost NPCs.

```
cmp edi, [eax]        ; compare sender owner with target owner
je +0x2F              ; if SAME owner -> process attack
                      ; fall through -> reject
```

**Patch:** Change `74` (JE) to `EB` (JMP) — always process the attack regardless of ownership.

### ShouldSynchronise Hook

**Applied by:** `nvmp_server_hook.dll` on injection
**Target:** `nvmp_storyserver.exe` at RVA `0x00105FE0`

Hooks the `ShouldSynchronise` function to block network sync for players in Singleplayer or Hybrid mode. Uses hand-assembled x86 code in VirtualAlloc'd executable memory.

**Singleplayer mode** (`[SP]` in player name): blocks ALL sync both directions
**Hybrid mode** (`[HB]` in player name): blocks non-character sync (NPCs, references, doors); allows player characters through

A background thread scans the GameNetCharacter linked list every 2 seconds to detect `[SP]`/`[HB]` markers in player names. The target player is resolved using the internal function at RVA `0x000FDF00`.

---

## Key Offsets Reference

### client.dll Network Reference

| Offset | Type | Description |
|---|---|---|
| `+0x04` | `char*` | Class name string pointer |
| `+0xF4` | `byte` | Flags (bit `0x01` = in PVS) |
| `+0x1AE` | `byte` | Locality: `0` = ghost, `1` = local |

### nvmp_storyserver.exe

| RVA | Description |
|---|---|
| `0x002F9CC4` | GameNetCharacter manager global pointer |
| `0x00105FE0` | ShouldSynchronise function |
| `0x001098E8` | Attack handler ownership check |
| `0x000FDF00` | ResolveTarget function (target context -> player netref) |

### Linked List Structure (server)

```
[manager + 0x54] = linked list head (sentinel)
[node + 0x00]    = next node pointer
[node + 0x0C]    = netref pointer
```

### FNV Engine

| Address | Description |
|---|---|
| `0x011DEA3C` | `g_thePlayer` (PlayerCharacter*) |

### Player Base Form (TESNPC)

| Offset | Description |
|---|---|
| `+0xD4` | Character name string pointer (TESFullName) |

---

## Server Settings Reference

### server.cfg

| Setting | Default | Description |
|---|---|---|
| `EnableConsole` | `0` | Allow console commands |
| `EnableSaving` | `1` | Allow local saves |
| `EnableVATS` | `0` | Allow VATS |
| `Difficulty` | `2` | 0=VEasy, 1=Easy, 2=Normal, 3=Hard, 4=VHard |
| `EnablePartySystem` | `1` | Party blips and location overlay |
| `UseClientsideHitReg` | `1` | Client-side hit detection |

### client.cfg

| Setting | Default | Description |
|---|---|---|
| `bDisableNPCAttacks` | `0` | Disable all NPC attacks |
| `bDisablePlayerCollision` | `0` | Disable player collision |
| `bSetDemigodMode` | `0` | Reduced damage mode |
| `AllowMultipleNVInstances` | `0` | Run multiple FNV instances |
