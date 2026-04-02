#include "pch.h"
#include "nvmp_state.h"
#include "nvse_lite.h"

namespace nvmp {

// 0=MP (multiplayer), 1=HB (hybrid - players only), 2=SP (singleplayer)
static int s_mode = 0;
static char s_originalName[256] = {0};

static const char* ModeNames[] = {"MULTIPLAYER", "HYBRID", "SINGLEPLAYER"};
static const char* ModeMarkers[] = {"", " [HB]", " [SP]"};

// Forward declarations
void UpdateModeIndicator(int mode);
void SetOverlayText(const char* playerName, int mode);

int GetCurrentMode() {
    return s_mode;
}

// Read the player's displayed name from FNV's ExtraDataList
// NV:MP sets the character name via ExtraName (type 0x01)
// ExtraDataList is inline at TESObjectREFR + 0x44
// BSExtraData: [vtable+0x00][next+0x04][type+0x08]
// ExtraName: [BSExtraData][BSStringT at +0x0C] where BSStringT has char* at +0x00
static const char* ReadPlayerName() {
    __try {
        uintptr_t player = *(uintptr_t*)0x011DEA3C;
        if (player < 0x10000) return nullptr;

        // ExtraDataList.m_data at player + 0x48 (inline ExtraDataList at +0x44, m_data at +0x04)
        uintptr_t data = *(uintptr_t*)(player + 0x48);
        int safety = 0;
        while (data > 0x10000 && safety < 200) {
            safety++;
            uint8_t type = *(uint8_t*)(data + 0x08);
            if (type == 0x01) { // ExtraName
                char* name = *(char**)(data + 0x0C);
                if (name && !IsBadReadPtr(name, 1) && name[0] >= 0x20 && name[0] <= 0x7E) {
                    return name;
                }
            }
            data = *(uintptr_t*)(data + 0x04); // next
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
    return nullptr;
}

void OnToggleMultiplayerKey(NVMPState& state) {
    s_mode = (s_mode + 1) % 3;

    // Get original name on first call
    if (s_originalName[0] == 0) {
        // Try ExtraName (NV:MP's display name)
        const char* name = ReadPlayerName();
        if (name && strlen(name) > 0) {
            strncpy(s_originalName, name, 250);
        }

        // Fallback: read from base form TESFullName at +0xD4
        if (s_originalName[0] == 0) {
            __try {
                uintptr_t player = *(uintptr_t*)0x011DEA3C;
                if (player > 0x10000) {
                    uintptr_t baseForm = *(uintptr_t*)(player + 0x20);
                    if (baseForm > 0x10000) {
                        char* n = *(char**)(baseForm + 0xD4);
                        if (n && !IsBadReadPtr(n, 4) && n[0] >= 0x20 && n[0] <= 0x7E && strlen(n) > 1) {
                            strncpy(s_originalName, n, 250);
                        }
                    }
                }
            } __except (EXCEPTION_EXECUTE_HANDLER) {}
        }

        // Strip any existing mode markers from saved games
        for (const char* marker : {" [SP]", " [HB]"}) {
            char* found = strstr(s_originalName, marker);
            if (found) *found = '\0';
        }

        if (s_originalName[0]) {
            LogMessage("[NVMP] Player name: \"%s\"", s_originalName);
        }

        if (s_originalName[0] == 0) {
            strcpy(s_originalName, "Player");
            LogMessage("[NVMP] WARNING: Could not read player name, using fallback");
        }
    }

    // Set name with mode marker
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "player.SetName \"%s%s\"", s_originalName, ModeMarkers[s_mode]);
    RunScript(cmd);

    // Disable encounter sync in both HB and SP modes
    if (state.IsConnected()) {
        bool enableEnc = (s_mode == 0);
        state.SetEncounterSync(enableEnc, enableEnc, enableEnc);
    }

    // Update overlay text with name + mode
    SetOverlayText(s_originalName, s_mode);
    UpdateModeIndicator(s_mode);

    LogMessage("[NVMP] Mode: %s (%s)", ModeNames[s_mode], cmd);
}

bool IsSinglePlayerMode() {
    return s_mode != 0;
}

} // namespace nvmp
