#include <windows.h>
#include "dxgi_hook.h"
#include "vulkan_hook.h"

DWORD WINAPI HookInitThread(LPVOID lpParam) {
    // Initialize DirectX (DX11/DX12) presentation hooks
    FramePacer::DXGIHookEngine::Instance().Initialize();
    // Initialize Vulkan (VK_KHR_swapchain) presentation hooks
    FramePacer::VulkanHookEngine::Instance().Initialize();
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule);
        CreateThread(NULL, 0, HookInitThread, NULL, 0, NULL);
        break;
    case DLL_PROCESS_DETACH:
        FramePacer::DXGIHookEngine::Instance().Shutdown();
        FramePacer::VulkanHookEngine::Instance().Shutdown();
        break;
    }
    return TRUE;
}


