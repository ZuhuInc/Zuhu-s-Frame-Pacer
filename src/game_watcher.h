#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <iostream>
#include <string>
#include <unordered_set>
#include <vector>
#include <algorithm>
#include <thread>
#include <atomic>
#include <chrono>

namespace FramePacer {

// Known system, desktop, browser, and anti-cheat processes to never inject
static const std::unordered_set<std::wstring> SYSTEM_BLOCKLIST = {
    L"explorer.exe",
    L"dwm.exe",
    L"taskmgr.exe",
    L"cmd.exe",
    L"powershell.exe",
    L"conhost.exe",
    L"svchost.exe",
    L"csrss.exe",
    L"lsass.exe",
    L"services.exe",
    L"smss.exe",
    L"wininit.exe",
    L"winlogon.exe",
    L"runtimebroker.exe",
    L"shellexperiencehost.exe",
    L"searchhost.exe",
    L"startmenuexperiencehost.exe",
    L"textinputhost.exe",
    L"applicationframehost.exe",
    L"systemsettings.exe",
    L"steam.exe",
    L"steamwebhelper.exe",
    L"epicgameslauncher.exe",
    L"origin.exe",
    L"eadesktop.exe",
    L"galaxyclient.exe",
    L"battle.net.exe",
    L"discord.exe",
    L"discordcanary.exe",
    L"discorddevelopment.exe",
    L"discordptb.exe",
    L"chrome.exe",
    L"msedge.exe",
    L"firefox.exe",
    L"brave.exe",
    L"code.exe",
    L"devenv.exe",
    L"antigravity.exe",
    L"electron.exe",
    L"spotify.exe",
    L"framepacerbridge.exe",
    L"framepacerbridge_v10.exe",
    L"framepaceroverlay.exe",
    L"framepaceroverlay_v2.exe",
    L"framepaceroverlay_v3.exe",
    L"framepacerinjector.exe",
    L"benchmark_timing.exe",
    // GPU and 3rd party overlay utilities to ignore
    L"nvidia overlay.exe",
    L"nvidia share.exe",
    L"nvcontainer.exe",
    L"nvdisplay.container.exe",
    L"nvspcaps64.exe",
    L"nvcplui.exe",
    L"radeonsoftware.exe",
    L"amddvr.exe",
    L"radeonoverlay.exe",
    L"gamebar.exe",
    L"gamebarpresencewriter.exe",
    L"gamebarftserver.exe",
    L"rtss.exe",
    L"rtsshnd64.exe",
    L"obs64.exe",
    L"obs32.exe",
    L"overwolf.exe",
    L"overwolfbrowser.exe",
    // Kernel-level anti-cheat titles to isolate
    L"valorant.exe",
    L"vgc.exe",
    L"easyanticheat.exe",
    L"easyanticheat_eos.exe",
    L"battleye.exe",
    L"fortniteclient-win64-shipping.exe"
};

class GameWatcher {
public:
    static GameWatcher& Instance() {
        static GameWatcher s_instance;
        return s_instance;
    }

    void Start(const std::wstring& dllPath) {
        if (m_isRunning) return;
        m_dllPath = dllPath;
        m_isRunning = true;
        m_isEnabled = true;
        m_workerThread = std::thread(&GameWatcher::WatcherLoop, this);
    }

    void Stop() {
        m_isRunning = false;
        if (m_workerThread.joinable()) {
            m_workerThread.join();
        }
    }

    void SetEnabled(bool enabled) {
        m_isEnabled = enabled;
        std::wcout << L"[GameWatcher] Auto-attach is now: " << (enabled ? L"ENABLED" : L"PAUSED") << std::endl;
    }

    bool IsEnabled() const { return m_isEnabled; }

private:
    GameWatcher() : m_isRunning(false), m_isEnabled(true) {}
    ~GameWatcher() { Stop(); }

    bool IsProcessAlreadyInjected(DWORD pid) {
        return m_injectedPids.find(pid) != m_injectedPids.end();
    }

    bool InjectIntoPid(DWORD pid, const std::wstring& procName) {
        DWORD fileAttr = GetFileAttributesW(m_dllPath.c_str());
        if (fileAttr == INVALID_FILE_ATTRIBUTES || (fileAttr & FILE_ATTRIBUTE_DIRECTORY)) {
            std::wcerr << L"[GameWatcher] DLL not found: " << m_dllPath << std::endl;
            return false;
        }

        HANDLE hProcess = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ, FALSE, pid);
        if (!hProcess) {
            hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
        }
        if (!hProcess) return false;

        // Check if DLL is already loaded in target process modules
        HMODULE hMods[1024];
        DWORD cbNeeded;
        if (EnumProcessModules(hProcess, hMods, sizeof(hMods), &cbNeeded)) {
            for (unsigned int i = 0; i < (cbNeeded / sizeof(HMODULE)); i++) {
                wchar_t szModName[MAX_PATH];
                if (GetModuleBaseNameW(hProcess, hMods[i], szModName, sizeof(szModName) / sizeof(wchar_t))) {
                    if (wcsstr(szModName, L"FramePacerHook64") != nullptr) {
                        m_injectedPids.insert(pid);
                        CloseHandle(hProcess);
                        return true;
                    }
                }
            }
        }

