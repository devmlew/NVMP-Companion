#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <windows.h>

namespace nvmp {

struct PlayerInfo {
    std::string name;        // parsed player name (without class prefix)
    std::string rawName;     // full name from GetName
    float health;
    float maxHealth;
    float posX, posY, posZ;
    bool isLocal;
    bool isValid;
    uintptr_t netref;        // for identity tracking
};

class NVMPState {
public:
    bool Initialize();
    bool IsConnected() const;
    bool IsEncounterSyncEnabled() const;
    void SetEncounterSync(bool actors, bool references, bool doors);
    void ToggleEncounterSync();
    uintptr_t GetClientBase() const { return m_clientBase; }

    // Read all connected players from the GameNetCharacter list
    std::vector<PlayerInfo> GetPlayers();

private:
    HMODULE  m_clientDll = nullptr;
    uintptr_t m_clientBase = 0;
    bool m_initialized = false;
    uintptr_t m_encounterGlobalAddr = 0;
    uintptr_t m_charMgrAddr = 0;

    template<typename T>
    bool SafeRead(uintptr_t addr, T& out) const;
    bool SafeReadString(uintptr_t addr, std::string& out, size_t maxLen = 64) const;

    const char* GetObjectName(uintptr_t netref) const;
    static std::string ParsePlayerName(const char* rawName);
};

} // namespace nvmp
