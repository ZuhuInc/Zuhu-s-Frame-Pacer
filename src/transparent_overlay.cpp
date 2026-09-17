#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include "ipc_shared_memory.h"

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")

using namespace Gdiplus;

class TransparentHUDOverlay {
public:
    TransparentHUDOverlay() 
        : m_hWnd(NULL)
        , m_userVisible(true)
        , m_gdiplusToken(0)
        , m_overlayWidth(248)
        , m_overlayHeight(90)
    {
        m_history.resize(32, 16.67f);
    }

    ~TransparentHUDOverlay() {
        if (m_hWnd) DestroyWindow(m_hWnd);
        if (m_gdiplusToken) GdiplusShutdown(m_gdiplusToken);
    }

    bool Initialize(HINSTANCE hInstance) {
        GdiplusStartupInput gdiplusStartupInput;
        if (GdiplusStartup(&m_gdiplusToken, &gdiplusStartupInput, NULL) != Ok) {
            std::cerr << "[Overlay] Failed to initialize GDI+." << std::endl;
            return false;
        }

        WNDCLASSEXW wc = {};
        wc.cbSize = sizeof(WNDCLASSEXW);
        wc.lpfnWndProc = WndProc;
        wc.hInstance = hInstance;
        wc.lpszClassName = L"FramePacerTransparentHUD";
        RegisterClassExW(&wc);

        // Click-through, topmost, layered, tool window (hidden from alt-tab & taskbar)
        m_hWnd = CreateWindowExW(
            WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
            wc.lpszClassName,
            L"FramePacer Overlay",
            WS_POPUP,
            -2000, -2000, m_overlayWidth, m_overlayHeight, // start offscreen
            NULL, NULL, hInstance, this
        );

        if (!m_hWnd) {
            std::cerr << "[Overlay] Failed to create layered window." << std::endl;
            return false;
        }

        m_ipc.OpenOrCreate(false);

        // Register Global Hotkeys
        RegisterHotKey(m_hWnd, 1, 0, VK_F11);
        RegisterHotKey(m_hWnd, 3, MOD_CONTROL | MOD_SHIFT, 'O');
        RegisterHotKey(m_hWnd, 4, MOD_CONTROL | MOD_SHIFT, 'P');
        RegisterHotKey(m_hWnd, 5, MOD_CONTROL | MOD_SHIFT, VK_UP);
        RegisterHotKey(m_hWnd, 6, MOD_CONTROL | MOD_SHIFT, VK_DOWN);

        std::cout << "================================================================" << std::endl;
        std::cout << "        FRAMEPACER - SEAMLESS TRANSPARENT HUD OVERLAY           " << std::endl;
        std::cout << "================================================================" << std::endl;
        std::cout << "[Overlay] Initialized successfully." << std::endl;
        std::cout << "[Overlay] Connected to Shared Memory IPC." << std::endl;
        std::cout << "[Overlay] Hotkeys: [F11] / [Ctrl+Shift+O]           : Toggle HUD" << std::endl;
        std::cout << "[Overlay] Hotkeys: [Ctrl+Shift+P]                    : Toggle Pacing" << std::endl;
        std::cout << "[Overlay] Hotkeys: [Ctrl+Shift+Up/Down]             : Adjust Target FPS" << std::endl;
        std::cout << "[Overlay] Status: Auto-attaching when hooked game is in foreground..." << std::endl;

        return true;
    }

