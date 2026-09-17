#include "dxgi_hook.h"
#include "overlay_renderer.h"
#include <iostream>
#include <cmath>
#include <numeric>
#include <vector>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "user32.lib")

namespace FramePacer {

// Static Trampolines for C-style VTable redirection
static HRESULT STDMETHODCALLTYPE HookedPresent(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags) {
    return DXGIHookEngine::Instance().OnPresent(pSwapChain, SyncInterval, Flags);
}

static HRESULT STDMETHODCALLTYPE HookedPresent1(IDXGISwapChain1* pSwapChain, UINT SyncInterval, UINT Flags, const DXGI_PRESENT_PARAMETERS* pPresentParameters) {
    return DXGIHookEngine::Instance().OnPresent1(pSwapChain, SyncInterval, Flags, pPresentParameters);
}

DXGIHookEngine& DXGIHookEngine::Instance() {
    static DXGIHookEngine s_instance;
    return s_instance;
}

DXGIHookEngine::DXGIHookEngine()
    : m_fnOriginalPresent(nullptr)
    , m_fnOriginalPresent1(nullptr)
    , m_pSwapChainVTable(nullptr)
    , m_isInitialized(false)
    , m_latencyClamped(false)
    , m_processId(0)
    , m_lastTargetFps(60.0)
    , m_recentFrameIndex(0)
{
    std::memset(m_processName, 0, sizeof(m_processName));
    std::memset(m_recentFrames, 0, sizeof(m_recentFrames));
}

DXGIHookEngine::~DXGIHookEngine() {
    Shutdown();
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

bool DXGIHookEngine::Initialize() {
    if (m_isInitialized) return true;

    // 1. Query current process information
    m_processId = GetCurrentProcessId();
    GetModuleFileNameA(NULL, m_processName, sizeof(m_processName) - 1);
    
    // Extract base name
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

    // 4. Create dummy window & swapchain to acquire DXGI VTable
    WNDCLASSEXA wc = {};
    wc.cbSize = sizeof(WNDCLASSEXA);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = "FramePacerDummyWindow";
    RegisterClassExA(&wc);

    HWND hWnd = CreateWindowExA(
        0,
        wc.lpszClassName,
        "FramePacerDummy",
        WS_OVERLAPPEDWINDOW,
        0, 0, 100, 100,
        NULL, NULL, wc.hInstance, NULL
    );

    if (!hWnd) {
        UnregisterClassA(wc.lpszClassName, wc.hInstance);
        return false;
    }

    DXGI_SWAP_CHAIN_DESC scd = {};
    scd.BufferCount = 1;
    scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    scd.BufferDesc.Width = 100;
    scd.BufferDesc.Height = 100;
    scd.BufferDesc.RefreshRate.Numerator = 60;
    scd.BufferDesc.RefreshRate.Denominator = 1;
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.OutputWindow = hWnd;
    scd.SampleDesc.Count = 1;
    scd.Windowed = TRUE;

    ID3D11Device* pDevice = nullptr;
    ID3D11DeviceContext* pContext = nullptr;
    IDXGISwapChain* pSwapChain = nullptr;
    D3D_FEATURE_LEVEL featureLevel;

    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        0,
        nullptr,
        0,
        D3D11_SDK_VERSION,
        &scd,
        &pSwapChain,
        &pDevice,
        &featureLevel,
        &pContext
    );

    // Fallback to WARP software renderer if hardware device unavailable in sandbox
    if (FAILED(hr)) {
        hr = D3D11CreateDeviceAndSwapChain(
            nullptr,
            D3D_DRIVER_TYPE_WARP,
            nullptr,
            0,
            nullptr,
            0,
            D3D11_SDK_VERSION,
            &scd,
            &pSwapChain,
            &pDevice,
            &featureLevel,
            &pContext
        );
    }

    if (SUCCEEDED(hr) && pSwapChain) {
        void** pVTable = *reinterpret_cast<void***>(pSwapChain);
        m_pSwapChainVTable = pVTable;

        // VTable Index 8: IDXGISwapChain::Present
        HookVTableMethod(pVTable, 8, reinterpret_cast<void*>(HookedPresent), reinterpret_cast<void**>(&m_fnOriginalPresent));

        // Try querying IDXGISwapChain1 for Present1 (Index 22)
        IDXGISwapChain1* pSwapChain1 = nullptr;
        if (SUCCEEDED(pSwapChain->QueryInterface(__uuidof(IDXGISwapChain1), reinterpret_cast<void**>(&pSwapChain1)))) {
            void** pVTable1 = *reinterpret_cast<void***>(pSwapChain1);
            HookVTableMethod(pVTable1, 22, reinterpret_cast<void*>(HookedPresent1), reinterpret_cast<void**>(&m_fnOriginalPresent1));
            pSwapChain1->Release();
        }

        pSwapChain->Release();
        pDevice->Release();
        pContext->Release();
    }

    DestroyWindow(hWnd);
    UnregisterClassA(wc.lpszClassName, wc.hInstance);

    m_isInitialized = (m_fnOriginalPresent != nullptr);
    return m_isInitialized;
}

bool DXGIHookEngine::HookVTableMethod(void** ppVTable, int index, void* pNewFunction, void** ppOldFunction) {
    if (!ppVTable || !pNewFunction) return false;

    DWORD oldProtect;
    if (VirtualProtect(&ppVTable[index], sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProtect)) {
        if (ppOldFunction && !*ppOldFunction) {
            *ppOldFunction = ppVTable[index];
        }
        ppVTable[index] = pNewFunction;
        VirtualProtect(&ppVTable[index], sizeof(void*), oldProtect, &oldProtect);
        return true;
    }
    return false;
}

void DXGIHookEngine::ApplyFrameLatencyClamp(IDXGISwapChain* pSwapChain) {
    if (m_latencyClamped || !pSwapChain) return;

    ID3D11Device* pDevice = nullptr;
    if (SUCCEEDED(pSwapChain->GetDevice(__uuidof(ID3D11Device), reinterpret_cast<void**>(&pDevice)))) {
        IDXGIDevice1* pDXGIDevice = nullptr;
        if (SUCCEEDED(pDevice->QueryInterface(__uuidof(IDXGIDevice1), reinterpret_cast<void**>(&pDXGIDevice)))) {
            pDXGIDevice->SetMaximumFrameLatency(1);
            pDXGIDevice->Release();
            m_latencyClamped = true;
        }
        pDevice->Release();
    }

    // Direct DXGI 1.3 Latency Clamping
    IDXGISwapChain2* pSwapChain2 = nullptr;
    if (SUCCEEDED(pSwapChain->QueryInterface(__uuidof(IDXGISwapChain2), reinterpret_cast<void**>(&pSwapChain2)))) {
        pSwapChain2->SetMaximumFrameLatency(1);
        pSwapChain2->Release();
        m_latencyClamped = true;
    }
}

void DXGIHookEngine::UpdateTelemetry(double frameTimeMs) {
    m_recentFrames[m_recentFrameIndex % IPC_HISTORY_SIZE] = static_cast<float>(frameTimeMs);
    m_recentFrameIndex++;

    FramePacerControlBlock* pIpc = m_ipc.Get();
    if (!pIpc) return;

    // Check if target FPS was changed in UI
    if (pIpc->target_fps > 0 && pIpc->target_fps != m_lastTargetFps) {
        m_lastTargetFps = pIpc->target_fps;
        m_pacer.SetTargetFps(m_lastTargetFps);
    }

    // Write live game telemetry
    pIpc->process_id = m_processId;
    std::strncpy(pIpc->process_name, m_processName, sizeof(pIpc->process_name) - 1);
    std::strncpy(pIpc->api_name, "DirectX 11/12 (DXGI)", sizeof(pIpc->api_name) - 1);
    
    pIpc->current_frametime_ms = static_cast<float>(frameTimeMs);
    pIpc->current_fps = (frameTimeMs > 0.1) ? static_cast<float>(1000.0 / frameTimeMs) : 0.0f;
    pIpc->frame_index = m_recentFrameIndex;

    // Compute standard deviation (jitter)
    uint32_t count = (m_recentFrameIndex < IPC_HISTORY_SIZE) ? m_recentFrameIndex : IPC_HISTORY_SIZE;
    if (count > 0) {
        float sum = 0.0f;
        for (uint32_t i = 0; i < count; ++i) sum += m_recentFrames[i];
        float mean = sum / count;

        float varSum = 0.0f;
        for (uint32_t i = 0; i < count; ++i) {
            varSum += (m_recentFrames[i] - mean) * (m_recentFrames[i] - mean);
        }
        float stdDevMs = std::sqrt(varSum / count);
        pIpc->jitter_us = stdDevMs * 1000.0f;
    }
}

HRESULT DXGIHookEngine::OnPresent(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags) {
    thread_local bool s_inPresent = false;
    if (s_inPresent) {
        if (m_fnOriginalPresent) return m_fnOriginalPresent(pSwapChain, SyncInterval, Flags);
        return S_OK;
    }
    s_inPresent = true;

    if (!m_latencyClamped) {
        ApplyFrameLatencyClamp(pSwapChain);
    }

    FramePacerControlBlock* pIpc = m_ipc.Get();
    bool pacingActive = pIpc ? (pIpc->pacing_enabled != 0) : true;

    double frameTimeMs = 0.0;
    if (pacingActive) {
        frameTimeMs = m_pacer.WaitAndPace();
    } else {
        frameTimeMs = m_pacer.MeasureUnpacedFrame();
    }

    UpdateTelemetry(frameTimeMs);

    HRESULT hr = S_OK;
    if (m_fnOriginalPresent) {
        UINT effectiveSyncInterval = pacingActive ? 0 : SyncInterval;
        hr = m_fnOriginalPresent(pSwapChain, effectiveSyncInterval, Flags);
    }

    s_inPresent = false;
    return hr;
}

HRESULT DXGIHookEngine::OnPresent1(IDXGISwapChain1* pSwapChain, UINT SyncInterval, UINT Flags, const DXGI_PRESENT_PARAMETERS* pPresentParameters) {
    thread_local bool s_inPresent1 = false;
    if (s_inPresent1) {
        if (m_fnOriginalPresent1) return m_fnOriginalPresent1(pSwapChain, SyncInterval, Flags, pPresentParameters);
        return S_OK;
    }
    s_inPresent1 = true;

    if (!m_latencyClamped) {
        ApplyFrameLatencyClamp(pSwapChain);
    }

    FramePacerControlBlock* pIpc = m_ipc.Get();
    bool pacingActive = pIpc ? (pIpc->pacing_enabled != 0) : true;

    double frameTimeMs = 0.0;
    if (pacingActive) {
        frameTimeMs = m_pacer.WaitAndPace();
    } else {
        frameTimeMs = m_pacer.MeasureUnpacedFrame();
    }

    UpdateTelemetry(frameTimeMs);

    HRESULT hr = S_OK;
    if (m_fnOriginalPresent1) {
        UINT effectiveSyncInterval = pacingActive ? 0 : SyncInterval;
        hr = m_fnOriginalPresent1(pSwapChain, effectiveSyncInterval, Flags, pPresentParameters);
    }

    s_inPresent1 = false;
    return hr;
}

void DXGIHookEngine::Shutdown() {
    m_ipc.Close();
    m_isInitialized = false;
}

} // namespace FramePacer

extern "C" __declspec(dllexport) bool FramePacer_Attach(IDXGISwapChain* pSwapChain) {
    if (!pSwapChain) return false;
    void** pVTable = *reinterpret_cast<void***>(pSwapChain);
    if (!pVTable) return false;

    FramePacer::DXGIHookEngine::Instance().Initialize();
    return FramePacer::DXGIHookEngine::Instance().HookVTableMethod(pVTable, 8, reinterpret_cast<void*>(FramePacer::HookedPresent), reinterpret_cast<void**>(&FramePacer::DXGIHookEngine::Instance().m_fnOriginalPresent));
}

extern "C" __declspec(dllexport) void FramePacer_SetFps(double fps) {
    FramePacer::SharedMemoryChannel ipc;
    if (ipc.OpenOrCreate(false)) {
        if (ipc.Get()) {
            ipc.Get()->target_fps = static_cast<uint32_t>(fps);
        }
    }
}
