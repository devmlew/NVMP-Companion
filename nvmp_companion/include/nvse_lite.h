#pragma once
#include "pch.h"

// ============================================================
// Minimal NVSE Plugin API
// ============================================================

typedef UInt32 PluginHandle;
enum { kPluginHandle_Invalid = 0xFFFFFFFF };

struct PluginInfo {
    enum { kInfoVersion = 1 };
    UInt32 infoVersion;
    const char* name;
    UInt32 version;
};

struct CommandInfo;

struct NVSEInterface {
    UInt32 nvseVersion;
    UInt32 runtimeVersion;
    UInt32 editorVersion;
    UInt32 isEditor;
    bool   (*RegisterCommand)(CommandInfo* info);
    void   (*SetOpcodeBase)(UInt32 opcode);
    void*  (*QueryInterface)(UInt32 id);
    PluginHandle (*GetPluginHandle)(void);
    bool   (*RegisterTypedCommand)(CommandInfo* info, UInt32 retnType);
    const char* (*GetRuntimeDirectory)();
    UInt32 isNogore;
};

enum {
    kInterface_Serialization = 0,
    kInterface_Console,
    kInterface_Messaging,
};

struct TESObjectREFR;

// Console interface — printing AND script execution
struct NVSEConsoleInterface {
    UInt32 version;
    bool (*RunScriptLine)(const char* buf, TESObjectREFR* object);
    bool (*RunScriptLine2)(const char* buf, TESObjectREFR* callingRefr, bool bSuppressConsoleOutput);
};

struct NVSEMessagingInterface {
    enum {
        kMessage_PostLoad = 0, kMessage_ExitGame, kMessage_ExitToMainMenu,
        kMessage_LoadGame, kMessage_SaveGame, kMessage_Precompile,
        kMessage_PreLoadGame, kMessage_ExitGame_Console, kMessage_PostLoadGame,
        kMessage_PostPostLoad, kMessage_RuntimeScriptError,
        kMessage_DeleteGame, kMessage_RenameGame, kMessage_RenameNewGame,
        kMessage_NewGame, kMessage_DeleteGameName, kMessage_RenameGameName,
        kMessage_RenameNewGameName, kMessage_DeferredInit,
        kMessage_ClearScriptDataCache, kMessage_MainGameLoop,
        kMessage_ScriptCompile, kMessage_EventListDestroyed,
        kMessage_PostQueryPlugins,
    };
    struct Message { const char* sender; UInt32 type; UInt32 dataLen; void* data; };
    typedef void (*EventCallback)(Message* msg);
    UInt32 version;
    bool (*RegisterListener)(PluginHandle handle, const char* sender, EventCallback callback);
    bool (*Dispatch)(PluginHandle handle, UInt32 msgID, void* data, UInt32 dataLen, const char* receiver);
};

// ============================================================
// Logging — file + NVSE console
// ============================================================

inline FILE* GetLogFile() {
    static FILE* s_log = nullptr;
    if (!s_log) {
        s_log = fopen("nvmp_companion.log", "w");
    }
    return s_log;
}

// Global interface pointers — set during NVSEPlugin_Load
inline NVSEConsoleInterface*& GetConsoleInterface() {
    static NVSEConsoleInterface* s_console = nullptr;
    return s_console;
}


inline void LogMessage(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char buf[512];
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    FILE* f = GetLogFile();
    if (f) { fprintf(f, "%s\n", buf); fflush(f); }

    OutputDebugStringA(buf);
    OutputDebugStringA("\n");
}

inline void ConsolePrint(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char buf[512];
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    // Log to file
    LogMessage("%s", buf);
}

// Run a script/console command via NVSE console interface
inline bool RunScript(const char* cmd) {
    NVSEConsoleInterface* con = GetConsoleInterface();
    if (con && con->RunScriptLine) {
        return con->RunScriptLine(cmd, nullptr);
    }
    return false;
}
