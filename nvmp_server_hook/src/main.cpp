#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>
#include <cstdint>
#include <cstring>

// ============================================================
// NV:MP Server Hook DLL - Combined approach:
// 1. Background thread scans GameNetCharacter list for [SP] names
// 2. Maintains a set of "singleplayer" netref pointers
// 3. Hook on ShouldSynchronise returns FALSE if source belongs to SP player
//    (prevents OTHER players' objects from syncing TO the SP player)
// ============================================================

static const uint32_t SHOULD_SYNC_RVA = 0x00105FE0;
static const uint32_t CHAR_MGR_RVA = 0x002F9CC4;

static FILE* g_log = nullptr;
static volatile bool g_running = false;
static HANDLE g_thread = nullptr;
static uintptr_t g_hookAddr = 0;
static uint8_t g_originalBytes[5] = {0};
static uint8_t* g_codeBlock = nullptr;

// Player mode tracking
// SP = full singleplayer (block all sync both directions)
// HB = hybrid (block NPCs/references, allow player characters)
struct PlayerEntry {
    uintptr_t netref;
    int ownerID;  // from vtable[1]
};
static volatile PlayerEntry g_spPlayers[8] = {{0}};
static volatile int g_spCount = 0;
static volatile PlayerEntry g_hbPlayers[8] = {{0}};
static volatile int g_hbCount = 0;

static void Log(const char* fmt, ...) {
    if (!g_log) g_log = fopen("nvmp_server_hook.log", "w");
    if (!g_log) return;
    va_list args;
    va_start(args, fmt);
    vfprintf(g_log, fmt, args);
    fprintf(g_log, "\n");
    fflush(g_log);
    va_end(args);
}

static const char* GetObjectName(uintptr_t netref) {
    __try {
        uintptr_t vtable = *(uintptr_t*)netref;
        if (vtable < 0x10000) return nullptr;
        uintptr_t fn = *(uintptr_t*)(vtable + 0x24);
        if (fn < 0x10000) return nullptr;
        typedef const char* (__thiscall *GetNameFn)(void*);
        return ((GetNameFn)fn)((void*)netref);
    } __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}

