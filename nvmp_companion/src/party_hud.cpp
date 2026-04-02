#include "pch.h"
#include "nvmp_state.h"
#include "nvse_lite.h"

namespace nvmp {

static bool s_overlayHooked = false;

// Global text buffer prepended with " | " for clean appearance on Connected line
static char s_overlayText[128] = " | MP";

// Patch point: VA 0x1009AA3C (RVA 0x9AA3C)
// Original: 8D 85 2C FE FF FF 50 (lea eax,[ebp-0x1D4]; push eax)
// Patched:  B8 [addr] 50 90      (mov eax,&s_overlayText; push eax; nop)
static constexpr uintptr_t PATCH_RVA = 0x9AA3C;
static uint8_t s_origBytes[7] = {0};

static bool InstallOverlayHook(uintptr_t clientBase) {
    uintptr_t patchAddr = clientBase + PATCH_RVA;

    // Verify original bytes
    uint8_t expected[] = {0x8D, 0x85, 0x2C, 0xFE, 0xFF, 0xFF, 0x50};
    if (memcmp((void*)patchAddr, expected, 7) != 0) {
        LogMessage("[Overlay] Byte mismatch at 0x%08X: %02X %02X %02X %02X %02X %02X %02X",
                   patchAddr,
                   ((uint8_t*)patchAddr)[0], ((uint8_t*)patchAddr)[1],
                   ((uint8_t*)patchAddr)[2], ((uint8_t*)patchAddr)[3],
                   ((uint8_t*)patchAddr)[4], ((uint8_t*)patchAddr)[5],
                   ((uint8_t*)patchAddr)[6]);
        return false;
    }
    memcpy(s_origBytes, (void*)patchAddr, 7);

    // Build patch: mov eax, &s_overlayText; push eax; nop
    uint8_t patch[7];
    patch[0] = 0xB8; // mov eax, imm32
    *(uintptr_t*)&patch[1] = (uintptr_t)s_overlayText;
    patch[5] = 0x50; // push eax
    patch[6] = 0x90; // nop

    DWORD oldProt;
    VirtualProtect((void*)patchAddr, 7, PAGE_EXECUTE_READWRITE, &oldProt);
    memcpy((void*)patchAddr, patch, 7);
    VirtualProtect((void*)patchAddr, 7, oldProt, &oldProt);
    FlushInstructionCache(GetCurrentProcess(), (void*)patchAddr, 7);

    LogMessage("[Overlay] Patched sprintf arg at 0x%08X -> text at 0x%08X (\"%s\")",
               patchAddr, (uintptr_t)s_overlayText, s_overlayText);
    return true;
}

void InjectHUD() {
    if (s_overlayHooked) return;
    HMODULE clientDll = GetModuleHandleA("client.dll");
    if (!clientDll) clientDll = GetModuleHandleA("nvmp\\client.dll");
    if (!clientDll) return;
    if (InstallOverlayHook((uintptr_t)clientDll)) {
        s_overlayHooked = true;
    }
}

void UpdateModeIndicator(int mode) {
    const char* labels[] = {"MP", "HYBRID", "SP"};
    if (mode < 0 || mode > 2) mode = 0;
    snprintf(s_overlayText, sizeof(s_overlayText), " | %s", labels[mode]);
}

void SetOverlayText(const char* playerName, int mode) {
    const char* labels[] = {"MP", "HYBRID", "SP"};
    if (mode < 0 || mode > 2) mode = 0;
    snprintf(s_overlayText, sizeof(s_overlayText), " | %s | %s", playerName, labels[mode]);
}

void ToggleHUD() {}
void UpdateHUD(NVMPState&) {}

} // namespace nvmp
