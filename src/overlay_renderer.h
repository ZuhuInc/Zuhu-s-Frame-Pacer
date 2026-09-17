#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <d3dcompiler.h>
#include <vector>
#include <string>
#include <sstream>
#include <iomanip>
#include <cmath>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")

namespace FramePacer {

// Compact 8x13 ASCII Bitmap Font for Direct3D 11 Overlay
static const unsigned char FONT_BITMAP[96][13] = {
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, // space
    {0x00,0x00,0x18,0x18,0x00,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x00}, // !
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x36,0x36,0x36,0x00}, // "
    {0x00,0x00,0x36,0x36,0x7f,0x36,0x36,0x36,0x7f,0x36,0x36,0x00,0x00}, // #
    {0x00,0x18,0x3e,0x60,0x3c,0x06,0x3e,0x60,0x3c,0x06,0x3e,0x18,0x00}, // $
    {0x00,0x00,0x66,0x66,0x30,0x18,0x0c,0x06,0x63,0x66,0x66,0x00,0x00}, // %
    {0x00,0x00,0x3c,0x66,0x66,0x3c,0x1e,0x33,0x66,0x66,0x3c,0x00,0x00}, // &
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x18,0x00}, // '
    {0x00,0x0c,0x18,0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x18,0x0c,0x00}, // (
    {0x00,0x30,0x18,0x0c,0x0c,0x0c,0x0c,0x0c,0x0c,0x0c,0x18,0x30,0x00}, // )
    {0x00,0x00,0x00,0x66,0x3c,0xff,0x3c,0x66,0x00,0x00,0x00,0x00,0x00}, // *
    {0x00,0x00,0x00,0x18,0x18,0x7e,0x18,0x18,0x00,0x00,0x00,0x00,0x00}, // +
    {0x00,0x30,0x18,0x18,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, // ,
    {0x00,0x00,0x00,0x00,0x00,0x7e,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, // -
    {0x00,0x00,0x18,0x18,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, // .
    {0x00,0x00,0x60,0x30,0x18,0x0c,0x06,0x03,0x00,0x00,0x00,0x00,0x00}, // /
    {0x00,0x00,0x3c,0x66,0x66,0x66,0x66,0x66,0x66,0x66,0x3c,0x00,0x00}, // 0
    {0x00,0x00,0x7e,0x18,0x18,0x18,0x18,0x18,0x18,0x1c,0x18,0x00,0x00}, // 1
    {0x00,0x00,0x7e,0x06,0x0c,0x18,0x30,0x60,0x66,0x66,0x3c,0x00,0x00}, // 2
    {0x00,0x00,0x3c,0x66,0x06,0x06,0x1c,0x06,0x06,0x66,0x3c,0x00,0x00}, // 3
    {0x00,0x00,0x06,0x06,0x7f,0x66,0x66,0x36,0x1e,0x06,0x06,0x00,0x00}, // 4
    {0x00,0x00,0x3c,0x66,0x06,0x06,0x3e,0x60,0x60,0x60,0x7e,0x00,0x00}, // 5
    {0x00,0x00,0x3c,0x66,0x66,0x66,0x7c,0x60,0x60,0x66,0x3c,0x00,0x00}, // 6
    {0x00,0x00,0x0c,0x0c,0x0c,0x0c,0x18,0x30,0x60,0x60,0x7e,0x00,0x00}, // 7
    {0x00,0x00,0x3c,0x66,0x66,0x66,0x3c,0x66,0x66,0x66,0x3c,0x00,0x00}, // 8
    {0x00,0x00,0x3c,0x66,0x06,0x06,0x3e,0x66,0x66,0x66,0x3c,0x00,0x00}, // 9
    {0x00,0x00,0x18,0x18,0x00,0x00,0x00,0x18,0x18,0x00,0x00,0x00,0x00}, // :
    {0x00,0x30,0x18,0x18,0x00,0x00,0x00,0x18,0x18,0x00,0x00,0x00,0x00}, // ;
    {0x00,0x06,0x0c,0x18,0x30,0x60,0x30,0x18,0x0c,0x06,0x00,0x00,0x00}, // <
    {0x00,0x00,0x00,0x00,0x7e,0x00,0x7e,0x00,0x00,0x00,0x00,0x00,0x00}, // =
    {0x00,0x60,0x30,0x18,0x0c,0x06,0x0c,0x18,0x30,0x60,0x00,0x00,0x00}, // >
    {0x00,0x00,0x18,0x18,0x00,0x18,0x0c,0x06,0x66,0x66,0x3c,0x00,0x00}, // ?
    {0x00,0x00,0x3c,0x66,0x6e,0x72,0x60,0x60,0x66,0x66,0x3c,0x00,0x00}, // @
    {0x00,0x00,0x66,0x66,0x66,0x7e,0x66,0x66,0x66,0x3c,0x18,0x00,0x00}, // A
    {0x00,0x00,0x7c,0x66,0x66,0x66,0x7c,0x66,0x66,0x66,0x7c,0x00,0x00}, // B
    {0x00,0x00,0x3c,0x66,0x60,0x60,0x60,0x60,0x60,0x66,0x3c,0x00,0x00}, // C
    {0x00,0x00,0x78,0x6c,0x66,0x66,0x66,0x66,0x66,0x6c,0x78,0x00,0x00}, // D
    {0x00,0x00,0x7e,0x60,0x60,0x60,0x7c,0x60,0x60,0x60,0x7e,0x00,0x00}, // E
    {0x00,0x00,0x60,0x60,0x60,0x60,0x7c,0x60,0x60,0x60,0x7e,0x00,0x00}, // F
    {0x00,0x00,0x3e,0x66,0x66,0x66,0x6e,0x60,0x60,0x66,0x3c,0x00,0x00}, // G
    {0x00,0x00,0x66,0x66,0x66,0x66,0x7e,0x66,0x66,0x66,0x66,0x00,0x00}, // H
    {0x00,0x00,0x3c,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x3c,0x00,0x00}, // I
    {0x00,0x00,0x3c,0x66,0x66,0x06,0x06,0x06,0x06,0x06,0x0f,0x00,0x00}, // J
    {0x00,0x00,0x66,0x66,0x36,0x1e,0x1c,0x36,0x66,0x66,0x66,0x00,0x00}, // K
    {0x00,0x00,0x7e,0x60,0x60,0x60,0x60,0x60,0x60,0x60,0x60,0x00,0x00}, // L
    {0x00,0x00,0x63,0x63,0x63,0x63,0x6b,0x7f,0x77,0x63,0x63,0x00,0x00}, // M
    {0x00,0x00,0x66,0x66,0x66,0x6e,0x7e,0x76,0x66,0x66,0x66,0x00,0x00}, // N
    {0x00,0x00,0x3c,0x66,0x66,0x66,0x66,0x66,0x66,0x66,0x3c,0x00,0x00}, // O
    {0x00,0x00,0x60,0x60,0x60,0x7c,0x66,0x66,0x66,0x66,0x7c,0x00,0x00}, // P
    {0x00,0x07,0x3c,0x6e,0x66,0x66,0x66,0x66,0x66,0x66,0x3c,0x00,0x00}, // Q
    {0x00,0x00,0x66,0x66,0x66,0x6c,0x78,0x6c,0x66,0x66,0x7c,0x00,0x00}, // R
    {0x00,0x00,0x3c,0x66,0x06,0x06,0x3c,0x60,0x60,0x66,0x3c,0x00,0x00}, // S
    {0x00,0x00,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x7e,0x00,0x00}, // T
    {0x00,0x00,0x3c,0x66,0x66,0x66,0x66,0x66,0x66,0x66,0x66,0x00,0x00}, // U
    {0x00,0x00,0x18,0x3c,0x66,0x66,0x66,0x66,0x66,0x66,0x66,0x00,0x00}, // V
    {0x00,0x00,0x63,0x77,0x7f,0x6b,0x63,0x63,0x63,0x63,0x63,0x00,0x00}, // W
    {0x00,0x00,0x66,0x66,0x36,0x1c,0x1c,0x36,0x66,0x66,0x66,0x00,0x00}, // X
    {0x00,0x00,0x18,0x18,0x18,0x18,0x3c,0x66,0x66,0x66,0x66,0x00,0x00}, // Y
    {0x00,0x00,0x7e,0x60,0x30,0x18,0x0c,0x06,0x06,0x66,0x7e,0x00,0x00}, // Z
    {0x00,0x3c,0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x3c,0x00}, // [
    {0x00,0x00,0x03,0x06,0x0c,0x18,0x30,0x60,0x00,0x00,0x00,0x00,0x00}, // backslash
    {0x00,0x3c,0x0c,0x0c,0x0c,0x0c,0x0c,0x0c,0x0c,0x0c,0x0c,0x3c,0x00}, // ]
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x66,0x3c,0x18,0x00}, // ^
    {0xff,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, // _
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x18,0x0c,0x06,0x00}  // `
};

struct OverlayVertex {
    float x, y, z;
    float r, g, b, a;
};

class Direct3D11Overlay {
public:
    static Direct3D11Overlay& Instance() {
        static Direct3D11Overlay s_instance;
        return s_instance;
    }

    bool Initialize(ID3D11Device* pDevice) {
        if (m_isInitialized && m_pDevice == pDevice) return true;
        Cleanup();

        m_pDevice = pDevice;
        if (!m_pDevice) return false;

        // Vertex Shader
        const char* vsSource = 
            "cbuffer MatrixBuffer : register(b0) {\n"
            "   float4x4 u_Proj;\n"
            "};\n"
            "struct VS_INPUT {\n"
            "   float3 pos : POSITION;\n"
            "   float4 col : COLOR;\n"
            "};\n"
            "struct PS_INPUT {\n"
            "   float4 pos : SV_POSITION;\n"
            "   float4 col : COLOR;\n"
            "};\n"
            "PS_INPUT main(VS_INPUT input) {\n"
            "   PS_INPUT output;\n"
            "   output.pos = mul(u_Proj, float4(input.pos, 1.0f));\n"
            "   output.col = input.col;\n"
            "   return output;\n"
            "}\n";

        ID3DBlob* vsBlob = nullptr;
        ID3DBlob* errorBlob = nullptr;
        HRESULT hr = D3DCompile(vsSource, strlen(vsSource), NULL, NULL, NULL, "main", "vs_4_0", 0, 0, &vsBlob, &errorBlob);
        if (FAILED(hr)) return false;

        hr = m_pDevice->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), NULL, &m_pVS);
        if (FAILED(hr)) { vsBlob->Release(); return false; }

        D3D11_INPUT_ELEMENT_DESC layout[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 }
        };

        hr = m_pDevice->CreateInputLayout(layout, 2, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &m_pInputLayout);
        vsBlob->Release();
        if (FAILED(hr)) return false;

        // Pixel Shader
        const char* psSource = 
            "struct PS_INPUT {\n"
            "   float4 pos : SV_POSITION;\n"
            "   float4 col : COLOR;\n"
            "};\n"
            "float4 main(PS_INPUT input) : SV_Target {\n"
            "   return input.col;\n"
            "}\n";

        ID3DBlob* psBlob = nullptr;
        hr = D3DCompile(psSource, strlen(psSource), NULL, NULL, NULL, "main", "ps_4_0", 0, 0, &psBlob, &errorBlob);
        if (FAILED(hr)) return false;

        hr = m_pDevice->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), NULL, &m_pPS);
        psBlob->Release();
        if (FAILED(hr)) return false;

        // Dynamic Vertex Buffer (Max 8192 vertices)
        D3D11_BUFFER_DESC bd = {};
        bd.Usage = D3D11_USAGE_DYNAMIC;
        bd.ByteWidth = sizeof(OverlayVertex) * 8192;
        bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        hr = m_pDevice->CreateBuffer(&bd, NULL, &m_pVB);
        if (FAILED(hr)) return false;

        // Constant Buffer for Projection Matrix
        bd.Usage = D3D11_USAGE_DYNAMIC;
        bd.ByteWidth = sizeof(float) * 16;
        bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        hr = m_pDevice->CreateBuffer(&bd, NULL, &m_pCB);
        if (FAILED(hr)) return false;

        // Alpha Blend State
        D3D11_BLEND_DESC bsd = {};
        bsd.RenderTarget[0].BlendEnable = TRUE;
        bsd.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
        bsd.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
        bsd.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
        bsd.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
        bsd.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
        bsd.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
        bsd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        m_pDevice->CreateBlendState(&bsd, &m_pBlendState);

        // Rasterizer State
        D3D11_RASTERIZER_DESC rd = {};
        rd.FillMode = D3D11_FILL_SOLID;
        rd.CullMode = D3D11_CULL_NONE;
        rd.ScissorEnable = FALSE;
        rd.DepthClipEnable = FALSE;
        m_pDevice->CreateRasterizerState(&rd, &m_pRasterizerState);

        // Depth Stencil State (Disabled Depth Test)
        D3D11_DEPTH_STENCIL_DESC dsd = {};
        dsd.DepthEnable = FALSE;
        dsd.StencilEnable = FALSE;
        m_pDevice->CreateDepthStencilState(&dsd, &m_pDepthStencilState);

        m_isInitialized = true;
        return true;
    }

    void Render(IDXGISwapChain* pSwapChain, float fps, float frametimeMs, float jitterUs, float targetFps, bool pacingActive) {
        // Toggle hotkey check: Ctrl + Shift + O, Ctrl + Shift + 0, F11, or Insert
        static bool s_lastHotkey = false;
        bool isHotkey = ((GetAsyncKeyState(VK_CONTROL) & 0x8000) && (GetAsyncKeyState(VK_SHIFT) & 0x8000) && ((GetAsyncKeyState('O') & 0x8000) || (GetAsyncKeyState('0') & 0x8000)))
                     || (GetAsyncKeyState(VK_F11) & 0x8000)
                     || (GetAsyncKeyState(VK_INSERT) & 0x8000);
        if (isHotkey && !s_lastHotkey) {
            m_isVisible = !m_isVisible;
        }
        s_lastHotkey = isHotkey;

        if (!m_isVisible || !pSwapChain) return;

        ID3D11Device* pDevice = nullptr;
        if (FAILED(pSwapChain->GetDevice(__uuidof(ID3D11Device), reinterpret_cast<void**>(&pDevice))) || !pDevice) {
            return;
        }

        if (!Initialize(pDevice)) {
            pDevice->Release();
            return;
        }

        ID3D11DeviceContext* pContext = nullptr;
        pDevice->GetImmediateContext(&pContext);
        if (!pContext) {
            pDevice->Release();
            return;
        }

        // Get BackBuffer dimensions and Target View
        ID3D11Texture2D* pBackBuffer = nullptr;
        if (FAILED(pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&pBackBuffer))) || !pBackBuffer) {
            pContext->Release();
            pDevice->Release();
            return;
        }

        D3D11_TEXTURE2D_DESC bbDesc;
        pBackBuffer->GetDesc(&bbDesc);

        ID3D11RenderTargetView* pRTV = nullptr;
        pDevice->CreateRenderTargetView(pBackBuffer, NULL, &pRTV);
        pBackBuffer->Release();

        if (!pRTV) {
            pContext->Release();
            pDevice->Release();
            return;
        }

        float screenW = static_cast<float>(bbDesc.Width);
        float screenH = static_cast<float>(bbDesc.Height);

        // Save D3D11 Pipeline State
        ID3D11RenderTargetView* prevRTV = nullptr;
        ID3D11DepthStencilView* prevDSV = nullptr;
        pContext->OMGetRenderTargets(1, &prevRTV, &prevDSV);

        UINT numViewports = 1;
        D3D11_VIEWPORT prevViewport;
        pContext->RSGetViewports(&numViewports, &prevViewport);

        ID3D11BlendState* prevBlendState = nullptr;
        FLOAT prevBlendFactor[4];
        UINT prevSampleMask;
        pContext->OMGetBlendState(&prevBlendState, prevBlendFactor, &prevSampleMask);

        ID3D11DepthStencilState* prevDepthState = nullptr;
        UINT prevStencilRef;
        pContext->OMGetDepthStencilState(&prevDepthState, &prevStencilRef);

        ID3D11RasterizerState* prevRasterState = nullptr;
        pContext->RSGetState(&prevRasterState);

        ID3D11InputLayout* prevInputLayout = nullptr;
        pContext->IAGetInputLayout(&prevInputLayout);

        D3D11_PRIMITIVE_TOPOLOGY prevTopology;
        pContext->IAGetPrimitiveTopology(&prevTopology);

        ID3D11Buffer* prevVB = nullptr;
        UINT prevStride, prevOffset;
        pContext->IAGetVertexBuffers(0, 1, &prevVB, &prevStride, &prevOffset);

        ID3D11VertexShader* prevVS = nullptr;
        ID3D11PixelShader* prevPS = nullptr;
        pContext->VSGetShader(&prevVS, NULL, NULL);
        pContext->PSGetShader(&prevPS, NULL, NULL);

        // Setup Projection Matrix (Orthographic 2D top-left [0,0] to [screenW, screenH])
        float orthoProj[16] = {
            2.0f / screenW, 0.0f, 0.0f, 0.0f,
            0.0f, -2.0f / screenH, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            -1.0f, 1.0f, 0.0f, 1.0f
        };

        D3D11_MAPPED_SUBRESOURCE mappedResource;
        if (SUCCEEDED(pContext->Map(m_pCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource))) {
            memcpy(mappedResource.pData, orthoProj, sizeof(orthoProj));
            pContext->Unmap(m_pCB, 0);
        }

        // Build HUD Overlay Vertices
        std::vector<OverlayVertex> verts;

        float boxX = 16.0f;
        float boxY = 16.0f;
        float boxW = 215.0f;
        float boxH = 75.0f;

        // 1. Dark Glass Background Quad
        AddFilledQuad(verts, boxX, boxY, boxW, boxH, 0.04f, 0.06f, 0.09f, 0.88f);

        // 2. Cyan / Green Glowing Border
        float br = pacingActive ? 0.0f : 0.6f;
        float bg = pacingActive ? 0.94f : 0.6f;
        float bb = pacingActive ? 1.0f : 0.6f;
        AddRectOutline(verts, boxX, boxY, boxW, boxH, 1.5f, br, bg, bb, 0.65f);

        // 3. Header Text: "FRAMEPACER [LOCKED]"
        std::string header = pacingActive ? "FRAMEPACER [LOCKED]" : "FRAMEPACER [PASSTHRU]";
        DrawString(verts, boxX + 10, boxY + 8, header, br, bg, bb, 1.0f);

        // 4. Metrics: FPS | Frametime ms | Jitter us
        std::ostringstream ssFps, ssMs, ssJitter;
        ssFps << std::fixed << std::setprecision(1) << fps << " FPS";
        ssMs << std::fixed << std::setprecision(2) << frametimeMs << " ms";
        if (jitterUs < 100.0f) {
            ssJitter << "JIT: +/-" << static_cast<int>(jitterUs) << " us";
        } else {
            ssJitter << "JIT: +/-" << std::fixed << std::setprecision(1) << (jitterUs / 1000.0f) << " ms";
        }

        DrawString(verts, boxX + 10, boxY + 28, ssFps.str(), 1.0f, 1.0f, 1.0f, 1.0f);
        DrawString(verts, boxX + 100, boxY + 28, ssMs.str(), 0.06f, 0.72f, 0.5f, 1.0f);
        DrawString(verts, boxX + 10, boxY + 48, ssJitter.str(), 0.65f, 0.7f, 0.8f, 1.0f);

        // 5. Sparkline
        m_history.push_back(frametimeMs);
        if (m_history.size() > 30) m_history.erase(m_history.begin());

        if (m_history.size() > 1) {
            float spX = boxX + 110.0f;
            float spY = boxY + 48.0f;
            float spW = 95.0f;
            float spH = 16.0f;

            AddFilledQuad(verts, spX, spY, spW, spH, 0.02f, 0.03f, 0.05f, 0.7f);

            float targetMs = (targetFps > 0) ? (1000.0f / targetFps) : 16.67f;
            float maxScale = targetMs * 1.6f;

            for (size_t i = 1; i < m_history.size(); ++i) {
                float x1 = spX + ((i - 1) / static_cast<float>(m_history.size() - 1)) * spW;
                float x2 = spX + (i / static_cast<float>(m_history.size() - 1)) * spW;

                float y1 = spY + spH - (m_history[i - 1] / maxScale) * spH;
                float y2 = spY + spH - (m_history[i] / maxScale) * spH;

                y1 = max(spY, min(spY + spH, y1));
                y2 = max(spY, min(spY + spH, y2));

                AddFilledQuad(verts, x1, y1, (x2 - x1) + 1.0f, 2.0f, br, bg, bb, 0.9f);
            }
        }

        // Upload vertex data
        if (!verts.empty() && SUCCEEDED(pContext->Map(m_pVB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource))) {
            memcpy(mappedResource.pData, verts.data(), verts.size() * sizeof(OverlayVertex));
            pContext->Unmap(m_pVB, 0);

            // Bind Pipeline State for Rendering
            D3D11_VIEWPORT vp = { 0.0f, 0.0f, screenW, screenH, 0.0f, 1.0f };
            pContext->RSSetViewports(1, &vp);
            pContext->OMSetRenderTargets(1, &pRTV, NULL);

            FLOAT blendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
            pContext->OMSetBlendState(m_pBlendState, blendFactor, 0xFFFFFFFF);
            pContext->OMSetDepthStencilState(m_pDepthStencilState, 0);
            pContext->RSSetState(m_pRasterizerState);

            pContext->IASetInputLayout(m_pInputLayout);
            pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

            UINT stride = sizeof(OverlayVertex);
            UINT offset = 0;
            pContext->IASetVertexBuffers(0, 1, &m_pVB, &stride, &offset);

            pContext->VSSetShader(m_pVS, NULL, 0);
            pContext->VSSetConstantBuffers(0, 1, &m_pCB);
            pContext->PSSetShader(m_pPS, NULL, 0);

            pContext->Draw(static_cast<UINT>(verts.size()), 0);
        }

        // Restore Pipeline State
        pContext->OMSetRenderTargets(1, &prevRTV, prevDSV);
        pContext->RSSetViewports(numViewports, &prevViewport);
        pContext->OMSetBlendState(prevBlendState, prevBlendFactor, prevSampleMask);
        pContext->OMSetDepthStencilState(prevDepthState, prevStencilRef);
        pContext->RSSetState(prevRasterState);
        pContext->IASetInputLayout(prevInputLayout);
        pContext->IASetPrimitiveTopology(prevTopology);
        pContext->IASetVertexBuffers(0, 1, &prevVB, &prevStride, &prevOffset);
        pContext->VSSetShader(prevVS, NULL, 0);
        pContext->PSSetShader(prevPS, NULL, 0);

        if (prevRTV) prevRTV->Release();
        if (prevDSV) prevDSV->Release();
        if (prevBlendState) prevBlendState->Release();
        if (prevDepthState) prevDepthState->Release();
        if (prevRasterState) prevRasterState->Release();
        if (prevInputLayout) prevInputLayout->Release();
        if (prevVB) prevVB->Release();
        if (prevVS) prevVS->Release();
        if (prevPS) prevPS->Release();

        pRTV->Release();
        pContext->Release();
        pDevice->Release();
    }

    void ToggleVisibility() { m_isVisible = !m_isVisible; }
    void SetVisible(bool v) { m_isVisible = v; }
    bool IsVisible() const { return m_isVisible; }

    void Cleanup() {
        if (m_pBlendState) { m_pBlendState->Release(); m_pBlendState = nullptr; }
        if (m_pRasterizerState) { m_pRasterizerState->Release(); m_pRasterizerState = nullptr; }
        if (m_pDepthStencilState) { m_pDepthStencilState->Release(); m_pDepthStencilState = nullptr; }
        if (m_pInputLayout) { m_pInputLayout->Release(); m_pInputLayout = nullptr; }
        if (m_pVS) { m_pVS->Release(); m_pVS = nullptr; }
        if (m_pPS) { m_pPS->Release(); m_pPS = nullptr; }
        if (m_pVB) { m_pVB->Release(); m_pVB = nullptr; }
        if (m_pCB) { m_pCB->Release(); m_pCB = nullptr; }
        m_isInitialized = false;
    }