static int GetOwnerID(uintptr_t netref) {
    __try {
        uintptr_t vtable = *(uintptr_t*)netref;
        if (vtable < 0x10000) return -1;
        // vtable[1] = GetOwner, returns pointer to owner struct, first dword is ID
        uintptr_t fn = *(uintptr_t*)(vtable + 0x04);
        if (fn < 0x10000) return -1;
        typedef int* (__thiscall *GetOwnerFn)(void*);
        int* ownerPtr = ((GetOwnerFn)fn)((void*)netref);
        if (ownerPtr && !IsBadReadPtr(ownerPtr, 4)) return *ownerPtr;
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
    return -1;
}

static bool IsPlayerInSP(uintptr_t netref) {
    for (int i = 0; i < g_spCount; i++) {
        if (g_spPlayers[i].netref == netref) return true;
    }
    return false;
}

static bool IsOwnedBySP(uintptr_t netref) {
    if (g_spCount == 0) return false;  // fast path
    int ownerID = GetOwnerID(netref);
    if (ownerID <= 0) return false;  // invalid or default
    for (int i = 0; i < g_spCount; i++) {
        if (g_spPlayers[i].ownerID > 0 && g_spPlayers[i].ownerID == ownerID) return true;
    }
    return false;
}

static bool IsPlayerInHybrid(uintptr_t netref) {
    for (int i = 0; i < g_hbCount; i++) {
        if (g_hbPlayers[i].netref == netref) return true;
    }
    return false;
}

static bool IsOwnedByHybrid(uintptr_t netref) {
    if (g_hbCount == 0) return false;  // fast path
    int ownerID = GetOwnerID(netref);
    if (ownerID <= 0) return false;
    for (int i = 0; i < g_hbCount; i++) {
        if (g_hbPlayers[i].ownerID > 0 && g_hbPlayers[i].ownerID == ownerID) return true;
    }
    return false;
}

static bool IsCharacterType(uintptr_t netref) {
    __try {
        const char* name = GetObjectName(netref);
        if (name && strstr(name, "GameNetCharacter")) return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
    return false;
}

// Resolves param_2 (target context) to the target player's netref
typedef int (__cdecl *ResolveTargetFn)(int param_2);
static ResolveTargetFn g_resolveTarget = nullptr;

static void __cdecl CheckShouldBlock(uintptr_t thisObj, uintptr_t param2, uintptr_t* resultPtr) {
    *resultPtr = 0xFF;  // default: don't override

    // === SINGLEPLAYER MODE: block ALL sync ===

    // Outgoing: is source an SP player's character?
    if (IsPlayerInSP(thisObj)) {
        *resultPtr = 0;
        return;
    }

    // Incoming: is target an SP player?
    if (g_resolveTarget && param2 > 0x10000) {
        __try {
            uintptr_t targetPlayer = (uintptr_t)g_resolveTarget((int)param2);
            if (targetPlayer > 0x10000 && IsPlayerInSP(targetPlayer)) {
                *resultPtr = 0;
                return;
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {}
    }

    // === HYBRID MODE: block non-character objects, allow player characters ===
    if (g_resolveTarget && param2 > 0x10000) {
        __try {
            uintptr_t targetPlayer = (uintptr_t)g_resolveTarget((int)param2);
            if (targetPlayer > 0x10000 && IsPlayerInHybrid(targetPlayer)) {
                // Target is hybrid. Only allow player characters through.
                if (!IsCharacterType(thisObj)) {
                    *resultPtr = 0;  // block NPCs, references, everything else
                    return;
                }
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {}
    }

    // Outgoing: block non-character objects owned by hybrid players
    // TODO: re-enable once GetOwnerID is verified safe
    // For now, rely on client-side encounter disable for outgoing
    // if (!IsCharacterType(thisObj) && IsOwnedByHybrid(thisObj)) {
    //     *resultPtr = 0;
    //     return;
    // }
}

// Background thread: scan character list, update SP player tracking
static DWORD WINAPI ScanThread(LPVOID) {
    uintptr_t serverBase = (uintptr_t)GetModuleHandleA(nullptr);
    uintptr_t mgrAddr = serverBase + CHAR_MGR_RVA;

    Log("[SCAN] Scanning GameNetCharacter list for [SP] names...");

    while (g_running) {
        Sleep(2000);
        if (!g_running) break;

        __try {
            uintptr_t manager = *(uintptr_t*)mgrAddr;
            if (manager < 0x10000) continue;

            uintptr_t sentinel = manager + 0x54;
            uintptr_t head = *(uintptr_t*)sentinel;
            if (head < 0x10000 || head == sentinel) continue;

            // Rebuild SP and HB lists
            int newSPCount = 0, newHBCount = 0;
            PlayerEntry newSP[8] = {{0}}, newHB[8] = {{0}};

            uintptr_t node = head;
            int safety = 0;
            while (node != sentinel && node > 0x10000 && safety < 500) {
                safety++;
                __try {
                    uintptr_t netref = *(uintptr_t*)(node + 0x0C);
                    if (netref > 0x10000) {
                        const char* name = GetObjectName(netref);
                        if (name) {
                            if (strstr(name, "[SP]") && newSPCount < 8) {
                                newSP[newSPCount].netref = netref;
                                newSP[newSPCount].ownerID = GetOwnerID(netref);
                                newSPCount++;
                                if (!IsPlayerInSP(netref))
                                    Log("[SCAN] '%s' owner=%d -> SINGLEPLAYER", name, GetOwnerID(netref));
                            } else if (strstr(name, "[HB]") && newHBCount < 8) {
                                newHB[newHBCount].netref = netref;
                                newHB[newHBCount].ownerID = GetOwnerID(netref);
                                newHBCount++;
                                if (!IsPlayerInHybrid(netref))
                                    Log("[SCAN] '%s' owner=%d -> HYBRID", name, GetOwnerID(netref));
                            }
                        }
                    }
                } __except (EXCEPTION_EXECUTE_HANDLER) {}
                node = *(uintptr_t*)node;
            }

            // Log players leaving modes
            for (int i = 0; i < g_spCount; i++) {
                bool still = false;
                for (int j = 0; j < newSPCount; j++) if (g_spPlayers[i].netref == newSP[j].netref) { still = true; break; }
                if (!still && g_spPlayers[i].netref) {
                    const char* n = GetObjectName(g_spPlayers[i].netref);
                    Log("[SCAN] '%s' -> MULTIPLAYER (was SP)", n ? n : "?");
                }
            }
            for (int i = 0; i < g_hbCount; i++) {
                bool still = false;
                for (int j = 0; j < newHBCount; j++) if (g_hbPlayers[i].netref == newHB[j].netref) { still = true; break; }
                if (!still && g_hbPlayers[i].netref) {
                    const char* n = GetObjectName(g_hbPlayers[i].netref);
                    Log("[SCAN] '%s' -> MULTIPLAYER (was HB)", n ? n : "?");
                }
            }

            memcpy((void*)g_spPlayers, newSP, sizeof(newSP));
            g_spCount = newSPCount;
            memcpy((void*)g_hbPlayers, newHB, sizeof(newHB));
            g_hbCount = newHBCount;

        } __except (EXCEPTION_EXECUTE_HANDLER) {}
    }
    return 0;
}

static bool InstallHook() {
    uintptr_t serverBase = (uintptr_t)GetModuleHandleA(nullptr);
    g_hookAddr = serverBase + SHOULD_SYNC_RVA;
    uintptr_t returnAddr = g_hookAddr + 5;

    // Resolve FUN_004fdf00 (resolves target context to player netref)
    g_resolveTarget = (ResolveTargetFn)(serverBase + 0x000FDF00);
    Log("[HOOK] ResolveTarget at: 0x%08X", (uintptr_t)g_resolveTarget);

    // Runtime patch: Ownership bypass in attack handler
    // Server RVA 0x1098E8: 74 (JE) -> EB (JMP) — allows cross-player NPC damage
    __try {
        uintptr_t ownerCheckAddr = serverBase + 0x001098E8;
        DWORD oldProt;
        if (VirtualProtect((void*)ownerCheckAddr, 1, PAGE_EXECUTE_READWRITE, &oldProt)) {
            if (*(uint8_t*)ownerCheckAddr == 0x74) {
                *(uint8_t*)ownerCheckAddr = 0xEB;
                Log("[HOOK] Ownership bypass applied (allows ghost NPC damage)");
            } else {
                Log("[HOOK] Ownership byte already patched: 0x%02X", *(uint8_t*)ownerCheckAddr);
            }
            VirtualProtect((void*)ownerCheckAddr, 1, oldProt, &oldProt);
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        Log("[HOOK] Ownership bypass patch failed");
    }

    memcpy(g_originalBytes, (void*)g_hookAddr, 5);
    if (memcmp(g_originalBytes, "\x55\x8B\xEC\x6A\xFF", 5) != 0) {
        Log("[HOOK] Byte mismatch!"); return false;
    }

    g_codeBlock = (uint8_t*)VirtualAlloc(nullptr, 128, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!g_codeBlock) return false;

    // Hook code block layout:
    // pushad -> sub esp,4 (result) -> push args -> call CheckShouldBlock
    // If result == 0: popad, xor al,al, ret 4 (block sync)
    // If result != 0: popad, execute original bytes, jmp back

    int pos = 0;

    // pushad
    g_codeBlock[pos++] = 0x60;

    // sub esp, 4 (make room for result)
    g_codeBlock[pos++] = 0x83; g_codeBlock[pos++] = 0xEC; g_codeBlock[pos++] = 0x04;
    // lea eax, [esp] (result ptr)
    g_codeBlock[pos++] = 0x8D; g_codeBlock[pos++] = 0x04; g_codeBlock[pos++] = 0x24;
    // push eax (result ptr)
    g_codeBlock[pos++] = 0x50;

    // Stack layout after pushad(32) + sub esp 4(4) + push resultPtr(4):
    // Original stack: [ret_addr][param_2]
    // param_2 at [esp+44], saved ECX at [esp+36]

    // push param_2 (target context — [esp+44])
    g_codeBlock[pos++] = 0xFF; g_codeBlock[pos++] = 0x74; g_codeBlock[pos++] = 0x24;
    g_codeBlock[pos++] = 0x2C; // [esp+44]

    // push ecx (this — [esp+32+4] = [esp+36] after prev push)
    g_codeBlock[pos++] = 0xFF; g_codeBlock[pos++] = 0x74; g_codeBlock[pos++] = 0x24;
    g_codeBlock[pos++] = 0x24; // [esp+36]

    // call CheckShouldBlock(thisObj, param2, resultPtr)
    g_codeBlock[pos++] = 0xE8;
    int call_pos = pos;
    pos += 4;
    // add esp, 12 (3 args)
    g_codeBlock[pos++] = 0x83; g_codeBlock[pos++] = 0xC4; g_codeBlock[pos++] = 0x0C;
    // pop eax (result)
    g_codeBlock[pos++] = 0x58;
    // cmp al, 0
    g_codeBlock[pos++] = 0x3C; g_codeBlock[pos++] = 0x00;
    // jne no_block (result != 0 means don't override)
    g_codeBlock[pos++] = 0x75;
    int jne_pos = pos; g_codeBlock[pos++] = 0x00;

    // BLOCK: popad, return 0
    g_codeBlock[pos++] = 0x61; // popad
    g_codeBlock[pos++] = 0x32; g_codeBlock[pos++] = 0xC0; // xor al, al
    g_codeBlock[pos++] = 0xC2; g_codeBlock[pos++] = 0x04; g_codeBlock[pos++] = 0x00; // ret 4

    // no_block:
    int no_block = pos;
    g_codeBlock[jne_pos] = (uint8_t)(no_block - (jne_pos + 1));
    // popad
    g_codeBlock[pos++] = 0x61;
    // original: push ebp / mov ebp,esp / push -1
    g_codeBlock[pos++] = 0x55;
    g_codeBlock[pos++] = 0x8B; g_codeBlock[pos++] = 0xEC;
    g_codeBlock[pos++] = 0x6A; g_codeBlock[pos++] = 0xFF;
    // jmp return
    g_codeBlock[pos++] = 0xE9;
    int32_t jmpRel = (int32_t)(returnAddr - ((uintptr_t)&g_codeBlock[pos] + 4));
    memcpy(&g_codeBlock[pos], &jmpRel, 4);
    pos += 4;

    // Fix call
    uintptr_t callFrom = (uintptr_t)&g_codeBlock[call_pos + 4];
    int32_t callRel = (int32_t)((uintptr_t)CheckShouldBlock - callFrom);
    memcpy(&g_codeBlock[call_pos], &callRel, 4);

    Log("[HOOK] Code block at 0x%08X (%d bytes)", (uintptr_t)g_codeBlock, pos);

    // Install JMP
    DWORD oldProtect;
    VirtualProtect((void*)g_hookAddr, 5, PAGE_EXECUTE_READWRITE, &oldProtect);
    uint8_t jmpPatch[5] = {0xE9};
    int32_t hookRel = (int32_t)((uintptr_t)g_codeBlock - (g_hookAddr + 5));
    memcpy(&jmpPatch[1], &hookRel, 4);
    memcpy((void*)g_hookAddr, jmpPatch, 5);
    VirtualProtect((void*)g_hookAddr, 5, oldProtect, &oldProtect);
    FlushInstructionCache(GetCurrentProcess(), (void*)g_hookAddr, 5);

    Log("[HOOK] ShouldSynchronise hooked — blocks sync FROM [SP] players");
    return true;
}

static void RemoveHook() {
    if (g_hookAddr && g_originalBytes[0]) {
        DWORD oldProtect;
        VirtualProtect((void*)g_hookAddr, 5, PAGE_EXECUTE_READWRITE, &oldProtect);
        memcpy((void*)g_hookAddr, g_originalBytes, 5);
        VirtualProtect((void*)g_hookAddr, 5, oldProtect, &oldProtect);
    }
    if (g_codeBlock) VirtualFree(g_codeBlock, 0, MEM_RELEASE);
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        if (InstallHook()) {
            g_running = true;
            g_thread = CreateThread(nullptr, 0, ScanThread, nullptr, 0, nullptr);
            Log("[SCAN] Background scan started");
        }
    } else if (reason == DLL_PROCESS_DETACH) {
        g_running = false;
        if (g_thread) { WaitForSingleObject(g_thread, 3000); CloseHandle(g_thread); }
        RemoveHook();
        if (g_log) { Log("Unloaded"); fclose(g_log); }
    }
    return TRUE;
}
