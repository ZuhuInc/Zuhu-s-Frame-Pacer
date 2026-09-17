# Implementation Plan: Universal Graphics API Frame Pacer

## Executive Summary
This document outlines the architecture, implementation steps, and technical mechanisms required to build a standalone, open-source display frame pacer (similar to tools like *framepacer*, *RTSS*, or *Special K*). 

The primary goal is to ensure **perfect on-screen frame persistence**: every rendered frame remains visible on the monitor for an identical duration, eliminating cadence judder and micro-stutter in single-player games across multiple graphics APIs (DirectX 11, DirectX 12, and Vulkan).

---

## 1. Problem Statement & Core Mechanics

### Why Engine-Level Limiters Fail
Most built-in game framerate limiters regulate the **simulation loop** (tick $\to$ physics $\to$ draw). However, because rendering workloads fluctuate frame-by-frame:
- A frame that takes 8 ms to render will finish early and sit in the driver backbuffer.
- The next frame might take 15 ms, causing the DWM/driver to present frames at irregular intervals (e.g., alternating between 16 ms and 33 ms on a 60 Hz display).
- **Result:** Average FPS might read 60, but visually the game jitters.

### The Solution: Presentation-Edge Pacing
A true frame pacer operates at the **presentation edge** (the exact moment the frame is handed over to the display pipeline).

```
Flawed Engine Pacing (Simulation Loop Limiting):
[Tick 16.6ms] -> [Fast Render 8ms]  -------------------------> Screen
[Tick 16.6ms] -> [Heavy Render 15ms] -----------------------> Screen (Late!)
Cadence: Wobbly, uneven on-screen exposure.

True Presentation Pacing:
[Render Done] ──> [Hold until exact scanout/time boundary] ──> Screen
[Render Done] ──> [Hold until exact scanout/time boundary] ──> Screen
Cadence: Flat, perfectly uniform on-screen duration.
```

---

## 2. Pacing Strategies by Display Type

### Mode A: Fixed Refresh Displays (60 Hz, 120 Hz, 144 Hz)
On fixed-refresh monitors, frames can only physically change during a **Vertical Blanking Interval (VBlank)**. Arbitrary microsecond pacing does not work here; frames must be locked to exact integer VBlank divisors:

| Display Hz | Target FPS | VBlank Divisor (`SyncInterval`) | Duration per Frame |
| :--- | :--- | :--- | :--- |
| **60 Hz** | 60 FPS | 1 | 16.67 ms |
| **60 Hz** | 30 FPS | 2 | 33.33 ms |
| **120 Hz** | 40 FPS | 3 | 25.00 ms |
| **120 Hz** | 60 FPS | 2 | 16.67 ms |
| **144 Hz** | 72 FPS | 2 | 13.88 ms |

**Mechanism:**
1. Hook `IDXGISwapChain::Present`.
2. Restrict frame queue latency to 1: `SetMaximumFrameLatency(1)` to eliminate buffered backlog.
3. Call `pSwapChain->Present(calculatedSyncInterval, 0)`.
4. Inspect `IDXGISwapChain::GetFrameStatistics` (`SyncRefreshCount`) to detect dropped/duplicated VBlanks.

### Mode B: Variable Refresh Rate (VRR / G-Sync / FreeSync)
On VRR panels, the monitor has no fixed scanout clock; its hardware refresh rate dynamically matches the presentation interval.
1. Target framerate must be capped **2–3 FPS below maximum monitor Hz** (e.g., 141 FPS on a 144 Hz panel) to prevent tripping standard VSync buffers.
2. Calculate exact interval:
   $$\Delta t = \frac{1\,000\,000\,000}{\text{Target FPS}} \text{ nanoseconds}$$
3. Perform a high-precision wait until `PreviousPresentTime + TargetInterval` before issuing `Present(0, 0)`.

---

## 3. High-Precision Timing Engine

Standard Win32 `Sleep()` is restricted by default OS scheduler ticks (~15.6 ms resolution). The pacer uses a **hybrid sleep/spin model**:

1. **Coarse Sleep:** Use modern high-resolution waitable timers:
   ```cpp
   HANDLE timer = CreateWaitableTimerExW(
       NULL, NULL, 
       CREATE_WAITABLE_TIMER_HIGH_RESOLUTION | CREATE_WAITABLE_TIMER_MANUAL_RESET, 
       TIMER_ALL_ACCESS
   );
   ```
2. **Fine Spin-Wait:** Sleep until ~0.5 ms prior to target deadline, then spin-wait using `QueryPerformanceCounter` (QPC) and `_mm_pause()` instructions to yield CPU pipeline resources without dropping thread priority.