private:
    Direct3D11Overlay()
        : m_isInitialized(false)
        , m_isVisible(true)
        , m_pDevice(nullptr)
        , m_pInputLayout(nullptr)
        , m_pVS(nullptr)
        , m_pPS(nullptr)
        , m_pVB(nullptr)
        , m_pCB(nullptr)
        , m_pBlendState(nullptr)
        , m_pRasterizerState(nullptr)
        , m_pDepthStencilState(nullptr)
    {}

    ~Direct3D11Overlay() { Cleanup(); }

    void AddFilledQuad(std::vector<OverlayVertex>& verts, float x, float y, float w, float h, float r, float g, float b, float a) {
        OverlayVertex v0 = { x, y, 0.0f, r, g, b, a };
        OverlayVertex v1 = { x + w, y, 0.0f, r, g, b, a };
        OverlayVertex v2 = { x, y + h, 0.0f, r, g, b, a };
        OverlayVertex v3 = { x + w, y + h, 0.0f, r, g, b, a };

        verts.push_back(v0);
        verts.push_back(v1);
        verts.push_back(v2);

        verts.push_back(v1);
        verts.push_back(v3);
        verts.push_back(v2);
    }

    void AddRectOutline(std::vector<OverlayVertex>& verts, float x, float y, float w, float h, float t, float r, float g, float b, float a) {
        AddFilledQuad(verts, x, y, w, t, r, g, b, a);             // Top
        AddFilledQuad(verts, x, y + h - t, w, t, r, g, b, a);     // Bottom
        AddFilledQuad(verts, x, y + t, t, h - (2 * t), r, g, b, a); // Left
        AddFilledQuad(verts, x + w - t, y + t, t, h - (2 * t), r, g, b, a); // Right
    }

    void DrawChar(std::vector<OverlayVertex>& verts, float x, float y, char c, float r, float g, float b, float a) {
        int idx = static_cast<unsigned char>(c) - 32;
        if (idx < 0 || idx >= 96) return;

        float pixelSize = 1.0f;
        for (int row = 0; row < 13; ++row) {
            unsigned char byteVal = FONT_BITMAP[idx][row];
            for (int col = 0; col < 8; ++col) {
                if ((byteVal >> (7 - col)) & 1) {
                    float px = x + col * pixelSize;
                    float py = y + (12 - row) * pixelSize;
                    AddFilledQuad(verts, px, py, pixelSize, pixelSize, r, g, b, a);
                }
            }
        }
    }

    void DrawString(std::vector<OverlayVertex>& verts, float x, float y, const std::string& text, float r, float g, float b, float a) {
        float curX = x;
        for (char c : text) {
            DrawChar(verts, curX, y, c, r, g, b, a);
            curX += 9.0f;
        }
    }

    bool m_isInitialized;
    bool m_isVisible;
    ID3D11Device* m_pDevice;
    ID3D11InputLayout* m_pInputLayout;
    ID3D11VertexShader* m_pVS;
    ID3D11PixelShader* m_pPS;
    ID3D11Buffer* m_pVB;
    ID3D11Buffer* m_pCB;
    ID3D11BlendState* m_pBlendState;
    ID3D11RasterizerState* m_pRasterizerState;
    ID3D11DepthStencilState* m_pDepthStencilState;

    std::vector<float> m_history;
};

} // namespace FramePacer
