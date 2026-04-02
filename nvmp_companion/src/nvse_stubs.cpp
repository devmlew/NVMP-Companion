#include "pch.h"

// ============================================================
// Stubs for xNVSE functions that we don't link against
// (We use xNVSE headers but don't link the full NVSE library)
// ============================================================

// IErrors.h stubs
void _AssertionFailed(const char* file, unsigned long line, const char* desc) {
    char buf[512];
    snprintf(buf, sizeof(buf), "Assertion failed: %s (%s:%lu)", desc, file, line);
    MessageBoxA(nullptr, buf, "NVMP Companion Error", MB_OK | MB_ICONERROR);
    TerminateProcess(GetCurrentProcess(), 1);
}

void _AssertionFailed_ErrCode(const char* file, unsigned long line, const char* desc, unsigned long long code) {
    char buf[512];
    snprintf(buf, sizeof(buf), "Assertion failed: %s (code: %llu) (%s:%lu)", desc, code, file, line);
    MessageBoxA(nullptr, buf, "NVMP Companion Error", MB_OK | MB_ICONERROR);
    TerminateProcess(GetCurrentProcess(), 1);
}

void _AssertionFailed_ErrCode(const char* file, unsigned long line, const char* desc, const char* code) {
    char buf[512];
    snprintf(buf, sizeof(buf), "Assertion failed: %s (code: %s) (%s:%lu)", desc, code, file, line);
    MessageBoxA(nullptr, buf, "NVMP Companion Error", MB_OK | MB_ICONERROR);
    TerminateProcess(GetCurrentProcess(), 1);
}

// IDebugLog stubs
#include "common/IDebugLog.h"
IDebugLog gLog;

void IDebugLog::Open(const char*) {}
void IDebugLog::OpenRelative(int, const char*) {}
void IDebugLog::SetAutoFlush(bool) {}
void IDebugLog::SetTitle(const char*) {}
void IDebugLog::SetPrintLevel(LogLevel) {}
void IDebugLog::SetLogLevel(LogLevel) {}
void IDebugLog::Message(const char*, ...) {}
void IDebugLog::FormattedMessage(const char*, ...) {}
void IDebugLog::FormattedMessage(const char*, va_list) {}
void IDebugLog::Log(LogLevel, const char*, va_list) {}

// GameAPI stubs - these are normally resolved from FalloutNV.exe at runtime
// Console_Print is what we need for in-game messages
// In a real NVSE plugin, these are resolved via the game's export table
// We'll resolve them at runtime instead

static bool s_consolePrintResolved = false;
typedef void (*_Console_Print)(const char* fmt, ...);
static _Console_Print s_Console_Print = nullptr;

// The actual Console_Print function in FalloutNV.exe
// Address: 0x0071D0A0 (FNV 1.4.0.525)
void Console_Print(const char* fmt, ...) {
    if (!s_consolePrintResolved) {
        // Resolve at runtime from FalloutNV.exe
        HMODULE fnv = GetModuleHandleA(nullptr); // main exe
        if (fnv) {
            // Console_Print is at a known offset in FNV
            s_Console_Print = (_Console_Print)((uintptr_t)fnv + 0x0031D0A0);
        }
        s_consolePrintResolved = true;
    }

    if (s_Console_Print) {
        va_list args;
        va_start(args, fmt);
        char buf[512];
        vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);
        s_Console_Print("%s", buf);
    }
}

// InterfaceManager stubs
InterfaceManager* InterfaceManager::GetSingleton() {
    // Known address in FNV: 0x011D8A80 contains the singleton pointer
    uintptr_t* ptr = reinterpret_cast<uintptr_t*>(0x011D8A80);
    __try {
        return reinterpret_cast<InterfaceManager*>(*ptr);
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        return nullptr;
    }
}

Menu* InterfaceManager::GetMenuByType(UInt32 menuType) {
    // Use TempMenuByType function
    // Address in FNV: 0x00A1DEA0
    typedef Menu* (*_GetMenu)(UInt32);
    static _GetMenu fn = (_GetMenu)0x00A1DEA0;
    __try {
        return fn(menuType);
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        return nullptr;
    }
}

Tile::Value* InterfaceManager::GetMenuComponentValue(const char* componentPath) {
    // Resolve at known FNV address: 0x00A01B20
    typedef Tile::Value* (*_GetVal)(const char*);
    static _GetVal fn = (_GetVal)0x00A01B20;
    __try {
        return fn(componentPath);
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        return nullptr;
    }
}

Tile* InterfaceManager::GetMenuComponentTile(const char* componentPath) {
    Tile::Value* val = GetMenuComponentValue(componentPath);
    if (val) return val->IsParentTile() ? val->IsParentTile() : nullptr;
    return nullptr;
}

// TempMenuByType extern
const _TempMenuByType TempMenuByType = (_TempMenuByType)0x00A1DEA0;
