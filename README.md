# ? FramePacer

<div align=""center"">
  <img src=""assets/framepacer-logo.png"" alt=""FramePacer Logo"" width=""128"" height=""128"">
  
  ### Universal Presentation Frame Pacer & Jitter Eliminator
  
  *Sub-millisecond presentation-edge pacing for perfectly flat frametime cadence.*

  [![Platform](https://img.shields.io/badge/platform-Windows%20x64-blue.svg)](https://github.com/ZuhuInc/Zuhu-s-Frame-Pacer)
  [![C++ Standard](https://img.shields.io/badge/C%2B%2B-17-00599C.svg?logo=c%2B%2B)](https://github.com/ZuhuInc/Zuhu-s-Frame-Pacer)
  [![Electron](https://img.shields.io/badge/Electron-Desktop-47848F.svg?logo=electron)](https://github.com/ZuhuInc/Zuhu-s-Frame-Pacer)
  [![License](https://img.shields.io/badge/license-ISC-green.svg)](LICENSE)
</div>

---

## ?? Overview

**FramePacer** is a lightweight, high-precision frame pacing utility engineered for PC gaming, single-player titles, emulators, and display frame generation tools (like Lossless Scaling / LSFG). 

Unlike traditional software frame limiters that sleep inside CPU render threads—which often leads to micro-stutter and display presentation jitter—FramePacer intercepts frames directly at the **Presentation Edge** (IDXGISwapChain::Present / Present1) using microsecond-accurate hybrid spin-wait timing.

`
+------------------+     +-------------------+     +--------------------------+
|  Game Engine CPU | --> | DirectX / Vulkan  | --> | FramePacer Presentation  | --> Display Swapchain
|   Render Loop    |     |    Draw Calls     |     |   Microsecond Pacing     |     (Flat Cadence)
+------------------+     +-------------------+     +--------------------------+
`

---

## ? Key Features

- ?? **Presentation-Edge Pacing**: Clamps frametimes right before the GPU swapchain flip, eliminating micro-stutters and uneven frame pacing.
- ?? **Sub-Millisecond Precision**: Combines 	imeBeginPeriod(1), CreateWaitableTimerEx, and High-Resolution Query Performance Counters (QPC) with adaptive spin-wait loops.
- ?? **Real-Time Oscilloscope**: Live visual frametime waveform oscilloscope, rolling FPS average, standard deviation jitter calculation ($\pm \mu s$), and 1% low consistency metrics.
- ?? **Compact Mini Mode**: Switch to a sleek cyberpunk floating widget (480 x 290) with one click or hotkey.
- ?? **In-Game Transparent HUD Overlay**: Hardware-accelerated GDI+ click-through overlay with anti-aliased live sparkline graphs.
- ??? **Safe Auto-Attach Engine**: Automatically detects active 3D game windows while safeguarding system processes, background overlays, and anti-cheat protected titles.
- ? **Zero Global Registry Pollution**: In-process hooking that leaves your Windows Vulkan and DirectX system registry 100% clean.

---

## ?? Global Hotkeys

| Hotkey | Action | Description |
| :--- | :--- | :--- |
| <kbd>Insert</kbd> | **Toggle Dashboard** | Show / Hide the main FramePacer window from anywhere |
| <kbd>F11</kbd> / <kbd>Ctrl</kbd> + <kbd>Shift</kbd> + <kbd>O</kbd> | **Toggle In-Game HUD** | Show / Hide the transparent in-game telemetry HUD overlay |
| <kbd>Ctrl</kbd> + <kbd>Shift</kbd> + <kbd>P</kbd> | **Toggle Pacer Engine** | Instantly switch between locked precision pacing & raw passthrough |
| <kbd>Ctrl</kbd> + <kbd>Shift</kbd> + <kbd>?</kbd> / <kbd>?</kbd> | **Fine-Tune FPS** | Adjust your target FPS limit on the fly in $\pm 1\text{ FPS}$ steps |

---

## ?? Getting Started

### Prerequisites
- **Windows 10 / 11 (64-bit)**
- **Node.js** (v18 or newer)
- **Visual Studio 2019/2022/2026** (C++ x64 Build Tools)

### Quick Start (Development)
`ash
# 1. Clone repository
git clone https://github.com/ZuhuInc/Zuhu-s-Frame-Pacer.git
cd Zuhu-s-Frame-Pacer

# 2. Install dependencies
npm install

# 3. Build C++ engine binaries
build.bat

# 4. Launch FramePacer
npm start
`
*Or simply double-click Start_FramePacer.bat.*

---

## ?? Building Standalone Executable (.exe)

To generate a standalone portable distribution of FramePacer:

1. Double-click **Build_Executable.bat** (or run 
pm run build:exe).
2. Your packaged build will be generated in:
   `
   release/FramePacer-win32-x64/FramePacer.exe
   `

---

## ??? Architecture

`
FramePacer/
+-- src/
¦   +-- dxgi_hook.cpp / .h       # DirectX 11 & DirectX 12 presentation VTable hook
¦   +-- vulkan_hook.cpp / .h     # In-process Vulkan presentation dispatcher
¦   +-- timing_engine.h          # Microsecond hybrid spin-wait QPC pacer
¦   +-- transparent_overlay.cpp  # GDI+ layered click-through in-game HUD
¦   +-- bridge_server.cpp        # Real-time WebSocket IPC telemetry bridge
¦   +-- game_watcher.h           # Auto-detection scanner & process injector
¦   +-- minhook/                 # Minimalist x64 instruction relocation engine
+-- ui/
¦   +-- index.html               # Modern Dashboard & Mini Widget interface
¦   +-- css/style.css            # Dark glassmorphic styling system
¦   +-- js/                      # Real-time WebSocket telemetry & oscilloscope canvas
+-- assets/                      # Vector SVG icons, ICOs, and tray graphics
+-- main.js                      # Electron main lifecycle & global hotkey manager
+-- build.bat                    # MSVC compiler script for C++ engine binaries
+-- Build_Executable.bat         # One-click standalone executable packaging script
`

---

## ??? Roadmap

- [x] DirectX 11 & DirectX 12 Presentation-Edge Pacing
- [x] Real-time oscilloscope with Jitter ($\pm \mu s$) and 1% lows
- [x] Hardware-accelerated transparent in-game HUD
- [x] Mini mode Cyberpunk widget
- [x] Standalone .exe packaging
- [ ] In-process Native Vulkan (kQueuePresentKHR) engine
- [ ] Per-game profile auto-saving & auto-loading
- [ ] Steam Deck / Windows Handheld auto-TDP synchronization

---

## ??? Anti-Cheat & Safety Disclaimer

FramePacer is designed specifically for **single-player games, retro games, emulators, and local display enhancements**. 

Because DLL injection and presentation table hooking are used, kernel-level multiplayer anti-cheat systems (e.g., Easy Anti-Cheat, BattlEye, Vanguard, Ricochet) will block or flag third-party hooks. FramePacer includes a comprehensive process blocklist to protect known multiplayer titles from being attached.

---

## ?? License

Distributed under the **ISC License**. See LICENSE for more information.

Developed with ?? by **ZuhuInc**.
