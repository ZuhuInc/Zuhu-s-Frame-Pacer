#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <iostream>
#include <string>
#include <vector>

DWORD FindProcessIdByName(const std::wstring& processName) {
    DWORD pid = 0;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return 0;

    PROCESSENTRY32W entry = {};
    entry.dwSize = sizeof(PROCESSENTRY32W);

    if (Process32FirstW(snapshot, &entry)) {
        do {
            if (_wcsicmp(entry.szExeFile, processName.c_str()) == 0) {
                pid = entry.th32ProcessID;
                break;
            }
        } while (Process32NextW(snapshot, &entry));
    }

    CloseHandle(snapshot);
    return pid;
}

bool InjectDLL(DWORD pid, const std::wstring& dllPath) {
    // Verify DLL exists on disk before attempting injection
    DWORD fileAttr = GetFileAttributesW(dllPath.c_str());
    if (fileAttr == INVALID_FILE_ATTRIBUTES || (fileAttr & FILE_ATTRIBUTE_DIRECTORY)) {
        std::wcerr << L"[Injector] ERROR: DLL file does not exist at path: " << dllPath << std::endl;
        return false;
    }

    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!hProcess) {
        std::wcerr << L"[Injector] ERROR: Failed to open target process PID " << pid << L" (Win32 Error: " << GetLastError() << L")" << std::endl;
        std::wcerr << L"[Injector] (Tip: Run terminal as Administrator if target process runs elevated)" << std::endl;
        return false;
    }

    size_t pathSize = (dllPath.length() + 1) * sizeof(wchar_t);
    LPVOID pRemoteBuf = VirtualAllocEx(hProcess, NULL, pathSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!pRemoteBuf) {
        std::wcerr << L"[Injector] ERROR: Failed to allocate memory in target process." << std::endl;
        CloseHandle(hProcess);
        return false;
    }

    if (!WriteProcessMemory(hProcess, pRemoteBuf, dllPath.c_str(), pathSize, NULL)) {
        std::wcerr << L"[Injector] ERROR: Failed to write memory in target process." << std::endl;
        VirtualFreeEx(hProcess, pRemoteBuf, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }

    HMODULE hKernel32 = GetModuleHandleW(L"kernel32.dll");
    LPVOID pLoadLibraryW = (LPVOID)GetProcAddress(hKernel32, "LoadLibraryW");

    HANDLE hThread = CreateRemoteThread(hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)pLoadLibraryW, pRemoteBuf, 0, NULL);
    if (!hThread) {
        std::wcerr << L"[Injector] ERROR: Failed to create remote thread." << std::endl;
        VirtualFreeEx(hProcess, pRemoteBuf, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }

    WaitForSingleObject(hThread, 5000);

    DWORD remoteExitCode = 0;
    GetExitCodeThread(hThread, &remoteExitCode);

    CloseHandle(hThread);
    VirtualFreeEx(hProcess, pRemoteBuf, 0, MEM_RELEASE);
    CloseHandle(hProcess);

    if (remoteExitCode == 0) {
        std::wcerr << L"[Injector] ERROR: LoadLibraryW returned NULL in remote process. Failed to load DLL." << std::endl;
        return false;
    }

    std::wcout << L"[Injector] SUCCESS: Injected " << dllPath << L" into PID " << pid << L" (Module Handle: 0x" << std::hex << remoteExitCode << L")" << std::dec << std::endl;
    return true;
}

int wmain(int argc, wchar_t** argv) {
    std::wcout << L"================================================================" << std::endl;
    std::wcout << L"            FRAMEPACER - UNIVERSAL DLL INJECTOR                 " << std::endl;
    std::wcout << L"================================================================" << std::endl;

    if (argc < 2) {
        std::wcout << L"Usage: FramePacerInjector.exe <process_name.exe | PID> [dll_path]" << std::endl;
        std::wcout << L"Example: FramePacerInjector.exe test_dx11_app.exe" << std::endl;
        return 1;
    }

    std::wstring target = argv[1];
    wchar_t fullDllPath[MAX_PATH] = {};

    if (argc >= 3) {
        GetFullPathNameW(argv[2], MAX_PATH, fullDllPath, NULL);
    } else {
        // Auto-search bin\FramePacerHook64.dll first, then FramePacerHook64.dll
        if (GetFileAttributesW(L"bin\\FramePacerHook64.dll") != INVALID_FILE_ATTRIBUTES) {
            GetFullPathNameW(L"bin\\FramePacerHook64.dll", MAX_PATH, fullDllPath, NULL);
        } else if (GetFileAttributesW(L"FramePacerHook64.dll") != INVALID_FILE_ATTRIBUTES) {
            GetFullPathNameW(L"FramePacerHook64.dll", MAX_PATH, fullDllPath, NULL);
        } else {
            // Check in same directory as injector exe
            wchar_t exePath[MAX_PATH];
            GetModuleFileNameW(NULL, exePath, MAX_PATH);
            wchar_t* lastSlash = wcsrchr(exePath, L'\\');
            if (lastSlash) {
                *(lastSlash + 1) = L'\0';
                wcscat_s(exePath, L"FramePacerHook64.dll");
                if (GetFileAttributesW(exePath) != INVALID_FILE_ATTRIBUTES) {
                    wcscpy_s(fullDllPath, exePath);
                }
            }
            if (fullDllPath[0] == L'\0') {
                GetFullPathNameW(L"bin\\FramePacerHook64.dll", MAX_PATH, fullDllPath, NULL);
            }
        }
    }

    DWORD pid = 0;
    try {
        pid = std::stoul(target);
    } catch (...) {
        pid = FindProcessIdByName(target);
    }

    if (!pid) {
        std::wcerr << L"[Injector] ERROR: Could not find running process: " << target << std::endl;
        return 1;
    }

    std::wcout << L"[Injector] Target: " << target << L" (PID: " << pid << L")" << std::endl;
    std::wcout << L"[Injector] DLL:    " << fullDllPath << std::endl;

    if (InjectDLL(pid, fullDllPath)) {
        std::wcout << L"[Injector] Pacer is now active in target process!" << std::endl;
        return 0;
    } else {
        std::wcerr << L"[Injector] Injection failed." << std::endl;
        return 1;
    }
}