---

## 4. Architectural Components

```
+-------------------------------------------------------------+
|                 Desktop Frontend / UI                       |
|  - Sliders for target frametime / FPS                       |
|  - Real-time frametime histogram & frame-cadence graph      |
|  - Profiles for single-player presets (30/40/60/VRR)        |
+------------------------------+------------------------------+
                               | IPC (Shared Memory / Named Pipes)
+------------------------------v------------------------------+
|                Service / Injector Module                    |
|  - Monitors active foreground window (SetWinEventHook)      |
|  - Checks process architecture (x64 / ARM64)                |
|  - Injects Pacer Hook DLL into game process                 |
+------------------------------+------------------------------+
                               |
+------------------------------v------------------------------+
|                 Injected Pacer Core (DLL)                   |
|  - API Hooks: DXGI (DX11/DX12), Vulkan Layer, OpenGL        |
|  - Cadence calculation & waitable timer synchronization     |
|  - Frame latency queue clamping (Queue Depth = 1)           |
+-------------------------------------------------------------+
```

---

## 5. Graphics API Interception Breakdown

### DirectX 11 & DirectX 12
- **Target DLL:** `dxgi.dll`
- **Functions to Hook:**
  - `IDXGISwapChain::Present` (VTable Index 8)
  - `IDXGISwapChain1::Present1` (VTable Index 22)
- **Hooking Method:** Use **MinHook** or **Microsoft Detours** to redirect the presentation function to the pacer proxy.
- **Latency Control:** Query `IDXGISwapChain2` for `GetFrameLatencyWaitableObject()` to align directly with the Windows composition pipeline.

### Vulkan
- **Target Method:** Implicit Vulkan Layer (Native Khronos Specification).
- **Function to Intercept:** `vkQueuePresentKHR`.
- **Advantage:** No memory patching or injection needed. Vulkan automatically loads layers defined in:
  `HKEY_LOCAL_MACHINE\SOFTWARE\Khronos\Vulkan\ImplicitLayers`
- Modern extensions `VK_KHR_present_wait` and `VK_GOOGLE_display_timing` provide direct query access to actual display scanout timing.

### OpenGL
- **Target Functions:** `wglSwapBuffers` (Windows) / `glXSwapBuffers` (Linux).

---

## 6. Phased Implementation Roadmap

### Phase 1: Standalone Timing Benchmark (C++)
- Implement the hybrid high-resolution timer (`CreateWaitableTimerExW` + QPC spin-wait).
- Benchmark jitter across 10,000 cycles. Verify max standard deviation is $<0.05\text{ ms}$.

### Phase 2: DX11/DX12 Hook Prototype
- Build a barebones DLL utilizing **MinHook** to attach to `IDXGISwapChain::Present`.
- Enforce `SetMaximumFrameLatency(1)`.
- Test on a simple 3D game (e.g., single-player DX11 title) with fixed 30 FPS / 60 FPS pacing.

### Phase 3: IPC & Configuration Channel
- Implement Windows File Mapping (`CreateFileMappingW`) for zero-overhead shared memory.
- Create a simple payload struct:
  ```cpp
  struct PacerControlBlock {
      uint32_t target_fps;
      bool vrr_mode_enabled;
      float recent_frametimes[120];
  };
  ```

### Phase 4: Frontend GUI
- Lightweight UI using Dear ImGui, Slint, or C#/WPF.
- Provide quick presets:
  - **Console Flat 30:** Rock-solid 33.33 ms delivery.
  - **Handheld Balance 40:** Optimized for 120 Hz displays (Steam Deck / ROG Ally / 120 Hz TVs).
  - **Standard 60:** 16.67 ms.
  - **VRR Flush:** Automatic `(RefreshRate - 3)` cap.
- Visual frametime graph (displaying frametime variance in milliseconds).

### Phase 5: Vulkan Layer Support
- Generate Vulkan JSON manifest and build `vkQueuePresentKHR` layer wrapper for full modern Linux / Windows Vulkan support.

---

## 7. Operational Warnings & Anti-Cheat Scope
- **Scope:** Intended strictly for single-player titles, retro games, emulation, or pairing with frame-generation software (such as Lossless Scaling / LSFG).
- **Anti-Cheat Notice:** DLL injection and presentation table hooking are flagged by kernel-level anti-cheat engines (Easy Anti-Cheat, BattlEye, Vanguard, Ricochet). The launcher should maintain a strict blocklist for known multiplayer executables.
