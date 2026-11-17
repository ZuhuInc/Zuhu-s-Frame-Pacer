#include "opengl_hook.h"
#include "dxgi_hook.h"
#include "vulkan_hook.h"
#include <MinHook.h>
#include <iostream>
#include <cmath>
#include <vector>
#include <atomic>
#include <algorithm>
#include <functional>
#include <cctype>

namespace FramePacer {

static PFN_wglSwapBuffers s_realWglSwapBuffers = nullptr;
static PFN_wglGetCurrentContext s_fnWglGetCurrentContext = nullptr;
static std::atomic<bool> s_hookedOpenGL(false);
static std::atomic<bool> s_stopScanner(false);

static BOOL WINAPI Hooked_wglSwapBuffers(HDC hdc) {
    return OpenGLHookEngine::Instance().OnWglSwapBuffers(hdc);
}

static bool IsBlockedProcessName(const char* procName) {
    if (!procName) return true;
    char lower[128] = {};
    for (size_t i = 0; procName[i] && i < 127; ++i) {
        lower[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(procName[i])));
    }
    if (std::strstr(lower, "nvidia") || std::strstr(lower, "overlay") || std::strstr(lower, "discord") ||
        std::strstr(lower, "electron") || std::strstr(lower, "obs") || std::strstr(lower, "rtss") ||
        std::strstr(lower, "steam") || std::strstr(lower, "chrome") || std::strstr(lower, "framepacer") ||
        std::strstr(lower, "gamebar") || std::strstr(lower, "radeon") || std::strstr(lower, "overwolf") ||
        std::strstr(lower, "antigravity")) {
        return true;
    }
    return false;
}

static void TryHookOpenGLModule() {
    if (s_hookedOpenGL.load()) return;

    HMODULE hOpengl = GetModuleHandleW(L"opengl32.dll");
    if (!hOpengl) return;

    s_fnWglGetCurrentContext = reinterpret_cast<PFN_wglGetCurrentContext>(
        GetProcAddress(hOpengl, "wglGetCurrentContext")
    );

    MH_STATUS initStatus = MH_Initialize();
    if (initStatus != MH_OK && initStatus != MH_ERROR_ALREADY_INITIALIZED) {
        return;
    }

    void* pWglSwapBuffers = reinterpret_cast<void*>(GetProcAddress(hOpengl, "wglSwapBuffers"));
    if (pWglSwapBuffers) {
        MH_STATUS hookStatus = MH_CreateHook(
            pWglSwapBuffers,
            reinterpret_cast<void*>(&Hooked_wglSwapBuffers),
            reinterpret_cast<void**>(&s_realWglSwapBuffers)
        );
        if (hookStatus == MH_OK || hookStatus == MH_ERROR_ALREADY_CREATED) {
            if (MH_EnableHook(pWglSwapBuffers) == MH_OK) {
                OpenGLHookEngine::Instance().SetOriginalWglSwapBuffers(s_realWglSwapBuffers);
                s_hookedOpenGL.store(true);
            }
        }
    }
}

static DWORD WINAPI OpenGLHookScannerThread(LPVOID lpParam) {
    for (int i = 0; i < 60 && !s_stopScanner.load(); ++i) {
        if (GetModuleHandleW(L"opengl32.dll") && !s_hookedOpenGL.load()) {
            TryHookOpenGLModule();
            if (s_hookedOpenGL.load()) break;
        }
        Sleep(500);
    }
    return 0;
}

OpenGLHookEngine& OpenGLHookEngine::Instance() {
    static OpenGLHookEngine s_instance;
    return s_instance;
}

OpenGLHookEngine::OpenGLHookEngine()
    : m_fnOriginalWglSwapBuffers(nullptr)
    , m_isInitialized(false)
    , m_processId(0)
    , m_lastTargetFps(60.0)
    , m_recentFrameIndex(0)
{
    std::memset(m_processName, 0, sizeof(m_processName));
    std::memset(m_recentFrames, 0, sizeof(m_recentFrames));
}

