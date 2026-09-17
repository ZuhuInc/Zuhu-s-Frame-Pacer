#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <cstdint>
#include "ipc_shared_memory.h"
#include "timing_engine.h"

namespace FramePacer {

#ifndef VKAPI_PTR
#define VKAPI_PTR __stdcall
#endif
#ifndef VKAPI_CALL
#define VKAPI_CALL __stdcall
#endif
#ifndef VKAPI_ATTR
#define VKAPI_ATTR
#endif

// Minimal Khronos Vulkan type definitions for presentation hook
typedef uint32_t VkFlags;
typedef uint64_t VkDeviceSize;
typedef void* VkHandle;

typedef struct VkQueue_T* VkQueue;
typedef struct VkDevice_T* VkDevice;
typedef struct VkInstance_T* VkInstance;
typedef struct VkSwapchainKHR_T* VkSwapchainKHR;
typedef struct VkSemaphore_T* VkSemaphore;

typedef enum VkResult {
    VK_SUCCESS = 0,
    VK_NOT_READY = 1,
    VK_TIMEOUT = 2,
    VK_EVENT_SET = 3,
    VK_EVENT_RESET = 4,
    VK_INCOMPLETE = 5,
    VK_ERROR_OUT_OF_HOST_MEMORY = -1,
    VK_ERROR_OUT_OF_DEVICE_MEMORY = -2,
    VK_ERROR_INITIALIZATION_FAILED = -3,
    VK_ERROR_DEVICE_LOST = -4,
    VK_ERROR_OUT_OF_DATE_KHR = -1000001004,
    VK_SUBOPTIMAL_KHR = 1000001003
} VkResult;

typedef enum VkStructureType {
    VK_STRUCTURE_TYPE_PRESENT_INFO_KHR = 1000001001
} VkStructureType;

typedef struct VkPresentInfoKHR {
    VkStructureType          sType;
    const void*              pNext;
    uint32_t                 waitSemaphoreCount;
    const VkSemaphore*       pWaitSemaphores;
    uint32_t                 swapchainCount;
    const VkSwapchainKHR*    pSwapchains;
    const uint32_t*          pImageIndices;
    VkResult*                pResults;
} VkPresentInfoKHR;

typedef void (*PFN_vkVoidFunction)(void);
typedef PFN_vkVoidFunction (*PFN_vkGetInstanceProcAddr)(VkInstance instance, const char* pName);
typedef PFN_vkVoidFunction (*PFN_vkGetDeviceProcAddr)(VkDevice device, const char* pName);
typedef VkResult (VKAPI_PTR *PFN_vkQueuePresentKHR)(VkQueue queue, const VkPresentInfoKHR* pPresentInfo);

/**
 * @brief VulkanHookEngine manages presentation-edge pacing for Vulkan games
 */
class VulkanHookEngine {
public:
    static VulkanHookEngine& Instance();

    bool Initialize();
    void Shutdown();

    VkResult OnQueuePresentKHR(VkQueue queue, const VkPresentInfoKHR* pPresentInfo);
    
    void SetOriginalQueuePresentKHR(PFN_vkQueuePresentKHR fn) {
        if (fn) {
            m_fnOriginalQueuePresent = fn;
        }
    }

    PFN_vkQueuePresentKHR GetOriginalQueuePresentKHR() const {
        return m_fnOriginalQueuePresent;
    }

    void HookLoadedModules();

private:
    VulkanHookEngine();
    ~VulkanHookEngine();

    void UpdateTelemetry(double achievedFrametimeMs);

    PFN_vkQueuePresentKHR m_fnOriginalQueuePresent;
    bool m_isInitialized;
    
    SharedMemoryChannel m_ipc;
    HighPrecisionPacer m_pacer;

    uint32_t m_processId;
    char m_processName[128];
    double m_lastTargetFps;

    float m_recentFrames[IPC_HISTORY_SIZE];
    uint32_t m_recentFrameIndex;
};

} // namespace FramePacer

