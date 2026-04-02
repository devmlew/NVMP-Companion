#include "pch.h"
#include "nvse_lite.h"
#include "nvmp_offsets.h"
#include "nvmp_state.h"

// ============================================================
// NV:MP Companion Plugin
// Configurable hotkey: toggle multiplayer/singleplayer/hybrid
// ============================================================

static NVSEMessagingInterface* g_messaging = nullptr;
static PluginHandle g_pluginHandle = kPluginHandle_Invalid;
static nvmp::NVMPState g_nvmpState;
static bool g_stateInitialized = false;

// Config — read from INI
static int g_toggleKey = VK_F9;
static bool s_prevToggle = false;

namespace nvmp {
    void OnToggleMultiplayerKey(NVMPState& state);
    bool IsSinglePlayerMode();
    int  GetCurrentMode();
    void InjectHUD();
    void UpdateModeIndicator(int mode);
    void SetOverlayText(const char* playerName, int mode);
    void ToggleHUD();
    void UpdateHUD(NVMPState& state);
}

void Notify(const char* msg) {
    LogMessage(msg);
    ConsolePrint(msg);
}

// ============================================================
// INI Config
// ============================================================

static char g_iniPath[MAX_PATH] = {0};

static void LoadConfig() {
    if (g_iniPath[0] == 0) {
        GetModuleFileNameA(nullptr, g_iniPath, MAX_PATH);
        char* lastSlash = strrchr(g_iniPath, '\\');
        if (lastSlash) {
            strcpy(lastSlash + 1, "Data\\config\\NVMPCompanion.ini");
        }
    }

    // MCM stores DirectX scancodes — convert to VK code
    // Default: 67 = DIK_F9
    int scanCode = GetPrivateProfileIntA("Controls", "iToggleKey", 67, g_iniPath);
    if (scanCode > 0 && scanCode < 256) {
        UINT vk = MapVirtualKeyA(scanCode, MAPVK_VSC_TO_VK);
        if (vk > 0 && (int)vk != g_toggleKey) {
            g_toggleKey = (int)vk;
            LogMessage("[NVMP] Config: scancode=%d -> VK=0x%02X", scanCode, g_toggleKey);
        }
    }
}

// Called periodically to pick up MCM changes
static void RefreshConfig() {
    static DWORD s_lastCheck = 0;
    DWORD now = GetTickCount();
    if (now - s_lastCheck < 2000) return; // check every 2 seconds
    s_lastCheck = now;
    LoadConfig();
}

// ============================================================
// Main update
// ============================================================

void UpdateCompanion() {
    static bool s_welcomed = false;
    if (!s_welcomed) {
        s_welcomed = true;
        char msg[128];
        snprintf(msg, sizeof(msg), "[NVMP] Companion active - key 0x%02X: toggle MP/HB/SP", g_toggleKey);
        Notify(msg);
    }

    if (!g_stateInitialized) {
        if (g_nvmpState.Initialize()) {
            g_stateInitialized = true;
            LogMessage("[NVMP] client.dll at 0x%08X", g_nvmpState.GetClientBase());
        }
        return;
    }

    // Only process when game window is focused
    HWND fg = GetForegroundWindow();
    DWORD pid = 0;
    GetWindowThreadProcessId(fg, &pid);
    if (pid != GetCurrentProcessId()) return;

    // Re-read config for MCM changes
    RefreshConfig();

    // Inject mode indicator HUD
    nvmp::InjectHUD();

    // Toggle key
    bool pressed = (GetAsyncKeyState(g_toggleKey) & 0x8000) != 0;
    if (pressed && !s_prevToggle) {
        nvmp::OnToggleMultiplayerKey(g_nvmpState);
        int mode = nvmp::GetCurrentMode();
        const char* names[] = {"MULTIPLAYER", "HYBRID", "SINGLEPLAYER"};
        char msg[128];
        snprintf(msg, sizeof(msg), "[NVMP] >> %s MODE <<", names[mode]);
        Notify(msg);
        Beep(mode == 0 ? 400 : (mode == 1 ? 600 : 800), 150);
    }
    s_prevToggle = pressed;
}

// ============================================================
// NVSE callbacks
// ============================================================

void NVSEMessageHandler(NVSEMessagingInterface::Message* msg) {
    if (msg->type == NVSEMessagingInterface::kMessage_MainGameLoop) {
        UpdateCompanion();
    }
}

extern "C" {

__declspec(dllexport) bool NVSEPlugin_Query(const NVSEInterface* nvse, PluginInfo* info) {
    info->infoVersion = PluginInfo::kInfoVersion;
    info->name = "nvmp_companion";
    info->version = 2;

    if (nvse->isEditor) return false;
    LogMessage("[NVMP] Query: NVSE v%u, runtime v%08X", nvse->nvseVersion, nvse->runtimeVersion);
    return true;
}

__declspec(dllexport) bool NVSEPlugin_Load(const NVSEInterface* nvse) {
    if (nvse->isEditor) return false;

    g_pluginHandle = nvse->GetPluginHandle();

    GetConsoleInterface() = (NVSEConsoleInterface*)nvse->QueryInterface(kInterface_Console);
    LogMessage("[NVMP] Console interface: %s", GetConsoleInterface() ? "OK" : "NOT FOUND");

    g_messaging = (NVSEMessagingInterface*)nvse->QueryInterface(kInterface_Messaging);
    if (g_messaging) {
        g_messaging->RegisterListener(g_pluginHandle, "NVSE", NVSEMessageHandler);
        LogMessage("[NVMP] Registered for MainGameLoop");
    }

    LoadConfig();

    LogMessage("[NVMP] Plugin loaded v2");
    return true;
}

} // extern "C"

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH)
        DisableThreadLibraryCalls(hModule);
    return TRUE;
}
