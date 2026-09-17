/**
 * FramePacer - Main Frontend Application Logic
 */

document.addEventListener('DOMContentLoaded', () => {
  // Initialize Oscilloscope
  const oscilloscope = new FrametimeOscilloscope('frametime-canvas', {
    targetFps: 60,
    bufferSize: 180
  });
  oscilloscope.startLoop();

  // Initialize IPC Bridge
  const ipc = new IPCBridge();
  ipc.connect();

  // DOM Elements
  const fpsSlider = document.getElementById('fps-slider');
  const fpsInput = document.getElementById('fps-input');
  const targetFrametimeLabel = document.getElementById('target-frametime-label');
  const presetButtons = document.querySelectorAll('.preset-btn');
  const masterToggle = document.getElementById('toggle-master-pacer');
  const queueClampToggle = document.getElementById('toggle-queue-clamp');
  const vrrToggle = document.getElementById('toggle-vrr-mode');
  const overlayToggle = document.getElementById('toggle-overlay');
  const tabButtons = document.querySelectorAll('.tab-btn');
  const tabPanes = document.querySelectorAll('.tab-pane');
  const processBadge = document.getElementById('hook-status-badge');
  const processName = document.getElementById('process-name');
  const apiBadge = document.getElementById('process-api');

  // HUD & Hotkeys DOM elements
  const hudBadge = document.getElementById('hud-preview-badge');
  const hudFps = document.getElementById('hud-fps-val');
  const hudMs = document.getElementById('hud-ms-val');
  const hudJitter = document.getElementById('hud-jitter-val');
  const hudPacerStatus = document.getElementById('hud-pacer-status');
  const hudTargetProc = document.getElementById('hud-target-proc');
  const hudTargetPid = document.getElementById('hud-target-pid');
  const hudActivePosLabel = document.getElementById('hud-active-pos-label');
  const hudPosButtons = document.querySelectorAll('.hud-pos-btn');

  const posMap = { 'top-left': 0, 'top-right': 1, 'bottom-left': 2, 'bottom-right': 3 };
  const posLabels = ['Top-Left', 'Top-Right', 'Bottom-Left', 'Bottom-Right'];

  // Handle Real-Time Live Game Telemetry from Bridge
  ipc.on('frame', (data) => {
    if (data.pid > 0 && data.process && data.process.length > 0) {
      // Switch from simulation mode to live game data
      oscilloscope.simulationNoise = false;
      
      processName.textContent = data.process;
      apiBadge.textContent = `${data.api} (PID: ${data.pid})`;
      processBadge.className = 'hook-status-badge';

      if (hudTargetProc) hudTargetProc.textContent = data.process;
      if (hudTargetPid) hudTargetPid.textContent = `PID: ${data.pid} (DirectX Hook Active)`;

      if (miniProcessName) miniProcessName.textContent = data.process;
      if (miniProcessPill) {
        miniProcessPill.className = 'mini-process-pill';
        miniProcessPill.title = `${data.process} (PID: ${data.pid})`;
      }
      if (miniStatApi && data.api) miniStatApi.textContent = data.api;

      if (data.frametime > 0.01) {
        oscilloscope.pushFrametime(data.frametime);
      }
    } else {
      if (processName) processName.textContent = "Waiting for game...";
      if (apiBadge) apiBadge.textContent = "Standby";
      if (processBadge) processBadge.className = 'hook-status-badge waiting';

      if (hudTargetProc) hudTargetProc.textContent = "Standby";
      if (hudTargetPid) hudTargetPid.textContent = "Waiting for game...";

      if (miniProcessName) miniProcessName.textContent = "Waiting for game...";
      if (miniProcessPill) {
        miniProcessPill.className = 'mini-process-pill waiting';
        miniProcessPill.title = "Waiting for game...";
      }
      if (miniStatApi) miniStatApi.textContent = "Standby";
    }

    // Update HUD preview badge telemetry
    if (hudFps && data.fps !== undefined) {
      hudFps.textContent = `${data.fps.toFixed(1)} FPS`;
    }
    if (hudMs && data.frametime !== undefined) {
      hudMs.textContent = `${data.frametime.toFixed(2)} ms`;
    }
    if (hudJitter && data.jitter !== undefined) {
      if (data.jitter < 100) {
        hudJitter.textContent = `+/-${Math.round(data.jitter)} us`;
      } else {
        hudJitter.textContent = `+/-${(data.jitter / 1000).toFixed(1)} ms`;
      }
    }
    if (hudPacerStatus && data.pacingEnabled !== undefined) {
      hudPacerStatus.textContent = data.pacingEnabled ? '[LOCKED]' : '[PASSTHROUGH]';
      hudPacerStatus.style.color = data.pacingEnabled ? 'var(--emerald-accent)' : 'var(--text-muted)';
    }
  });

  // Sync FPS Value to UI, Oscilloscope, and IPC
  function updateFpsTarget(fps, updatePresetButtons = true) {
    fps = Math.max(10, Math.min(360, Math.round(fps * 10) / 10));
    const frametimeMs = (1000.0 / fps).toFixed(2);

    fpsSlider.value = fps;
    fpsInput.value = fps;
    targetFrametimeLabel.textContent = `${frametimeMs} ms / frame`;

    if (miniFpsVal) miniFpsVal.textContent = Math.round(fps);

    oscilloscope.setTargetFps(fps);
    ipc.setTargetFps(fps);

    if (updatePresetButtons) {
      presetButtons.forEach(btn => {
        const btnFps = parseFloat(btn.dataset.fps);
        if (btnFps === fps) {
          btn.classList.add('active');
        } else {
          btn.classList.remove('active');
        }
      });
    }
  }

  // Preset Buttons Click Handler
  presetButtons.forEach(btn => {
    btn.addEventListener('click', () => {
      const fps = parseFloat(btn.dataset.fps);
      if (!isNaN(fps)) {
        updateFpsTarget(fps, true);
      }
    });
  });

  // Slider Input Handler
  fpsSlider.addEventListener('input', (e) => {
    const val = parseFloat(e.target.value);
    updateFpsTarget(val, true);
  });

  // Numeric Text Input Handler
  fpsInput.addEventListener('change', (e) => {
    let val = parseFloat(e.target.value);
    if (isNaN(val)) val = 60;
    updateFpsTarget(val, true);
  });

  // Master Pacer Toggle
  masterToggle.addEventListener('change', (e) => {
    const isEnabled = e.target.checked;
    oscilloscope.setPacingState(isEnabled);
    ipc.setPacingEnabled(isEnabled);
    if (hudPacerStatus) {
      hudPacerStatus.textContent = isEnabled ? '[LOCKED]' : '[PASSTHROUGH]';
      hudPacerStatus.style.color = isEnabled ? 'var(--emerald-accent)' : 'var(--text-muted)';
    }
  });

  // Queue Depth Clamp Toggle
  if (queueClampToggle) {
    queueClampToggle.addEventListener('change', (e) => {
      ipc.setQueueDepthClamp(e.target.checked);
    });
  }

  // Auto-Attach Toggle
  const autoAttachToggle = document.getElementById('toggle-auto-attach');
  if (autoAttachToggle) {
    autoAttachToggle.addEventListener('change', (e) => {
      ipc.setAutoAttach(e.target.checked);
    });
  }

  // VRR Mode Toggle
  if (vrrToggle) {
    vrrToggle.addEventListener('change', (e) => {
      if (e.target.checked) {
        updateFpsTarget(141, true);
      }
    });
  }

  // In-Game Overlay Toggle
  if (overlayToggle) {
    overlayToggle.addEventListener('change', (e) => {
      const isEnabled = e.target.checked;
      ipc.setOverlayEnabled(isEnabled);
      if (hudBadge) {
        hudBadge.style.opacity = isEnabled ? '1' : '0.2';
      }
    });
  }

  // HUD Placement Position Buttons
  hudPosButtons.forEach(btn => {
    btn.addEventListener('click', () => {
      hudPosButtons.forEach(b => b.classList.remove('active'));
      btn.classList.add('active');

      const posKey = btn.dataset.pos;
      const posIndex = posMap[posKey] !== undefined ? posMap[posKey] : 0;

      if (hudBadge) {
        hudBadge.className = `hud-preview-badge pos-${posKey}`;
      }
      if (hudActivePosLabel) {
        hudActivePosLabel.textContent = posLabels[posIndex];
      }

      ipc.setOverlayPosition(posIndex);
    });
  });

  // Tab Navigation
  tabButtons.forEach(btn => {
    btn.addEventListener('click', () => {
      const targetTab = btn.dataset.tab;
      
      tabButtons.forEach(b => b.classList.remove('active'));
      tabPanes.forEach(p => p.style.display = 'none');

      btn.classList.add('active');
      const activePane = document.getElementById(`tab-${targetTab}`);
      if (activePane) {
        activePane.style.display = 'grid';
      }
    });
  });

  // Window Control Buttons (Electron Desktop App)
  const windowFrame = document.querySelector('.window-frame');
  const btnMiniMode = document.getElementById('btn-mini-mode');
  const miniProcessPill = document.getElementById('mini-process-pill');
  const miniProcessName = document.getElementById('mini-process-name');
  const miniFpsVal = document.getElementById('mini-fps-val');
  const miniCapBox = document.getElementById('mini-cap-box');
  const miniStatApi = document.getElementById('mini-stat-api');
  const btnPin = document.getElementById('btn-pin');
  const btnMinimize = document.getElementById('btn-minimize');
  const btnClose = document.getElementById('btn-close');

  function setMiniModeUI(isMini) {
    if (!windowFrame) return;
    if (isMini) {
      windowFrame.classList.add('mini-mode');
      if (btnMiniMode) {
        btnMiniMode.title = 'Expand to Full Dashboard';
        btnMiniMode.innerHTML = `
          <svg id="icon-mini-mode" width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5">
            <polyline points="6 9 12 15 18 9"></polyline>
          </svg>
        `;
      }
    } else {
      windowFrame.classList.remove('mini-mode');
      if (btnMiniMode) {
        btnMiniMode.title = 'Toggle Mini Mode (Compact View)';
        btnMiniMode.innerHTML = `
          <svg id="icon-mini-mode" width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5">
            <polyline points="18 15 12 9 6 15"></polyline>
          </svg>
        `;
      }
    }
    setTimeout(() => oscilloscope.resizeCanvas(), 50);
  }

  if (btnMiniMode) {
    btnMiniMode.addEventListener('click', async () => {
      if (window.electronAPI?.toggleMiniMode) {
        const isMini = await window.electronAPI.toggleMiniMode();
        setMiniModeUI(isMini);
      } else {
        const isMini = !windowFrame.classList.contains('mini-mode');
        setMiniModeUI(isMini);
      }
    });
  }

  if (miniCapBox) {
    const commonPresets = [30, 40, 60, 90, 120, 141, 144, 240];
    miniCapBox.addEventListener('click', () => {
      const current = parseFloat(fpsInput.value) || 60;
      const next = commonPresets.find(p => p > current) || commonPresets[0];
      updateFpsTarget(next);
    });
  }

  const btnSettings = document.getElementById('btn-settings');
  if (btnSettings) {
    btnSettings.addEventListener('click', () => {
      if (windowFrame?.classList.contains('mini-mode')) {
        btnMiniMode?.click();
      }
      const engineTabBtn = document.querySelector('.tab-btn[data-tab="engine"]');
      if (engineTabBtn) engineTabBtn.click();
    });
  }

  if (window.electronAPI) {
    // Check initial pin & mini mode states
    window.electronAPI.getPinState?.().then(pinned => {
      if (btnPin && pinned) btnPin.classList.add('pinned');
    });

    window.electronAPI.getMiniMode?.().then(isMini => {
      if (isMini) setMiniModeUI(true);
    });

    window.electronAPI.onMiniModeChanged?.((isMini) => {
      setMiniModeUI(isMini);
    });

    window.electronAPI.onPinStateChanged?.((pinned) => {
      if (btnPin) {
        if (pinned) {
          btnPin.classList.add('pinned');
        } else {
          btnPin.classList.remove('pinned');
        }
      }
    });

    if (btnPin) {
      btnPin.addEventListener('click', async () => {
        const isPinned = await window.electronAPI.togglePin();
        if (isPinned) {
          btnPin.classList.add('pinned');
        } else {
          btnPin.classList.remove('pinned');
        }
      });
    }

    if (btnMinimize) {
      btnMinimize.addEventListener('click', () => {
        window.electronAPI.minimizeWindow();
      });
    }

    if (btnClose) {
      btnClose.addEventListener('click', () => {
        window.electronAPI.closeWindow();
      });
    }
  } else {
    // In plain browser mode, pin button toggles visual indicator
    if (btnPin) {
      btnPin.addEventListener('click', () => {
        btnPin.classList.toggle('pinned');
      });
    }
  }

  // Global Hotkey Listener in Web UI (e.g. Ctrl + Shift + P to toggle pacer)
  window.addEventListener('keydown', (e) => {
    if (e.ctrlKey && e.shiftKey && e.code === 'KeyP') {
      masterToggle.checked = !masterToggle.checked;
      masterToggle.dispatchEvent(new Event('change'));
    } else if (e.ctrlKey && e.shiftKey && e.code === 'ArrowUp') {
      const current = parseFloat(fpsInput.value) || 60;
      updateFpsTarget(current + 1);
    } else if (e.ctrlKey && e.shiftKey && e.code === 'ArrowDown') {
      const current = parseFloat(fpsInput.value) || 60;
      updateFpsTarget(current - 1);
    }
  });

  // Initial target setup
  updateFpsTarget(60, true);
});
