#include "pch.h"
#include "nvmp_state.h"
#include "nvmp_offsets.h"
#include "nvse_lite.h"
#include <psapi.h>

namespace nvmp {

template<typename T>
bool NVMPState::SafeRead(uintptr_t addr, T& out) const {
    __try {
        out = *reinterpret_cast<T*>(addr);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool NVMPState::SafeReadString(uintptr_t addr, std::string& out, size_t maxLen) const {
    __try {
        const char* str = reinterpret_cast<const char*>(addr);
        if (IsBadReadPtr(str, 1)) return false;
        out.clear();
        for (size_t i = 0; i < maxLen && str[i] != '\0'; i++) out += str[i];
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

const char* NVMPState::GetObjectName(uintptr_t netref) const {
    __try {
        uintptr_t vtable = *(uintptr_t*)netref;
        if (vtable < 0x10000) return nullptr;
        uintptr_t fn = *(uintptr_t*)(vtable + 0x24);
        if (fn < 0x10000) return nullptr;
        typedef const char* (__thiscall *GetNameFn)(void*);
        return ((GetNameFn)fn)((void*)netref);
    } __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}

std::string NVMPState::ParsePlayerName(const char* rawName) {
    if (!rawName) return "";
    const char* p = rawName;
    int bracketCount = 0;
    const char* nameStart = nullptr;
    while (*p) {
        if (*p == '[') {
            bracketCount++;
            if (bracketCount == 2) nameStart = p + 1;
        }
        if (*p == ']' && bracketCount == 2 && nameStart) {
            return std::string(nameStart, p - nameStart);
        }
        p++;
    }
    return rawName;
}

bool NVMPState::Initialize() {
    m_clientDll = GetModuleHandleA("client.dll");
    if (!m_clientDll) m_clientDll = GetModuleHandleA("nvmp\\client.dll");
    if (!m_clientDll) return false;

    m_clientBase = reinterpret_cast<uintptr_t>(m_clientDll);
    m_encounterGlobalAddr = m_clientBase + GLOBAL_ENCOUNTER_PTR_RVA;
    m_charMgrAddr = m_clientBase + GLOBAL_CHAR_MGR_RVA;

    LogMessage("[NVMP] client.dll base: 0x%08X", m_clientBase);

    // Runtime patch: bypass locality check in damage receiver
    // RVA 0x9CFC9: change 0F 85 (jne) to 90 E9 (NOP+JMP)
    // Allows client-side damage to ghost NPCs and other players
    {
        uintptr_t patchAddr = m_clientBase + 0x9CFC9;
        DWORD oldProt;
        if (VirtualProtect((void*)patchAddr, 2, PAGE_EXECUTE_READWRITE, &oldProt)) {
            uint8_t* p = (uint8_t*)patchAddr;
            if (p[0] == 0x0F && p[1] == 0x85) {
                p[0] = 0x90;  // NOP
                p[1] = 0xE9;  // JMP
                FlushInstructionCache(GetCurrentProcess(), (void*)patchAddr, 2);
                LogMessage("[NVMP] Ghost damage patch applied (locality bypass at 0x%08X)", patchAddr);
            } else {
                LogMessage("[NVMP] Ghost damage patch: unexpected bytes %02X %02X at 0x%08X", p[0], p[1], patchAddr);
            }
            VirtualProtect((void*)patchAddr, 2, oldProt, &oldProt);
        }
    }

    m_initialized = true;
    return true;
}

bool NVMPState::IsConnected() const {
    if (!m_initialized) return false;
    uintptr_t mgr = 0;
    if (!SafeRead(m_charMgrAddr, mgr) || mgr == 0) return false;
    return true;
}

bool NVMPState::IsEncounterSyncEnabled() const {
    return true;
}

void NVMPState::SetEncounterSync(bool actors, bool references, bool doors) {
    if (!m_initialized) return;
    uintptr_t encPtr = 0;
    if (!SafeRead(m_encounterGlobalAddr, encPtr) || encPtr == 0) return;

    __try {
        *reinterpret_cast<uint8_t*>(encPtr + ENC_FLAG_BASE + ENC_TYPE_ACTORS) = actors ? 1 : 0;
        *reinterpret_cast<uint8_t*>(encPtr + ENC_FLAG_BASE + ENC_TYPE_REFERENCE) = references ? 1 : 0;
        *reinterpret_cast<uint8_t*>(encPtr + ENC_FLAG_BASE + ENC_TYPE_DOOR) = doors ? 1 : 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
}

void NVMPState::ToggleEncounterSync() {}

// GetPlayers — stubbed for now, party HUD removed
std::vector<PlayerInfo> NVMPState::GetPlayers() {
    return {};
}

} // namespace nvmp
