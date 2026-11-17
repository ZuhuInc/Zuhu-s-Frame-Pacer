/**
 * FramePacer IPC Bridge & Telemetry Connector
 */

class IPCBridge {
  constructor() {
    this.isConnected = false;
    this.targetProcess = {
      name: "Waiting for game...",
      pid: 0,
      api: "DirectX 11/12 (DXGI)",
      attached: false
    };

    this.settings = {
      targetFps: 60.0,
      pacingEnabled: true,
      queueDepthClamp: true,
      vrrMode: true,
      overlayEnabled: true,
      hookDxgi: true,
      hookVulkan: true,
      hookOpengl: true,
      hybridTiming: true
    };

    this.listeners = [];
  }

  on(event, callback) {
    this.listeners.push({ event, callback });
  }

  emit(event, data) {
    this.listeners
      .filter(l => l.event === event)
      .forEach(l => l.callback(data));
  }

  connect(url = "ws://127.0.0.1:28472") {
    try {
      this.socket = new WebSocket(url);
      
      this.socket.onopen = () => {
        this.isConnected = true;
        this.emit("status", { connected: true });
        this.syncSettings();
      };

      this.socket.onmessage = (event) => {
        try {
          const msg = JSON.parse(event.data);
          this.handleIncomingMessage(msg);
        } catch (e) {
          console.error("IPC Parse error:", e);
        }
      };

      this.socket.onclose = () => {
        this.isConnected = false;
        this.emit("status", { connected: false });
        // Attempt reconnect every 2s
        setTimeout(() => this.connect(url), 2000);
      };

      this.socket.onerror = () => {
        this.isConnected = false;
      };
    } catch (e) {
      this.isConnected = false;
    }
  }

  handleIncomingMessage(msg) {
    if (msg.type === "TELEMETRY_FRAME") {
      this.emit("frame", msg.data);
    }
  }

  send(command, payload = {}) {
    if (this.isConnected && this.socket && this.socket.readyState === WebSocket.OPEN) {
      const packet = JSON.stringify({ command, ...payload, timestamp: Date.now() });
      this.socket.send(packet);
    }
  }

  setTargetFps(fps) {
    this.settings.targetFps = fps;
    this.send("SET_TARGET_FPS", { fps });
  }

  setPacingEnabled(enabled) {
    this.settings.pacingEnabled = enabled;
    this.send("SET_PACING_ENABLED", { enabled });
  }

  setQueueDepthClamp(enabled) {
    this.settings.queueDepthClamp = enabled;
    this.send("SET_QUEUE_CLAMP", { enabled });
  }

  setHookDxgi(enabled) {
    this.settings.hookDxgi = enabled;
    this.send("SET_HOOK_DXGI", { enabled });
  }

  setHookVulkan(enabled) {
    this.settings.hookVulkan = enabled;
    this.send("SET_HOOK_VULKAN", { enabled });
  }

  setHookOpengl(enabled) {
    this.settings.hookOpengl = enabled;
    this.send("SET_HOOK_OPENGL", { enabled });
  }

  setHybridTiming(enabled) {
    this.settings.hybridTiming = enabled;
    this.send("SET_HYBRID_TIMING", { enabled });
  }

  setAutoAttach(enabled) {
    this.send("SET_AUTO_ATTACH", { enabled });
  }

  setOverlayEnabled(enabled) {
    this.settings.overlayEnabled = enabled;
    this.send("SET_OVERLAY_ENABLED", { enabled });
  }

  setOverlayPosition(position) {
    // position: 0=top-left, 1=top-right, 2=bottom-left, 3=bottom-right
    this.send("SET_OVERLAY_POSITION", { position });
  }

  setBypassedGames(exes) {
    this.send("SET_BYPASSED_GAMES", { exes });
  }

  syncSettings() {
    this.setTargetFps(this.settings.targetFps);
    this.setPacingEnabled(this.settings.pacingEnabled);
    this.setQueueDepthClamp(this.settings.queueDepthClamp);
    this.setOverlayEnabled(this.settings.overlayEnabled);
    this.setHookDxgi(this.settings.hookDxgi);
    this.setHookVulkan(this.settings.hookVulkan);
    this.setHookOpengl(this.settings.hookOpengl);
    this.setHybridTiming(this.settings.hybridTiming);
  }
}

window.IPCBridge = IPCBridge;
