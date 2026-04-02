#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <cstdio>
#include <cstring>

// Simple DLL injector for nvmp_storyserver.exe
// Usage: nvmp_inject.exe [path_to_dll]

static DWORD FindProcess(const char* name) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return 0;

    PROCESSENTRY32 pe = {sizeof(pe)};
    if (Process32First(snap, &pe)) {
        do {
            if (_stricmp(pe.szExeFile, name) == 0) {
                CloseHandle(snap);
                return pe.th32ProcessID;
            }
        } while (Process32Next(snap, &pe));
    }
    CloseHandle(snap);
    return 0;
}

static bool InjectDLL(DWORD pid, const char* dllPath) {
    HANDLE proc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!proc) {
        printf("Failed to open process %u (error %u). Run as admin?\n", pid, GetLastError());
        return false;
    }

    size_t pathLen = strlen(dllPath) + 1;
    void* remoteMem = VirtualAllocEx(proc, nullptr, pathLen, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remoteMem) {
        printf("VirtualAllocEx failed (error %u)\n", GetLastError());
        CloseHandle(proc);
        return false;
    }

    if (!WriteProcessMemory(proc, remoteMem, dllPath, pathLen, nullptr)) {
        printf("WriteProcessMemory failed (error %u)\n", GetLastError());
        VirtualFreeEx(proc, remoteMem, 0, MEM_RELEASE);
        CloseHandle(proc);
        return false;
    }

    HMODULE k32 = GetModuleHandleA("kernel32.dll");
    FARPROC loadLib = GetProcAddress(k32, "LoadLibraryA");

    HANDLE thread = CreateRemoteThread(proc, nullptr, 0, (LPTHREAD_START_ROUTINE)loadLib, remoteMem, 0, nullptr);
    if (!thread) {
        printf("CreateRemoteThread failed (error %u)\n", GetLastError());
        VirtualFreeEx(proc, remoteMem, 0, MEM_RELEASE);
        CloseHandle(proc);
        return false;
    }

    WaitForSingleObject(thread, 5000);
    CloseHandle(thread);
    VirtualFreeEx(proc, remoteMem, 0, MEM_RELEASE);
    CloseHandle(proc);
    return true;
}

int main(int argc, char* argv[]) {
    printf("NV:MP Server Hook Injector\n\n");

    // Find the server process
    DWORD pid = FindProcess("nvmp_storyserver.exe");
    if (!pid) {
        printf("nvmp_storyserver.exe not found! Start the server first.\n");
        return 1;
    }
    printf("Found nvmp_storyserver.exe (PID %u)\n", pid);

    // Get DLL path
    char dllPath[MAX_PATH];
    if (argc > 1) {
        GetFullPathNameA(argv[1], MAX_PATH, dllPath, nullptr);
    } else {
        // Default: look for nvmp_server_hook.dll next to this exe
        GetModuleFileNameA(nullptr, dllPath, MAX_PATH);
        char* lastSlash = strrchr(dllPath, '\\');
        if (lastSlash) {
            strcpy(lastSlash + 1, "nvmp_server_hook.dll");
        }
    }

    printf("Injecting: %s\n", dllPath);

    if (InjectDLL(pid, dllPath)) {
        printf("SUCCESS! Hook DLL injected.\n");
        printf("\nCheck nvmp_server_hook.log for details.\n");
        return 0;
    } else {
        printf("FAILED to inject DLL.\n");
        return 1;
    }
}