    void Run() {
        MSG msg = {};
        while (msg.message != WM_QUIT) {
            while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
                if (msg.message == WM_HOTKEY) {
                    HandleHotkey(static_cast<int>(msg.wParam));
                }
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }

            UpdateAndRender();
            Sleep(16); // ~60 Hz render tick
        }
    }

    void HandleHotkey(int id) {
        auto* pIpc = m_ipc.Get();
        if (id == 1 || id == 2 || id == 3) {
            // Toggle Overlay Visibility
            m_userVisible = !m_userVisible;
            if (pIpc) pIpc->overlay_enabled = m_userVisible ? 1 : 0;
            std::cout << "[Overlay] User toggled HUD: " << (m_userVisible ? "VISIBLE" : "HIDDEN") << std::endl;
            if (!m_userVisible) {
                ShowWindow(m_hWnd, SW_HIDE);
            }
        } else if (id == 4) {
            // Toggle Pacer Active / Passthrough
            if (pIpc) {
                pIpc->pacing_enabled = (pIpc->pacing_enabled != 0) ? 0 : 1;
                std::cout << "[Overlay] Pacing toggled: " << (pIpc->pacing_enabled ? "ACTIVE" : "PASSTHROUGH") << std::endl;
            }
        } else if (id == 5) {
            // Target FPS +1
            if (pIpc) {
                pIpc->target_fps = min(360u, pIpc->target_fps + 1);
                std::cout << "[Overlay] Target FPS increased to: " << pIpc->target_fps << " FPS" << std::endl;
            }
        } else if (id == 6) {
            // Target FPS -1
            if (pIpc) {
                pIpc->target_fps = max(15u, pIpc->target_fps - 1);
                std::cout << "[Overlay] Target FPS decreased to: " << pIpc->target_fps << " FPS" << std::endl;
            }
        }
    }

    void UpdateAndRender() {
        auto* pIpc = m_ipc.Get();
        if (!pIpc) return;

        bool overlayEnabled = (pIpc->overlay_enabled != 0) && m_userVisible;
        if (!overlayEnabled || pIpc->process_id == 0) {
            ShowWindow(m_hWnd, SW_HIDE);
            return;
        }

        // Check if the foreground window belongs to the hooked game process
        HWND fgWnd = GetForegroundWindow();
        if (!fgWnd || fgWnd == m_hWnd) {
            return;
        }

        DWORD fgPid = 0;
        GetWindowThreadProcessId(fgWnd, &fgPid);

        if (fgPid != pIpc->process_id) {
            // User alt-tabbed to desktop, browser, discord, etc. -> Hide overlay
            ShowWindow(m_hWnd, SW_HIDE);
            return;
        }

        // Attached to active game window! Position relative to game rect
        RECT fgRect;
        if (!GetWindowRect(fgWnd, &fgRect)) {
            return;
        }

        int width = fgRect.right - fgRect.left;
        int height = fgRect.bottom - fgRect.top;
        if (width < 200 || height < 200) {
            return;
        }

        int posX = fgRect.left + 24;
        int posY = fgRect.top + 24;

        uint32_t posMode = pIpc->overlay_position;
        switch (posMode) {
            case 0: // Top-Left
                posX = fgRect.left + 24;
                posY = fgRect.top + 24;
                break;
            case 1: // Top-Right
                posX = fgRect.right - m_overlayWidth - 24;
                posY = fgRect.top + 24;
                break;
            case 2: // Bottom-Left
                posX = fgRect.left + 24;
                posY = fgRect.bottom - m_overlayHeight - 24;
                break;
            case 3: // Bottom-Right
                posX = fgRect.right - m_overlayWidth - 24;
                posY = fgRect.bottom - m_overlayHeight - 24;
                break;
            default:
                break;
        }

        SetWindowPos(m_hWnd, HWND_TOPMOST, posX, posY, m_overlayWidth, m_overlayHeight, SWP_NOACTIVATE | SWP_NOSIZE);
        ShowWindow(m_hWnd, SW_SHOWNOACTIVATE);

        // Read Live Telemetry
        float fps = pIpc->current_fps;
        float frameTimeMs = pIpc->current_frametime_ms;
        float jitterUs = pIpc->jitter_us;
        float targetFps = static_cast<float>(pIpc->target_fps);
        bool pacingActive = (pIpc->pacing_enabled != 0);

        // Update sparkline history
        if (frameTimeMs > 0.1f) {
            m_history.push_back(frameTimeMs);
            if (m_history.size() > 32) m_history.erase(m_history.begin());
        }

        // Render Layered Window with Per-Pixel Alpha (GDI+ Double Buffer)
        HDC screenDC = GetDC(NULL);
        HDC memDC = CreateCompatibleDC(screenDC);
        
        BITMAPINFO bmi = {};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = m_overlayWidth;
        bmi.bmiHeader.biHeight = -m_overlayHeight; // Top-down
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        void* pBits = nullptr;
        HBITMAP memBitmap = CreateDIBSection(memDC, &bmi, DIB_RGB_COLORS, &pBits, NULL, 0);
        HGDIOBJ oldBitmap = SelectObject(memDC, memBitmap);

        Graphics g(memDC);
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);

        g.Clear(Color(0, 0, 0, 0));

        // 1. Sleek Cyberpunk Dark Glass Background with Inset Rounded Corners (prevents stroke clipping)
        RectF cardRect(1.5f, 1.5f, static_cast<float>(m_overlayWidth) - 3.0f, static_cast<float>(m_overlayHeight) - 3.0f);
        SolidBrush bgBrush(Color(235, 8, 12, 18));
        GraphicsPath bgPath;
        AddRoundedRect(bgPath, cardRect, 8.0f);
        g.FillPath(&bgBrush, &bgPath);

        // 2. Glowing Cyan / Subtle Border
        Color borderColor = pacingActive ? Color(220, 0, 240, 255) : Color(140, 148, 163, 184);
        Pen borderPen(borderColor, 1.5f);
        g.DrawPath(&borderPen, &bgPath);

        // 3. Fonts
        FontFamily ffSans(L"Segoe UI");
        FontFamily ffMono(L"Consolas");
        Font titleFont(&ffSans, 8.5f, FontStyleBold, UnitPoint);
        Font statusFont(&ffMono, 8.5f, FontStyleBold, UnitPoint);
        Font metricFont(&ffMono, 10.0f, FontStyleBold, UnitPoint);
        Font subFont(&ffMono, 8.0f, FontStyleRegular, UnitPoint);

        // 4. Header Badge: "FRAMEPACER" [LOCKED]
        SolidBrush titleBrush(Color(255, 0, 240, 255));
        g.DrawString(L"FRAMEPACER", -1, &titleFont, PointF(12.0f, 8.0f), &titleBrush);

        std::wstring lockText = pacingActive ? L"[LOCKED]" : L"[PASS]";
        SolidBrush lockBrush(pacingActive ? Color(255, 16, 185, 129) : Color(255, 148, 163, 184));
        g.DrawString(lockText.c_str(), -1, &statusFont, PointF(172.0f, 8.0f), &lockBrush);

        // 5. Left Column: FPS (cyan), Frametime (green), Jitter (dim)
        std::wostringstream ssFps, ssMs, ssJitter;
        ssFps << std::fixed << std::setprecision(1) << fps << L" FPS";
        ssMs << std::fixed << std::setprecision(2) << frameTimeMs << L" ms";
        if (jitterUs < 100.0f) {
            ssJitter << L"JIT: +/-" << static_cast<int>(jitterUs) << L" us";
        } else {
            ssJitter << L"JIT: +/-" << std::fixed << std::setprecision(1) << (jitterUs / 1000.0f) << L" ms";
        }

        SolidBrush cyanBrush(Color(255, 0, 240, 255));
        SolidBrush emeraldBrush(Color(255, 16, 185, 129));
        SolidBrush dimBrush(Color(255, 148, 163, 184));

        std::wstring sFps = ssFps.str();
        std::wstring sMs = ssMs.str();
        std::wstring sJit = ssJitter.str();

        g.DrawString(sFps.c_str(), -1, &metricFont, PointF(12.0f, 28.0f), &cyanBrush);
        g.DrawString(sMs.c_str(), -1, &metricFont, PointF(12.0f, 48.0f), &emeraldBrush);
        g.DrawString(sJit.c_str(), -1, &subFont, PointF(12.0f, 68.0f), &dimBrush);

        // 6. Right Column: Dedicated Mini Oscilloscope Sparkline Box (Rounded)
        float spX = 136.0f;
        float spY = 28.0f;
        float spW = 98.0f;
        float spH = 52.0f;

        GraphicsPath spPath;
        AddRoundedRect(spPath, RectF(spX, spY, spW, spH), 4.0f);

        SolidBrush spBg(Color(190, 4, 6, 10));
        g.FillPath(&spBg, &spPath);

        Pen spBorder(Color(80, 255, 255, 255), 1.0f);
        g.DrawPath(&spBorder, &spPath);

        if (m_history.size() > 1) {
            float targetMs = (targetFps > 0) ? (1000.0f / targetFps) : 16.67f;
            float maxScale = targetMs * 1.6f;

            Pen sparkPen(pacingActive ? Color(255, 0, 240, 255) : Color(255, 244, 63, 94), 1.5f);

            for (size_t i = 1; i < m_history.size(); ++i) {
                float x1 = spX + ((i - 1) / static_cast<float>(m_history.size() - 1)) * spW;
                float x2 = spX + (i / static_cast<float>(m_history.size() - 1)) * spW;

                float y1 = spY + spH - (m_history[i - 1] / maxScale) * spH;
                float y2 = spY + spH - (m_history[i] / maxScale) * spH;

                y1 = max(spY + 2.0f, min(spY + spH - 2.0f, y1));
                y2 = max(spY + 2.0f, min(spY + spH - 2.0f, y2));

                g.DrawLine(&sparkPen, x1, y1, x2, y2);
            }
        }

        // Apply Layered Alpha to Window
        POINT ptSrc = { 0, 0 };
        SIZE szWindow = { m_overlayWidth, m_overlayHeight };
        BLENDFUNCTION blend = {};
        blend.BlendOp = AC_SRC_OVER;
        blend.SourceConstantAlpha = 255;
        blend.AlphaFormat = AC_SRC_ALPHA;

        UpdateLayeredWindow(m_hWnd, screenDC, NULL, &szWindow, memDC, &ptSrc, 0, &blend, ULW_ALPHA);

        SelectObject(memDC, oldBitmap);
        DeleteObject(memBitmap);
        DeleteDC(memDC);
        ReleaseDC(NULL, screenDC);
    }

    static void AddRoundedRect(GraphicsPath& path, RectF rect, float radius) {
        float diameter = radius * 2.0f;
        path.AddArc(rect.X, rect.Y, diameter, diameter, 180.0f, 90.0f);
        path.AddArc(rect.X + rect.Width - diameter, rect.Y, diameter, diameter, 270.0f, 90.0f);
        path.AddArc(rect.X + rect.Width - diameter, rect.Y + rect.Height - diameter, diameter, diameter, 0.0f, 90.0f);
        path.AddArc(rect.X, rect.Y + rect.Height - diameter, diameter, diameter, 90.0f, 90.0f);
        path.CloseFigure();
    }

    static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        switch (msg) {
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        }
        return DefWindowProc(hWnd, msg, wParam, lParam);
    }

private:
    HWND m_hWnd;
    bool m_userVisible;
    ULONG_PTR m_gdiplusToken;
    int m_overlayWidth;
    int m_overlayHeight;
    FramePacer::SharedMemoryChannel m_ipc;
    std::vector<float> m_history;
};

int main(int argc, char* argv[]) {
    HINSTANCE hInstance = GetModuleHandle(NULL);
    TransparentHUDOverlay overlay;
    if (overlay.Initialize(hInstance)) {
        overlay.Run();
    }
    return 0;
}