        size_t pathSize = (m_dllPath.length() + 1) * sizeof(wchar_t);
        LPVOID pRemoteBuf = VirtualAllocEx(hProcess, NULL, pathSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (!pRemoteBuf) {
            CloseHandle(hProcess);
            return false;
        }

        if (!WriteProcessMemory(hProcess, pRemoteBuf, m_dllPath.c_str(), pathSize, NULL)) {
            VirtualFreeEx(hProcess, pRemoteBuf, 0, MEM_RELEASE);
            CloseHandle(hProcess);
            return false;
        }

        HMODULE hKernel32 = GetModuleHandleW(L"kernel32.dll");
        LPVOID pLoadLibraryW = (LPVOID)GetProcAddress(hKernel32, "LoadLibraryW");

        HANDLE hThread = CreateRemoteThread(hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)pLoadLibraryW, pRemoteBuf, 0, NULL);
        if (!hThread) {
            VirtualFreeEx(hProcess, pRemoteBuf, 0, MEM_RELEASE);
            CloseHandle(hProcess);
            return false;
        }

        WaitForSingleObject(hThread, 3000);
        DWORD exitCode = 0;
        GetExitCodeThread(hThread, &exitCode);

        CloseHandle(hThread);
        VirtualFreeEx(hProcess, pRemoteBuf, 0, MEM_RELEASE);
        CloseHandle(hProcess);

        if (exitCode != 0) {
            m_injectedPids.insert(pid);
            std::wcout << L"\n================================================================" << std::endl;
            std::wcout << L" [Auto-Attach] 🟢 Successfully hooked active game: " << procName << L" (PID: " << pid << L")" << std::endl;
            std::wcout << L"================================================================" << std::endl;
            return true;
        }
        return false;
    }

    void CleanupDeadPids() {
        for (auto it = m_injectedPids.begin(); it != m_injectedPids.end(); ) {
            HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, *it);
            if (hProc) {
                DWORD exitCode = 0;
                if (GetExitCodeProcess(hProc, &exitCode) && exitCode != STILL_ACTIVE) {
                    it = m_injectedPids.erase(it);
                } else {
                    ++it;
                }
                CloseHandle(hProc);
            } else {
                it = m_injectedPids.erase(it);
            }
        }
    }

    static BOOL CALLBACK EnumWindowsProc(HWND hWnd, LPARAM lParam) {
        if (!IsWindowVisible(hWnd)) return TRUE;

        LONG_PTR exStyle = GetWindowLongPtrW(hWnd, GWL_EXSTYLE);
        if (exStyle & WS_EX_TOOLWINDOW) return TRUE;
        if (exStyle & WS_EX_TRANSPARENT) return TRUE;

        RECT r = {};
        if (!GetWindowRect(hWnd, &r) || (r.right - r.left < 200) || (r.bottom - r.top < 200)) {
            return TRUE;
        }

        wchar_t className[256] = {};
        GetClassNameW(hWnd, className, 255);
        std::wstring cls(className);
        std::transform(cls.begin(), cls.end(), cls.begin(), ::towlower);
        if (cls.find(L"overlay") != std::wstring::npos || cls.find(L"nvidia") != std::wstring::npos || cls.find(L"cef-osc-widget") != std::wstring::npos) {
            return TRUE;
        }

        DWORD pid = 0;
        GetWindowThreadProcessId(hWnd, &pid);
        if (pid == 0 || pid == GetCurrentProcessId()) return TRUE;

        GameWatcher* self = reinterpret_cast<GameWatcher*>(lParam);
        if (self->IsProcessAlreadyInjected(pid)) return TRUE;

        HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        if (hProcess) {
            wchar_t fullPath[MAX_PATH] = {};
            DWORD size = MAX_PATH;
            if (QueryFullProcessImageNameW(hProcess, 0, fullPath, &size)) {
                std::wstring pathStr(fullPath);
                size_t lastSlash = pathStr.find_last_of(L"\\/");
                std::wstring exeName = (lastSlash != std::wstring::npos) ? pathStr.substr(lastSlash + 1) : pathStr;

                std::wstring lowerExe = exeName;
                std::transform(lowerExe.begin(), lowerExe.end(), lowerExe.begin(), ::towlower);

                if (SYSTEM_BLOCKLIST.find(lowerExe) == SYSTEM_BLOCKLIST.end()) {
                    self->InjectIntoPid(pid, exeName);
                }
            }
            CloseHandle(hProcess);
        }
        return TRUE;
    }

    void WatcherLoop() {
        while (m_isRunning) {
            CleanupDeadPids();
            if (m_isEnabled) {
                EnumWindows(EnumWindowsProc, reinterpret_cast<LPARAM>(this));
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
    }

    std::atomic<bool> m_isRunning;
    std::atomic<bool> m_isEnabled;
    std::wstring m_dllPath;
    std::unordered_set<DWORD> m_injectedPids;
    std::thread m_workerThread;
};

} // namespace FramePacer
