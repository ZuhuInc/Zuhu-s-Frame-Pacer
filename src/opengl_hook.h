#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <cstdint>
#include "ipc_shared_memory.h"
#include "timing_engine.h"

namespace FramePacer {

typedef BOOL (WINAPI *PFN_wglSwapBuffers)(HDC hdc);
typedef HGLRC (WINAPI *PFN_wglGetCurrentContext)(void);

/**
 * @brief OpenGLHookEngine intercepts wglSwapBuffers presentation edge
 * for OpenGL titles, emulators, and classic 3D engines.
 */
class OpenGLHookEngine {
public:
    static OpenGLHookEngine& Instance();

    bool Initialize();
    void Shutdown();

    BOOL OnWglSwapBuffers(HDC hdc);

    void SetOriginalWglSwapBuffers(PFN_wglSwapBuffers fn) { m_fnOriginalWglSwapBuffers = fn; }

private:
    OpenGLHookEngine();
    ~OpenGLHookEngine();

    BOOL CallOriginalWglSwapBuffers(HDC hdc);
    void UpdateTelemetry(double achievedFrametimeMs);

    PFN_wglSwapBuffers m_fnOriginalWglSwapBuffers;
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
