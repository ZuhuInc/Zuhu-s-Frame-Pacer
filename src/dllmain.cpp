#include <windows.h>
#include "dxgi_hook.h"
#include "vulkan_hook.h"

DWORD WINAPI HookInitThread(LPVOID lpParam) {
    // Safety delay to allow game's DirectX 11/12 graphics engine and command queues to stabilize
    Sleep(600);

    __try {
        // Initialize DirectX (DX11/DX12) presentation hooks
        FramePacer::DXGIHookEngine::Instance().Initialize();

        // Only initialize Vulkan hooks if vulkan-1.dll is loaded in the process
        if (GetModuleHandleW(L"vulkan-1.dll") != NULL) {
            FramePacer::VulkanHookEngine::Instance().Initialize();
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        // Safely catch any initialization anomalies in protected games
    }

    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule);
        CreateThread(NULL, 0, HookInitThread, NULL, 0, NULL);
        break;
    case DLL_PROCESS_DETACH:
        __try {
            FramePacer::DXGIHookEngine::Instance().Shutdown();
            FramePacer::VulkanHookEngine::Instance().Shutdown();
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
        }
        break;
    }
    return TRUE;
}


