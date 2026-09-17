#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_4.h>
#include "timing_engine.h"
#include "ipc_shared_memory.h"

namespace FramePacer {

typedef HRESULT (WINAPI *D3D11CreateDeviceAndSwapChain_t)(
    IDXGIAdapter*,
    D3D_DRIVER_TYPE,
    HMODULE,
    UINT,
    const D3D_FEATURE_LEVEL*,
    UINT,
    UINT,
    const DXGI_SWAP_CHAIN_DESC*,
    IDXGISwapChain**,
    ID3D11Device**,
    D3D_FEATURE_LEVEL*,
    ID3D11DeviceContext**
);

typedef HRESULT (STDMETHODCALLTYPE *DXGIPresent_t)(
    IDXGISwapChain* pSwapChain,
    UINT SyncInterval,
    UINT Flags
);

typedef HRESULT (STDMETHODCALLTYPE *DXGIPresent1_t)(
    IDXGISwapChain1* pSwapChain,
    UINT SyncInterval,
    UINT Flags,
    const DXGI_PRESENT_PARAMETERS* pPresentParameters
);

class DXGIHookEngine {
public:
    static DXGIHookEngine& Instance();

    bool Initialize();
    void Shutdown();

    // Hook callbacks
    HRESULT OnPresent(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags);
    HRESULT OnPresent1(IDXGISwapChain1* pSwapChain, UINT SyncInterval, UINT Flags, const DXGI_PRESENT_PARAMETERS* pPresentParameters);

    bool HookVTableMethod(void** ppVTable, int index, void* pNewFunction, void** ppOldFunction);

    DXGIPresent_t GetOriginalPresent() const { return m_fnOriginalPresent; }
    DXGIPresent1_t GetOriginalPresent1() const { return m_fnOriginalPresent1; }

    DXGIPresent_t m_fnOriginalPresent;
    DXGIPresent1_t m_fnOriginalPresent1;

private:
    DXGIHookEngine();
    ~DXGIHookEngine();

    void ApplyFrameLatencyClamp(IDXGISwapChain* pSwapChain);
    void UpdateTelemetry(double frameTimeMs);

    void** m_pSwapChainVTable;

    HighPrecisionPacer m_pacer;
    SharedMemoryChannel m_ipc;

    bool m_isInitialized;
    bool m_latencyClamped;
    char m_processName[128];
    uint32_t m_processId;
    double m_lastTargetFps;

    // Telemetry stats
    float m_recentFrames[IPC_HISTORY_SIZE];
    uint32_t m_recentFrameIndex;
};

} // namespace FramePacer
