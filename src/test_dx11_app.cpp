#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <iostream>
#include <chrono>
#include <string>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")

// Global Direct3D 11 Variables
IDXGISwapChain*         g_pSwapChain = nullptr;
ID3D11Device*           g_pd3dDevice = nullptr;
ID3D11DeviceContext*    g_pd3dDeviceContext = nullptr;
ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;

void CreateRenderTarget() {
    ID3D11Texture2D* pBackBuffer = nullptr;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    if (pBackBuffer) {
        g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
        pBackBuffer->Release();
    }
}

void CleanupRenderTarget() {
    if (g_mainRenderTargetView) {
        g_mainRenderTargetView->Release();
        g_mainRenderTargetView = nullptr;
    }
}

bool CreateDeviceD3D(HWND hWnd) {
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
    
    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        createDeviceFlags,
        featureLevelArray,
        2,
        D3D11_SDK_VERSION,
        &sd,
        &g_pSwapChain,
        &g_pd3dDevice,
        &featureLevel,
        &g_pd3dDeviceContext
    );

    if (FAILED(hr)) {
        hr = D3D11CreateDeviceAndSwapChain(
            nullptr,
            D3D_DRIVER_TYPE_WARP,
            nullptr,
            createDeviceFlags,
            featureLevelArray,
            2,
            D3D11_SDK_VERSION,
            &sd,
            &g_pSwapChain,
            &g_pd3dDevice,
            &featureLevel,
            &g_pd3dDeviceContext
        );
    }

    if (FAILED(hr)) return false;

    CreateRenderTarget();
    return true;
}

void CleanupDeviceD3D() {
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
}

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_SIZE:
        if (g_pd3dDevice != nullptr && wParam != SIZE_MINIMIZED) {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
            CreateRenderTarget();
        }
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

int main(int argc, char** argv) {
    // Optionally load FramePacer hook DLL directly if passed via args
    bool autoHook = false;
    if (argc > 1 && std::string(argv[1]) == "--hook") {
        HMODULE hHook = LoadLibraryA("FramePacerHook64.dll");
        if (hHook) {
            std::cout << "[TestApp] FramePacerHook64.dll attached successfully!" << std::endl;
            autoHook = true;
        } else {
            std::cout << "[TestApp] Failed to load FramePacerHook64.dll" << std::endl;
        }
    }

    WNDCLASSEXA wc = { sizeof(WNDCLASSEXA), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(NULL), NULL, NULL, NULL, NULL, "DX11TestAppClass", NULL };
    RegisterClassExA(&wc);

    HWND hWnd = CreateWindowA(
        wc.lpszClassName,
        "FramePacer DirectX 11 Test Game Harness",
        WS_OVERLAPPEDWINDOW,
        100, 100, 800, 600,
        NULL, NULL, wc.hInstance, NULL
    );

    if (!CreateDeviceD3D(hWnd)) {
        CleanupDeviceD3D();
        UnregisterClassA(wc.lpszClassName, wc.hInstance);
        std::cerr << "Failed to initialize Direct3D 11." << std::endl;
        return 1;
    }

    if (autoHook) {
        typedef bool (*FramePacer_Attach_t)(IDXGISwapChain*);
        HMODULE hHook = GetModuleHandleA("FramePacerHook64.dll");
        if (!hHook) hHook = LoadLibraryA("FramePacerHook64.dll");
        if (hHook) {
            auto fnAttach = (FramePacer_Attach_t)GetProcAddress(hHook, "FramePacer_Attach");
            if (fnAttach && fnAttach(g_pSwapChain)) {
                std::cout << "[TestApp] FramePacer Presentation Hook attached directly to swapchain!" << std::endl;
            }
        }
    }

    ShowWindow(hWnd, SW_SHOWDEFAULT);
    UpdateWindow(hWnd);

    std::cout << "=================================================================" << std::endl;
    std::cout << "    DIRECTX 11 TEST HARNESS RUNNING (Press ESC or Close to Exit) " << std::endl;
    std::cout << "    Pacer Attached: " << (autoHook ? "YES [FramePacer Active]" : "NO [Uncapped]") << std::endl;
    std::cout << "=================================================================" << std::endl;

    MSG msg = {};
    float hue = 0.0f;
    uint32_t frameCount = 0;
    auto lastFpsTime = std::chrono::high_resolution_clock::now();

    while (msg.message != WM_QUIT) {
        if (PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            continue;
        }

        // Animate smooth color pulsing in 3D viewport
        hue += 0.005f;
        if (hue > 1.0f) hue -= 1.0f;

        float clearColor[4] = {
            0.1f + 0.1f * sinf(hue * 6.28318f),
            0.15f + 0.15f * sinf(hue * 6.28318f + 2.094f),
            0.3f + 0.2f * sinf(hue * 6.28318f + 4.188f),
            1.0f
        };

        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clearColor);

        // Present with 0 sync interval (uncapped present, paced by our hook)
        g_pSwapChain->Present(0, 0);

        frameCount++;
        auto now = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed = now - lastFpsTime;
        if (elapsed.count() >= 1.0) {
            double currentFps = frameCount / elapsed.count();
            double avgFrametimeMs = 1000.0 / currentFps;
            std::cout << "\r[DX11 Render Loop] FPS: " << std::fixed << currentFps 
                      << " | Frametime: " << avgFrametimeMs << " ms     " << std::flush;
            frameCount = 0;
            lastFpsTime = now;
        }
    }

    CleanupDeviceD3D();
    DestroyWindow(hWnd);
    UnregisterClass(wc.lpszClassName, wc.hInstance);

    return 0;
}
