#include "vulkan_hook.h"
#include <MinHook.h>
#include <psapi.h>
#include <iostream>
#include <cmath>
#include <numeric>
#include <vector>
#include <thread>
#include <atomic>
#include <algorithm>

#pragma comment(lib, "psapi.lib")

namespace FramePacer {

static PFN_vkGetInstanceProcAddr s_realVkGetInstanceProcAddr = nullptr;
static PFN_vkGetDeviceProcAddr s_realVkGetDeviceProcAddr = nullptr;
static PFN_vkQueuePresentKHR s_realVkQueuePresentKHR = nullptr;
static decltype(&GetProcAddress) s_realGetProcAddress = nullptr;
static std::atomic<bool> s_hookedVulkan(false);

static VkResult VKAPI_CALL Hooked_vkQueuePresentKHR(VkQueue queue, const VkPresentInfoKHR* pPresentInfo);
static PFN_vkVoidFunction VKAPI_CALL Hooked_vkGetDeviceProcAddr(VkDevice device, const char* pName);
static PFN_vkVoidFunction VKAPI_CALL Hooked_vkGetInstanceProcAddr(VkInstance instance, const char* pName);
static FARPROC WINAPI Hooked_GetProcAddress(HMODULE hModule, LPCSTR lpProcName);

static PFN_vkVoidFunction VKAPI_CALL Hooked_vkGetDeviceProcAddr(VkDevice device, const char* pName) {
    if (pName && std::strcmp(pName, "vkQueuePresentKHR") == 0) {
        if (s_realVkGetDeviceProcAddr) {
            PFN_vkQueuePresentKHR realPresent = reinterpret_cast<PFN_vkQueuePresentKHR>(
                s_realVkGetDeviceProcAddr(device, pName)
            );
            if (realPresent) {
                s_realVkQueuePresentKHR = realPresent;
                VulkanHookEngine::Instance().SetOriginalQueuePresentKHR(realPresent);
            }
        }
        return reinterpret_cast<PFN_vkVoidFunction>(Hooked_vkQueuePresentKHR);
    }
    if (s_realVkGetDeviceProcAddr) {
        return s_realVkGetDeviceProcAddr(device, pName);
    }
    return nullptr;
}

static PFN_vkVoidFunction VKAPI_CALL Hooked_vkGetInstanceProcAddr(VkInstance instance, const char* pName) {
    if (pName) {
        if (std::strcmp(pName, "vkQueuePresentKHR") == 0) {
            if (s_realVkGetInstanceProcAddr) {
                PFN_vkQueuePresentKHR realPresent = reinterpret_cast<PFN_vkQueuePresentKHR>(
                    s_realVkGetInstanceProcAddr(instance, pName)
                );
                if (realPresent) {
                    s_realVkQueuePresentKHR = realPresent;
                    VulkanHookEngine::Instance().SetOriginalQueuePresentKHR(realPresent);
                }
            }
            return reinterpret_cast<PFN_vkVoidFunction>(Hooked_vkQueuePresentKHR);
        }
        if (std::strcmp(pName, "vkGetDeviceProcAddr") == 0) {
            return reinterpret_cast<PFN_vkVoidFunction>(Hooked_vkGetDeviceProcAddr);
        }
    }
    if (s_realVkGetInstanceProcAddr) {
        return s_realVkGetInstanceProcAddr(instance, pName);
    }
    return nullptr;
}

static FARPROC WINAPI Hooked_GetProcAddress(HMODULE hModule, LPCSTR lpProcName) {
    if (!lpProcName || (reinterpret_cast<ULONG_PTR>(lpProcName) <= 0xFFFF)) {
        if (s_realGetProcAddress) return s_realGetProcAddress(hModule, lpProcName);
        return GetProcAddress(hModule, lpProcName);
    }

    if (std::strcmp(lpProcName, "vkGetInstanceProcAddr") == 0) {
        if (!s_realVkGetInstanceProcAddr) {
            FARPROC realProc = s_realGetProcAddress ? s_realGetProcAddress(hModule, lpProcName) : GetProcAddress(hModule, lpProcName);
            if (realProc) s_realVkGetInstanceProcAddr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(realProc);
        }
        return reinterpret_cast<FARPROC>(Hooked_vkGetInstanceProcAddr);
    }

    if (std::strcmp(lpProcName, "vkGetDeviceProcAddr") == 0) {
        if (!s_realVkGetDeviceProcAddr) {
            FARPROC realProc = s_realGetProcAddress ? s_realGetProcAddress(hModule, lpProcName) : GetProcAddress(hModule, lpProcName);
            if (realProc) s_realVkGetDeviceProcAddr = reinterpret_cast<PFN_vkGetDeviceProcAddr>(realProc);
        }
        return reinterpret_cast<FARPROC>(Hooked_vkGetDeviceProcAddr);
    }

    if (std::strcmp(lpProcName, "vkQueuePresentKHR") == 0) {
        FARPROC realProc = s_realGetProcAddress ? s_realGetProcAddress(hModule, lpProcName) : GetProcAddress(hModule, lpProcName);
        if (realProc) {
            s_realVkQueuePresentKHR = reinterpret_cast<PFN_vkQueuePresentKHR>(realProc);
            VulkanHookEngine::Instance().SetOriginalQueuePresentKHR(s_realVkQueuePresentKHR);
        }
        return reinterpret_cast<FARPROC>(Hooked_vkQueuePresentKHR);
    }

    if (s_realGetProcAddress) {
        return s_realGetProcAddress(hModule, lpProcName);
    }
    return GetProcAddress(hModule, lpProcName);
}

static bool HookIATInModule(HMODULE hMod) {
    if (!hMod) return false;

    PIMAGE_DOS_HEADER pDosHeader = reinterpret_cast<PIMAGE_DOS_HEADER>(hMod);
    if (pDosHeader->e_magic != IMAGE_DOS_SIGNATURE) return false;

    PIMAGE_NT_HEADERS pNtHeaders = reinterpret_cast<PIMAGE_NT_HEADERS>(reinterpret_cast<BYTE*>(hMod) + pDosHeader->e_lfanew);
    if (pNtHeaders->Signature != IMAGE_NT_SIGNATURE) return false;

    IMAGE_DATA_DIRECTORY importDir = pNtHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (importDir.VirtualAddress == 0 || importDir.Size == 0) return false;

    PIMAGE_IMPORT_DESCRIPTOR pImportDesc = reinterpret_cast<PIMAGE_IMPORT_DESCRIPTOR>(reinterpret_cast<BYTE*>(hMod) + importDir.VirtualAddress);

    while (pImportDesc->Name != 0) {
        const char* szModName = reinterpret_cast<const char*>(reinterpret_cast<BYTE*>(hMod) + pImportDesc->Name);
        
        bool isVulkan = (_stricmp(szModName, "vulkan-1.dll") == 0);
        bool isKernel32 = (_stricmp(szModName, "kernel32.dll") == 0 || 
                           _strnicmp(szModName, "api-ms-win-core-libraryloader", 28) == 0);

        if (isVulkan || isKernel32) {
            DWORD thunkRva = pImportDesc->OriginalFirstThunk ? pImportDesc->OriginalFirstThunk : pImportDesc->FirstThunk;
            PIMAGE_THUNK_DATA pOriginalThunk = reinterpret_cast<PIMAGE_THUNK_DATA>(reinterpret_cast<BYTE*>(hMod) + thunkRva);
            PIMAGE_THUNK_DATA pFirstThunk = reinterpret_cast<PIMAGE_THUNK_DATA>(reinterpret_cast<BYTE*>(hMod) + pImportDesc->FirstThunk);

            while (pOriginalThunk->u1.AddressOfData != 0) {
                if (!(pOriginalThunk->u1.Ordinal & IMAGE_ORDINAL_FLAG)) {
                    PIMAGE_IMPORT_BY_NAME pImportByName = reinterpret_cast<PIMAGE_IMPORT_BY_NAME>(reinterpret_cast<BYTE*>(hMod) + pOriginalThunk->u1.AddressOfData);
                    const char* funcName = reinterpret_cast<const char*>(pImportByName->Name);

                    void* hookFunc = nullptr;

                    if (isVulkan) {
                        if (std::strcmp(funcName, "vkGetInstanceProcAddr") == 0) {
                            if (!s_realVkGetInstanceProcAddr && pFirstThunk->u1.Function != reinterpret_cast<ULONG_PTR>(Hooked_vkGetInstanceProcAddr)) {
                                s_realVkGetInstanceProcAddr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(pFirstThunk->u1.Function);
                            }
                            hookFunc = reinterpret_cast<void*>(Hooked_vkGetInstanceProcAddr);
                        } else if (std::strcmp(funcName, "vkGetDeviceProcAddr") == 0) {
                            if (!s_realVkGetDeviceProcAddr && pFirstThunk->u1.Function != reinterpret_cast<ULONG_PTR>(Hooked_vkGetDeviceProcAddr)) {
                                s_realVkGetDeviceProcAddr = reinterpret_cast<PFN_vkGetDeviceProcAddr>(pFirstThunk->u1.Function);
                            }
                            hookFunc = reinterpret_cast<void*>(Hooked_vkGetDeviceProcAddr);
                        } else if (std::strcmp(funcName, "vkQueuePresentKHR") == 0) {
                            if (!s_realVkQueuePresentKHR && pFirstThunk->u1.Function != reinterpret_cast<ULONG_PTR>(Hooked_vkQueuePresentKHR)) {
                                s_realVkQueuePresentKHR = reinterpret_cast<PFN_vkQueuePresentKHR>(pFirstThunk->u1.Function);
                                VulkanHookEngine::Instance().SetOriginalQueuePresentKHR(s_realVkQueuePresentKHR);
                            }
                            hookFunc = reinterpret_cast<void*>(Hooked_vkQueuePresentKHR);
                        }
                    } else if (isKernel32) {
                        if (std::strcmp(funcName, "GetProcAddress") == 0) {
                            if (!s_realGetProcAddress && pFirstThunk->u1.Function != reinterpret_cast<ULONG_PTR>(Hooked_GetProcAddress)) {
                                s_realGetProcAddress = reinterpret_cast<decltype(&GetProcAddress)>(pFirstThunk->u1.Function);
                            }
                            hookFunc = reinterpret_cast<void*>(Hooked_GetProcAddress);
                        }
                    }

                    if (hookFunc && pFirstThunk->u1.Function != reinterpret_cast<ULONG_PTR>(hookFunc)) {
                        DWORD oldProtect = 0;
                        if (VirtualProtect(&pFirstThunk->u1.Function, sizeof(ULONG_PTR), PAGE_READWRITE, &oldProtect)) {
                            pFirstThunk->u1.Function = reinterpret_cast<ULONG_PTR>(hookFunc);
                            VirtualProtect(&pFirstThunk->u1.Function, sizeof(ULONG_PTR), oldProtect, &oldProtect);
                        }
                    }
                }
                pOriginalThunk++;
                pFirstThunk++;
            }
        }
        pImportDesc++;
    }
    return true;
}

VulkanHookEngine& VulkanHookEngine::Instance() {
    static VulkanHookEngine s_instance;
    return s_instance;
}

VulkanHookEngine::VulkanHookEngine()
    : m_fnOriginalQueuePresent(nullptr)
    , m_isInitialized(false)
    , m_processId(0)
    , m_lastTargetFps(60.0)
    , m_recentFrameIndex(0)
{
    std::memset(m_processName, 0, sizeof(m_processName));
    std::memset(m_recentFrames, 0, sizeof(m_recentFrames));
}

VulkanHookEngine::~VulkanHookEngine() {
    Shutdown();
}

void VulkanHookEngine::HookLoadedModules() {
    HMODULE hMods[1024];
    DWORD cbNeeded = 0;
    HANDLE hProcess = GetCurrentProcess();
    
    if (EnumProcessModules(hProcess, hMods, sizeof(hMods), &cbNeeded)) {
        DWORD count = cbNeeded / sizeof(HMODULE);
        for (DWORD i = 0; i < count; ++i) {
            HookIATInModule(hMods[i]);
        }
    } else {
        HookIATInModule(GetModuleHandleA(NULL));
    }
}

static DWORD WINAPI VulkanHookScannerThread(LPVOID lpParam) {
    MH_Initialize();

    for (int i = 0; i < 240; ++i) {
        HMODULE hVulkan = GetModuleHandleW(L"vulkan-1.dll");
        if (hVulkan && !s_hookedVulkan) {
            void* pPres = reinterpret_cast<void*>(GetProcAddress(hVulkan, "vkQueuePresentKHR"));
            void* pGIPA = reinterpret_cast<void*>(GetProcAddress(hVulkan, "vkGetInstanceProcAddr"));
            void* pGDPA = reinterpret_cast<void*>(GetProcAddress(hVulkan, "vkGetDeviceProcAddr"));

            if (pPres) {
                if (MH_CreateHook(pPres, reinterpret_cast<void*>(&Hooked_vkQueuePresentKHR), reinterpret_cast<void**>(&s_realVkQueuePresentKHR)) == MH_OK) {
                    MH_EnableHook(pPres);
                    VulkanHookEngine::Instance().SetOriginalQueuePresentKHR(s_realVkQueuePresentKHR);
                }
            }
            if (pGIPA) {
                if (MH_CreateHook(pGIPA, reinterpret_cast<void*>(&Hooked_vkGetInstanceProcAddr), reinterpret_cast<void**>(&s_realVkGetInstanceProcAddr)) == MH_OK) {
                    MH_EnableHook(pGIPA);
                }
            }
            if (pGDPA) {
                if (MH_CreateHook(pGDPA, reinterpret_cast<void*>(&Hooked_vkGetDeviceProcAddr), reinterpret_cast<void**>(&s_realVkGetDeviceProcAddr)) == MH_OK) {
                    MH_EnableHook(pGDPA);
                }
            }
            s_hookedVulkan = true;
        }

        VulkanHookEngine::Instance().HookLoadedModules();
        Sleep(250);
    }
    return 0;
}

static bool IsBlockedProcessName(const char* procName) {
    if (!procName) return true;
    char lower[128] = {};
    for (size_t i = 0; procName[i] && i < 127; ++i) {
        lower[i] = static_cast<char>(tolower(static_cast<unsigned char>(procName[i])));
    }
    if (strstr(lower, "nvidia") || strstr(lower, "overlay") || strstr(lower, "discord") ||
        strstr(lower, "electron") || strstr(lower, "obs") || strstr(lower, "rtss") ||
        strstr(lower, "steam") || strstr(lower, "chrome") || strstr(lower, "framepacer") ||
        strstr(lower, "gamebar") || strstr(lower, "radeon") || strstr(lower, "overwolf")) {
        return true;
    }
    return false;
}

bool VulkanHookEngine::Initialize() {
    if (m_isInitialized) return true;

    // 1. Query process details
    m_processId = GetCurrentProcessId();
    GetModuleFileNameA(NULL, m_processName, sizeof(m_processName) - 1);
    char* pLastSlash = std::strrchr(m_processName, '\\');
    if (pLastSlash) {
        std::memmove(m_processName, pLastSlash + 1, std::strlen(pLastSlash + 1) + 1);
    }

    if (IsBlockedProcessName(m_processName)) {
        return false;
    }

    // 2. Initialize Shared Memory IPC
    m_ipc.OpenOrCreate(false);

    // 3. Initialize High Precision Pacer
    m_pacer.Initialize();
    m_pacer.SetTargetFps(60.0);

    // 4. Hook initially loaded modules and resolve vulkan-1.dll
    HMODULE hVulkan = GetModuleHandleW(L"vulkan-1.dll");
    if (hVulkan) {
        s_realVkGetInstanceProcAddr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(GetProcAddress(hVulkan, "vkGetInstanceProcAddr"));
        s_realVkGetDeviceProcAddr = reinterpret_cast<PFN_vkGetDeviceProcAddr>(GetProcAddress(hVulkan, "vkGetDeviceProcAddr"));
        s_realVkQueuePresentKHR = reinterpret_cast<PFN_vkQueuePresentKHR>(GetProcAddress(hVulkan, "vkQueuePresentKHR"));
        if (s_realVkQueuePresentKHR) {
            SetOriginalQueuePresentKHR(s_realVkQueuePresentKHR);
        }
    }

    HookLoadedModules();

    // 5. Spawn background hook thread to intercept modules loaded dynamically
    CreateThread(NULL, 0, VulkanHookScannerThread, NULL, 0, NULL);

    m_isInitialized = true;
    return true;
}

void VulkanHookEngine::Shutdown() {
    m_isInitialized = false;
}

void VulkanHookEngine::UpdateTelemetry(double achievedFrametimeMs) {
    auto* pIpc = m_ipc.Get();
    if (!pIpc) return;

    m_recentFrames[m_recentFrameIndex % IPC_HISTORY_SIZE] = static_cast<float>(achievedFrametimeMs);
    m_recentFrameIndex++;

    uint32_t count = (std::min)(m_recentFrameIndex, IPC_HISTORY_SIZE);
    double sumMs = 0.0;
    for (uint32_t i = 0; i < count; ++i) {
        sumMs += m_recentFrames[i];
    }
    double avgMs = sumMs / count;
    double currentFps = (avgMs > 0.001) ? (1000.0 / avgMs) : 0.0;

    // Compute standard deviation (Jitter)
    double varianceSum = 0.0;
    for (uint32_t i = 0; i < count; ++i) {
        double diff = m_recentFrames[i] - avgMs;
        varianceSum += diff * diff;
    }
    double variance = varianceSum / count;
    double stdDevMs = std::sqrt(variance);
    double jitterUs = stdDevMs * 1000.0;

    // 1% Low FPS computation
    std::vector<float> sortedFrames(m_recentFrames, m_recentFrames + count);
    std::sort(sortedFrames.begin(), sortedFrames.end(), std::greater<float>());
    size_t idx1Percent = static_cast<size_t>(count * 0.01);
    float low1PercentMs = sortedFrames[idx1Percent];
    float low1PercentFps = (low1PercentMs > 0.001f) ? (1000.0f / low1PercentMs) : 0.0f;

    // Write to Shared Memory
    pIpc->process_id = m_processId;
    std::strncpy(pIpc->process_name, m_processName, sizeof(pIpc->process_name) - 1);
    std::strncpy(pIpc->api_name, "Vulkan (VK_KHR_swapchain)", sizeof(pIpc->api_name) - 1);
    
    pIpc->current_fps = static_cast<float>(currentFps);
    pIpc->current_frametime_ms = static_cast<float>(achievedFrametimeMs);
    pIpc->jitter_us = static_cast<float>(jitterUs);
    pIpc->low_1_percent_fps = low1PercentFps;
    pIpc->frame_index = m_recentFrameIndex;

    // Copy ring buffer for oscilloscope
    for (uint32_t i = 0; i < IPC_HISTORY_SIZE; ++i) {
        uint32_t srcIdx = (m_recentFrameIndex + i) % IPC_HISTORY_SIZE;
        pIpc->frametime_history[i] = m_recentFrames[srcIdx];
    }
}

static VkResult VKAPI_CALL Hooked_vkQueuePresentKHR(VkQueue queue, const VkPresentInfoKHR* pPresentInfo) {
    return VulkanHookEngine::Instance().OnQueuePresentKHR(queue, pPresentInfo);
}

VkResult VulkanHookEngine::OnQueuePresentKHR(VkQueue queue, const VkPresentInfoKHR* pPresentInfo) {
    // Thread-local recursion guard
    thread_local bool s_inVkPresent = false;
    if (s_inVkPresent) {
        if (m_fnOriginalQueuePresent) {
            return m_fnOriginalQueuePresent(queue, pPresentInfo);
        }
        if (s_realVkQueuePresentKHR) {
            return s_realVkQueuePresentKHR(queue, pPresentInfo);
        }
        return VK_SUCCESS;
    }

    s_inVkPresent = true;

    // Sync target FPS from UI / IPC
    auto* pIpc = m_ipc.Get();
    if (pIpc) {
        uint32_t uiTargetFps = pIpc->target_fps;
        if (uiTargetFps >= 10 && uiTargetFps <= 360 && std::abs(m_lastTargetFps - uiTargetFps) > 0.1) {
            m_lastTargetFps = uiTargetFps;
            m_pacer.SetTargetFps(m_lastTargetFps);
        }
    }

    bool pacingEnabled = pIpc ? (pIpc->pacing_enabled != 0) : true;
    double achievedFrametimeMs = 16.67;

    if (pacingEnabled) {
        achievedFrametimeMs = m_pacer.WaitAndPace();
    } else {
        LARGE_INTEGER qpcNow;
        QueryPerformanceCounter(&qpcNow);
        m_pacer.Reset();
    }

    UpdateTelemetry(achievedFrametimeMs);

    VkResult res = VK_SUCCESS;
    if (m_fnOriginalQueuePresent) {
        res = m_fnOriginalQueuePresent(queue, pPresentInfo);
    } else if (s_realVkQueuePresentKHR) {
        res = s_realVkQueuePresentKHR(queue, pPresentInfo);
    }

    s_inVkPresent = false;
    return res;
}

} // namespace FramePacer