OpenGLHookEngine::~OpenGLHookEngine() {
    Shutdown();
}

bool OpenGLHookEngine::Initialize() {
    if (m_isInitialized) return true;

    m_processId = GetCurrentProcessId();
    GetModuleFileNameA(NULL, m_processName, sizeof(m_processName) - 1);
    char* pLastSlash = std::strrchr(m_processName, '\\');
    if (pLastSlash) {
        std::memmove(m_processName, pLastSlash + 1, std::strlen(pLastSlash + 1) + 1);
    }

    if (IsBlockedProcessName(m_processName)) {
        return false;
    }

    m_ipc.OpenOrCreate(false);
    m_pacer.Initialize();
    m_pacer.SetTargetFps(60.0);

    s_stopScanner.store(false);
    if (GetModuleHandleW(L"opengl32.dll")) {
        TryHookOpenGLModule();
    } else {
        HANDLE hThread = CreateThread(NULL, 0, OpenGLHookScannerThread, NULL, 0, NULL);
        if (hThread) CloseHandle(hThread);
    }

    m_isInitialized = true;
    return true;
}

void OpenGLHookEngine::Shutdown() {
    s_stopScanner.store(true);
    m_ipc.Close();
    m_isInitialized = false;
}

BOOL OpenGLHookEngine::CallOriginalWglSwapBuffers(HDC hdc) {
    if (m_fnOriginalWglSwapBuffers) {
        return m_fnOriginalWglSwapBuffers(hdc);
    }
    if (s_realWglSwapBuffers) {
        return s_realWglSwapBuffers(hdc);
    }
    return TRUE;
}

void OpenGLHookEngine::UpdateTelemetry(double achievedFrametimeMs) {
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
    std::strncpy(pIpc->api_name, "OpenGL (wglSwapBuffers)", sizeof(pIpc->api_name) - 1);

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

BOOL OpenGLHookEngine::OnWglSwapBuffers(HDC hdc) {
    thread_local bool s_inWglPresent = false;
    if (s_inWglPresent) {
        return CallOriginalWglSwapBuffers(hdc);
    }

    // Cross-API Collision Guard: never double-pace or overwrite telemetry if DXGI or Vulkan already owns presentation
    if (DXGIHookEngine::Instance().HasPresentedFrames() || VulkanHookEngine::Instance().HasPresentedFrames()) {
        return CallOriginalWglSwapBuffers(hdc);
    }

    // Ignore non-GL context calls
    if (s_fnWglGetCurrentContext && s_fnWglGetCurrentContext() == NULL) {
        return CallOriginalWglSwapBuffers(hdc);
    }

    auto* pIpc = m_ipc.Get();
    if (pIpc && pIpc->hook_opengl == 0) {
        return CallOriginalWglSwapBuffers(hdc);
    }

    s_inWglPresent = true;

    if (pIpc) {
        uint32_t uiTargetFps = pIpc->target_fps;
        if (uiTargetFps >= 10 && uiTargetFps <= 360 && std::abs(m_lastTargetFps - uiTargetFps) > 0.1) {
            m_lastTargetFps = uiTargetFps;
            m_pacer.SetTargetFps(m_lastTargetFps);
            m_pacer.Reset();
        }
    }

    bool pacingEnabled = pIpc ? (pIpc->pacing_enabled != 0) : true;
    bool hybridTiming = pIpc ? (pIpc->hybrid_timing != 0) : true;
    double achievedFrametimeMs = pacingEnabled ? m_pacer.WaitAndPace(hybridTiming) : m_pacer.MeasureUnpacedFrame();

    UpdateTelemetry(achievedFrametimeMs);

    BOOL res = CallOriginalWglSwapBuffers(hdc);
    s_inWglPresent = false;
    return res;
}

} // namespace FramePacer
