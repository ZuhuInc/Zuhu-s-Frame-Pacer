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

  // Helper for human-readable timestamps
  function timeAgo(dateString) {
    if (!dateString) return 'Never';
    const now = new Date();
    const past = new Date(dateString);
    const diffSec = Math.floor((now - past) / 1000);
    if (diffSec < 60) return 'Just now';
    const diffMin = Math.floor(diffSec / 60);
    if (diffMin < 60) return `${diffMin}m ago`;
    const diffHours = Math.floor(diffMin / 60);
    if (diffHours < 24) return `${diffHours}h ago`;
    const diffDays = Math.floor(diffHours / 24);
    if (diffDays === 1) return 'Yesterday';
    if (diffDays < 30) return `${diffDays}d ago`;
    return past.toLocaleDateString();
  }

  // =========================================================================
  // Game Profiles & Library System
  // =========================================================================
  class ProfileManager {
    constructor() {
      this.globalDefault = {
        fpsCap: 60,
        pacingEnabled: true,
        latencyClamp: true
      };
      this.profiles = {};
      this.activeExe = null;
      this.activeName = null;
      this.activeProfilePill = document.getElementById('active-profile-pill');
      this.activeProfileName = document.getElementById('active-profile-name');
      this.profileCount = document.getElementById('profile-count');
      this.profilesListContainer = document.getElementById('profiles-list-container');
      this.profileSearchInput = document.getElementById('profile-search-input');
      this.btnAddGameManual = document.getElementById('btn-add-game-manual');

      // Global Baseline DOM elements
      this.globalDefaultFpsInput = document.getElementById('global-default-fps-input');
      this.globalDefaultFpsSlider = document.getElementById('global-default-fps-slider');
      this.globalDefaultPacerToggle = document.getElementById('global-default-pacer-toggle');
      this.globalDefaultClampToggle = document.getElementById('global-default-clamp-toggle');
      this.btnSaveGlobalDefault = document.getElementById('btn-save-global-default');
      this.btnSyncActiveGame = document.getElementById('btn-sync-active-game');
      this.btnSavePreset = document.getElementById('btn-save-preset');

      this.initEvents();
    }

    async load() {
      try {
        let data = null;
        if (window.electronAPI?.loadProfiles) {
          data = await window.electronAPI.loadProfiles();
        }
        if (!data) {
          const raw = localStorage.getItem('framepacer_profiles_v1');
          if (raw) data = JSON.parse(raw);
        }
        if (data) {
          if (data.globalDefault) this.globalDefault = { ...this.globalDefault, ...data.globalDefault };
          if (data.profiles) this.profiles = data.profiles;
        }
      } catch (e) {
        console.error('[ProfileManager] Failed to load profiles:', e);
      }

      this.syncGlobalDefaultUI();
      this.renderList();
    }

    async save() {
      const data = {
        globalDefault: this.globalDefault,
        profiles: this.profiles
      };
      try {
        localStorage.setItem('framepacer_profiles_v1', JSON.stringify(data));
        if (window.electronAPI?.saveProfiles) {
          await window.electronAPI.saveProfiles(data);
        }
      } catch (e) {
        console.error('[ProfileManager] Failed to save profiles:', e);
      }
    }

    syncGlobalDefaultUI() {
      if (this.globalDefaultFpsInput) this.globalDefaultFpsInput.value = this.globalDefault.fpsCap;
      if (this.globalDefaultFpsSlider) this.globalDefaultFpsSlider.value = this.globalDefault.fpsCap;
      if (this.globalDefaultPacerToggle) this.globalDefaultPacerToggle.checked = this.globalDefault.pacingEnabled;
      if (this.globalDefaultClampToggle) this.globalDefaultClampToggle.checked = this.globalDefault.latencyClamp;
    }

    initEvents() {
      // Global Default Slider & Numeric Input sync
      if (this.globalDefaultFpsSlider) {
        this.globalDefaultFpsSlider.addEventListener('input', (e) => {
          const val = parseFloat(e.target.value) || 60;
          if (this.globalDefaultFpsInput) this.globalDefaultFpsInput.value = val;
          this.globalDefault.fpsCap = val;
        });
      }

      if (this.globalDefaultFpsInput) {
        this.globalDefaultFpsInput.addEventListener('change', (e) => {
          let val = parseFloat(e.target.value) || 60;
          val = Math.max(10, Math.min(360, val));
          e.target.value = val;
          if (this.globalDefaultFpsSlider) this.globalDefaultFpsSlider.value = Math.min(240, val);
          this.globalDefault.fpsCap = val;
        });
      }

      if (this.globalDefaultPacerToggle) {
        this.globalDefaultPacerToggle.addEventListener('change', (e) => {
          this.globalDefault.pacingEnabled = e.target.checked;
        });
      }

      if (this.globalDefaultClampToggle) {
        this.globalDefaultClampToggle.addEventListener('change', (e) => {
          this.globalDefault.latencyClamp = e.target.checked;
        });
      }

      if (this.btnSaveGlobalDefault) {
        this.btnSaveGlobalDefault.addEventListener('click', async () => {
          await this.save();
          const origText = this.btnSaveGlobalDefault.innerHTML;
          this.btnSaveGlobalDefault.textContent = 'Saved Baseline!';
          setTimeout(() => { this.btnSaveGlobalDefault.innerHTML = origText; }, 1200);
        });
      }

      if (this.btnSyncActiveGame) {
        this.btnSyncActiveGame.addEventListener('click', () => {
          if (this.activeExe && this.profiles[this.activeExe]) {
            const p = this.profiles[this.activeExe];
            this.globalDefault.fpsCap = p.fpsCap;
            this.globalDefault.pacingEnabled = p.enabled;
            this.globalDefault.latencyClamp = p.latencyClamp;
            this.syncGlobalDefaultUI();
            this.save();
          } else {
            const currentFps = parseFloat(fpsInput.value) || 60;
            this.globalDefault.fpsCap = currentFps;
            this.globalDefault.pacingEnabled = masterToggle.checked;
            this.globalDefault.latencyClamp = queueClampToggle ? queueClampToggle.checked : true;
            this.syncGlobalDefaultUI();
            this.save();
          }
        });
      }

      // Save Game Profile button in Tab 1
      if (this.btnSavePreset) {
        this.btnSavePreset.addEventListener('click', async () => {
          const currentFps = parseFloat(fpsInput.value) || 60;
          const currentPacing = masterToggle.checked;
          const currentClamp = queueClampToggle ? queueClampToggle.checked : true;

          if (this.activeExe) {
            this.profiles[this.activeExe] = {
              name: this.activeName || this.activeExe,
              exe: this.activeExe,
              fpsCap: currentFps,
              enabled: currentPacing,
              latencyClamp: currentClamp,
              lastPlayed: new Date().toISOString()
            };
          } else {
            // If no active game, save as custom entry or update global default
            this.globalDefault.fpsCap = currentFps;
            this.globalDefault.pacingEnabled = currentPacing;
            this.globalDefault.latencyClamp = currentClamp;
            this.syncGlobalDefaultUI();
          }

          await this.save();
          this.renderList();
          const origHTML = this.btnSavePreset.innerHTML;
          this.btnSavePreset.textContent = 'Profile Saved!';
          setTimeout(() => { this.btnSavePreset.innerHTML = origHTML; }, 1200);
        });
      }

      // Add Game Manually (.exe)
      if (this.btnAddGameManual) {
        this.btnAddGameManual.addEventListener('click', async () => {
          if (window.electronAPI?.selectGameExe) {
            const res = await window.electronAPI.selectGameExe();
            if (res && res.exe) {
              const exe = res.exe.toLowerCase();
              if (!this.profiles[exe]) {
                this.profiles[exe] = {
                  name: res.name || exe,
                  exe: exe,
                  fpsCap: this.globalDefault.fpsCap,
                  enabled: this.globalDefault.pacingEnabled,
                  latencyClamp: this.globalDefault.latencyClamp,
                  lastPlayed: new Date().toISOString()
                };
              } else {
                this.profiles[exe].lastPlayed = new Date().toISOString();
              }
              await this.save();
              this.renderList();
            }
          } else {
            const exeInput = prompt("Enter Game Executable Name (e.g. game.exe):");
            if (exeInput && exeInput.trim().length > 0) {
              const exe = exeInput.trim().toLowerCase();
              let cleanName = exe.replace(/\.exe$/i, '').replace(/[-_]/g, ' ').replace(/\b\w/g, c => c.toUpperCase());
              this.profiles[exe] = {
                name: cleanName,
                exe: exe,
                fpsCap: this.globalDefault.fpsCap,
                enabled: this.globalDefault.pacingEnabled,
                latencyClamp: this.globalDefault.latencyClamp,
                lastPlayed: new Date().toISOString()
              };
              await this.save();
              this.renderList();
            }
          }
        });
      }

      // Search filter
      if (this.profileSearchInput) {
        this.profileSearchInput.addEventListener('input', (e) => {
          this.renderList(e.target.value.trim().toLowerCase());
        });
      }
    }

    onGameDetected(processExe, rawProcessName) {
      if (!processExe) {
        this.activeExe = null;
        this.activeName = null;
        if (this.activeProfileName) this.activeProfileName.textContent = 'Global Default';
        if (this.activeProfilePill) {
          this.activeProfilePill.title = 'Global Default Baseline Active';
        }
        this.renderList(this.profileSearchInput ? this.profileSearchInput.value.trim().toLowerCase() : '');
        return;
      }

      const exe = processExe.toLowerCase();
      const isNewGame = (this.activeExe !== exe);
      this.activeExe = exe;

      // Extract clean title
      let cleanTitle = rawProcessName || exe;
      if (cleanTitle.toLowerCase().endsWith('.exe')) {
        cleanTitle = cleanTitle.substring(0, cleanTitle.length - 4);
      }
      cleanTitle = cleanTitle.replace(/[-_]/g, ' ').replace(/\b\w/g, c => c.toUpperCase());
      this.activeName = cleanTitle;

      if (!this.profiles[exe]) {
        // Auto-register new game with Global Default Baseline
        this.profiles[exe] = {
          name: cleanTitle,
          exe: exe,
          fpsCap: this.globalDefault.fpsCap,
          enabled: this.globalDefault.pacingEnabled,
          latencyClamp: this.globalDefault.latencyClamp,
          lastPlayed: new Date().toISOString()
        };
        this.save();
      } else {
        // Update last played timestamp
        this.profiles[exe].lastPlayed = new Date().toISOString();
        if (!this.profiles[exe].name || this.profiles[exe].name === exe) {
          this.profiles[exe].name = cleanTitle;
        }
        this.save();
      }

      const profile = this.profiles[exe];

      // If switching to this game for first time in session, apply profile settings
      if (isNewGame) {
        if (!profile.enabled) {
          // Bypass mode: unconstrained framerate
          masterToggle.checked = false;
          oscilloscope.setPacingState(false);
          ipc.setPacingEnabled(false);
        } else {
          masterToggle.checked = true;
          oscilloscope.setPacingState(true);
          ipc.setPacingEnabled(true);
          updateFpsTarget(profile.fpsCap, true);
          if (queueClampToggle) {
            queueClampToggle.checked = profile.latencyClamp;
            ipc.setQueueDepthClamp(profile.latencyClamp);
          }
        }
      }

      // Update Header Pill
      if (this.activeProfileName) {
        if (!profile.enabled) {
          this.activeProfileName.textContent = `${profile.name} [BYPASS]`;
        } else {
          this.activeProfileName.textContent = `${profile.name} [${Math.round(profile.fpsCap)} FPS]`;
        }
      }
      if (this.activeProfilePill) {
        this.activeProfilePill.title = `Profile: ${profile.name} (${profile.exe}) - ${profile.enabled ? `${profile.fpsCap} FPS Cap` : 'Bypassed'}`;
      }

      this.renderList(this.profileSearchInput ? this.profileSearchInput.value.trim().toLowerCase() : '');
    }

    updateActiveGameFps(newFps) {
      if (this.activeExe && this.profiles[this.activeExe]) {
        this.profiles[this.activeExe].fpsCap = newFps;
        this.save();
        if (this.activeProfileName && this.profiles[this.activeExe].enabled) {
          this.activeProfileName.textContent = `${this.profiles[this.activeExe].name} [${Math.round(newFps)} FPS]`;
        }
        this.renderList(this.profileSearchInput ? this.profileSearchInput.value.trim().toLowerCase() : '');
      }
    }

    renderList(filterText = '') {
      if (!this.profilesListContainer) return;

      const profileKeys = Object.keys(this.profiles);
      if (this.profileCount) this.profileCount.textContent = profileKeys.length;

      // Filter
      let filtered = profileKeys.map(k => this.profiles[k]);
      if (filterText) {
        filtered = filtered.filter(p => 
          (p.name && p.name.toLowerCase().includes(filterText)) ||
          (p.exe && p.exe.toLowerCase().includes(filterText))
        );
      }

      // Sort by lastPlayed descending
      filtered.sort((a, b) => {
        const tA = a.lastPlayed ? new Date(a.lastPlayed).getTime() : 0;
        const tB = b.lastPlayed ? new Date(b.lastPlayed).getTime() : 0;
        return tB - tA;
      });

      if (filtered.length === 0) {
        this.profilesListContainer.innerHTML = `
          <div class="profiles-empty-state">
            <svg width="32" height="32" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.5">
              <path d="M4 19.5A2.5 2.5 0 0 1 6.5 17H20"></path>
              <path d="M6.5 2H20v20H6.5A2.5 2.5 0 0 1 4 19.5v-15A2.5 2.5 0 0 1 6.5 2z"></path>
            </svg>
            <p>${filterText ? 'No matching games found.' : 'No game profiles tracked yet. Play a game or click "+ Add Game" to customize.'}</p>
          </div>
        `;
        return;
      }

      this.profilesListContainer.innerHTML = '';

      filtered.forEach(profile => {
        const isCurrentRunning = (this.activeExe === profile.exe);
        const card = document.createElement('div');
        card.className = `profile-item-card ${isCurrentRunning ? 'active-running' : ''} ${!profile.enabled ? 'bypassed' : ''}`;

        const letter = (profile.name && profile.name.length > 0) ? profile.name.charAt(0).toUpperCase() : 'G';
        const formattedTime = timeAgo(profile.lastPlayed);

        card.innerHTML = `
          <div class="profile-item-left">
            <div class="profile-avatar">${letter}</div>
            <div class="profile-meta">
              <div class="profile-title-row">
                <span class="profile-name" title="${profile.name}">${profile.name}</span>
                ${isCurrentRunning ? '<span class="profile-active-tag">PLAYING</span>' : ''}
              </div>
              <div class="profile-subtext">
                <span>${profile.exe}</span>
                <span>&bull;</span>
                <span>${formattedTime}</span>
              </div>
            </div>
          </div>

          <div class="profile-item-right">
            <!-- Inline Editable FPS Pill -->
            <div class="profile-fps-pill" title="Click to edit Target FPS" data-exe="${profile.exe}">
              <span class="pill-fps-text">${profile.enabled ? `${Math.round(profile.fpsCap)} FPS` : 'Bypassed'}</span>
              <input type="number" class="profile-fps-inline-input" value="${Math.round(profile.fpsCap)}" min="10" max="360" step="1" style="display: none;" />
            </div>

            <!-- On/Off Switch -->
            <label class="switch" title="${profile.enabled ? 'Pacing Active' : 'Pacing Bypassed (Uncapped)'}">
              <input type="checkbox" class="profile-toggle-enabled" data-exe="${profile.exe}" ${profile.enabled ? 'checked' : ''}>
              <span class="switch-slider"></span>
            </label>

            <!-- Delete Custom Override Button -->
            <button class="btn-profile-delete" title="Remove custom profile (Revert to default)" data-exe="${profile.exe}">
              <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
                <polyline points="3 6 5 6 21 6"></polyline>
                <path d="M19 6v14a2 2 0 0 1-2 2H7a2 2 0 0 1-2-2V6m3 0V4a2 2 0 0 1 2-2h4a2 2 0 0 1 2 2v2"></path>
              </svg>
            </button>
          </div>
        `;

        // Attach event listeners for this card
        const toggle = card.querySelector('.profile-toggle-enabled');
        if (toggle) {
          toggle.addEventListener('change', (e) => {
            const isChecked = e.target.checked;
            profile.enabled = isChecked;
            this.save();
            if (isCurrentRunning) {
              masterToggle.checked = isChecked;
              oscilloscope.setPacingState(isChecked);
              ipc.setPacingEnabled(isChecked);
              if (this.activeProfileName) {
                this.activeProfileName.textContent = isChecked 
                  ? `${profile.name} [${Math.round(profile.fpsCap)} FPS]` 
                  : `${profile.name} [BYPASS]`;
              }
            }
            this.renderList(this.profileSearchInput ? this.profileSearchInput.value.trim().toLowerCase() : '');
          });
        }

        const deleteBtn = card.querySelector('.btn-profile-delete');
        if (deleteBtn) {
          deleteBtn.addEventListener('click', () => {
            delete this.profiles[profile.exe];
            this.save();
            if (isCurrentRunning) {
              // Revert running game to global baseline
              updateFpsTarget(this.globalDefault.fpsCap);
              masterToggle.checked = this.globalDefault.pacingEnabled;
              oscilloscope.setPacingState(this.globalDefault.pacingEnabled);
              ipc.setPacingEnabled(this.globalDefault.pacingEnabled);
              if (this.activeProfileName) {
                this.activeProfileName.textContent = `${profile.name} [${Math.round(this.globalDefault.fpsCap)} FPS]`;
              }
            }
            this.renderList(this.profileSearchInput ? this.profileSearchInput.value.trim().toLowerCase() : '');
          });
        }

        // Inline FPS Pill Click-to-Edit
        const pill = card.querySelector('.profile-fps-pill');
        const pillText = card.querySelector('.pill-fps-text');
        const pillInput = card.querySelector('.profile-fps-inline-input');

        if (pill && pillText && pillInput) {
          pill.addEventListener('click', (e) => {
            if (e.target === pillInput) return;
            pillText.style.display = 'none';
            pillInput.style.display = 'inline-block';
            pillInput.value = Math.round(profile.fpsCap);
            pillInput.focus();
            pillInput.select();
          });

          const commitPillInput = () => {
            let val = parseFloat(pillInput.value);
            if (!isNaN(val) && val >= 10 && val <= 360) {
              profile.fpsCap = val;
              this.save();
              if (isCurrentRunning && profile.enabled) {
                updateFpsTarget(val);
              }
            }
            pillInput.style.display = 'none';
            pillText.style.display = 'inline-block';
            pillText.textContent = profile.enabled ? `${Math.round(profile.fpsCap)} FPS` : 'Bypassed';
          };

          pillInput.addEventListener('keydown', (e) => {
            if (e.key === 'Enter') {
              commitPillInput();
            } else if (e.key === 'Escape') {
              pillInput.style.display = 'none';
              pillText.style.display = 'inline-block';
            }
          });

          pillInput.addEventListener('blur', () => {
            commitPillInput();
          });
        }

        this.profilesListContainer.appendChild(card);
      });
    }
  }

  const profileManager = new ProfileManager();
  profileManager.load();

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

      // Notify profile manager of detected game
      profileManager.onGameDetected(data.process, data.process);

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

      profileManager.onGameDetected(null, null);
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

  // Sync FPS Value to UI, Oscilloscope, IPC, and Active Profile
  function updateFpsTarget(fps, updatePresetButtons = true) {
    fps = Math.max(10, Math.min(360, Math.round(fps * 10) / 10));
    const frametimeMs = (1000.0 / fps).toFixed(2);

    fpsSlider.value = fps;
    fpsInput.value = fps;
    targetFrametimeLabel.textContent = `${frametimeMs} ms / frame`;

    if (miniFpsVal) miniFpsVal.textContent = Math.round(fps);
    if (miniCapInput) miniCapInput.value = Math.round(fps);

    oscilloscope.setTargetFps(fps);
    ipc.setTargetFps(fps);

    profileManager.updateActiveGameFps(fps);

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
    if (profileManager.activeExe && profileManager.profiles[profileManager.activeExe]) {
      profileManager.profiles[profileManager.activeExe].enabled = isEnabled;
      profileManager.save();
      profileManager.renderList(profileManager.profileSearchInput ? profileManager.profileSearchInput.value.trim().toLowerCase() : '');
    }
  });

  // Queue Depth Clamp Toggle
  if (queueClampToggle) {
    queueClampToggle.addEventListener('change', (e) => {
      const isChecked = e.target.checked;
      ipc.setQueueDepthClamp(isChecked);
      if (profileManager.activeExe && profileManager.profiles[profileManager.activeExe]) {
        profileManager.profiles[profileManager.activeExe].latencyClamp = isChecked;
        profileManager.save();
      }
    });
  }

  // Auto-Attach Toggle
  const autoAttachToggle = document.getElementById('toggle-auto-attach');
  if (autoAttachToggle) {
    autoAttachToggle.addEventListener('change', (e) => {
      ipc.setAutoAttach(e.target.checked);
    });
  }

  // =========================================================================
  // Display & Adaptive-Sync / VRR Engine
  // =========================================================================
  class DisplayManager {
    constructor() {
      this.currentDisplay = {
        name: 'Primary Gaming Display',
        resolution: '1920x1080',
        refreshRate: 144,
        vrrCap: 141,
        halfRate: 72,
        thirdRate: 48
      };

      this.displayNameLabel = document.getElementById('display-name-label');
      this.displayResolutionLabel = document.getElementById('display-resolution-label');
      this.displayHzVal = document.getElementById('display-hz-val');
      this.displayVrrCapVal = document.getElementById('display-vrr-cap-val');
      this.labelHalfRate = document.getElementById('label-half-rate');
      this.labelThirdRate = document.getElementById('label-third-rate');
      this.btnCalibrateVrr = document.getElementById('btn-calibrate-vrr');
      this.btnApplyVrrSync = document.getElementById('btn-apply-vrr-sync');
      this.btnLockHalfVrr = document.getElementById('btn-lock-half-vrr');
      this.btnLockThirdVrr = document.getElementById('btn-lock-third-vrr');

      this.init();
    }

    async init() {
      if (window.electronAPI?.getDisplayInfo) {
        try {
          const info = await window.electronAPI.getDisplayInfo();
          if (info && info.primary) {
            this.updateDisplayData(info.primary);
          }
        } catch (e) {
          console.error('[DisplayManager] Failed to get display info:', e);
        }

        window.electronAPI.onDisplayChanged?.((info) => {
          if (info && info.primary) {
            this.updateDisplayData(info.primary);
          }
        });
      } else {
        // Fallback for browser testing
        this.updateDisplayData({
          name: 'LG UltraGear (Adaptive-Sync Primary)',
          resolution: `${window.screen.width}x${window.screen.height}`,
          refreshRate: 165,
          vrrCap: 161,
          halfRate: 82,
          thirdRate: 55
        });
      }

      this.initEvents();
    }

    updateDisplayData(data) {
      this.currentDisplay = { ...this.currentDisplay, ...data };
      const { name, resolution, refreshRate, vrrCap, halfRate, thirdRate } = this.currentDisplay;

      if (this.displayNameLabel) this.displayNameLabel.textContent = name;
      if (this.displayResolutionLabel) this.displayResolutionLabel.textContent = `${resolution} &bull; Active Output`;
      if (this.displayHzVal) this.displayHzVal.textContent = `${refreshRate} Hz`;
      if (this.displayVrrCapVal) this.displayVrrCapVal.textContent = `${vrrCap} FPS`;
      if (this.labelHalfRate) this.labelHalfRate.textContent = `${halfRate} FPS`;
      if (this.labelThirdRate) this.labelThirdRate.textContent = `${thirdRate} FPS`;

      if (this.btnCalibrateVrr) {
        this.btnCalibrateVrr.innerHTML = `
          <svg width="15" height="15" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
            <circle cx="12" cy="12" r="10"></circle>
            <polyline points="12 6 12 12 14 14"></polyline>
          </svg>
          Auto VRR (${vrrCap} FPS)
        `;
        this.btnCalibrateVrr.title = `Calibrate to detected ${refreshRate}Hz display (Optimal Adaptive-Sync cap: ${vrrCap} FPS)`;
      }

      // Update 6th Preset Button in Grid
      const vrrPresetBtn = document.getElementById('preset-vrr-btn');
      const vrrPresetFps = document.getElementById('preset-vrr-fps');
      const vrrPresetMs = document.getElementById('preset-vrr-ms');
      const vrrPresetTitle = document.getElementById('preset-vrr-title');

      if (vrrPresetBtn && vrrPresetFps && vrrPresetMs) {
        vrrPresetBtn.dataset.fps = vrrCap;
        vrrPresetFps.textContent = Math.round(vrrCap);
        vrrPresetMs.textContent = `${(1000.0 / vrrCap).toFixed(2)} ms`;
        if (vrrPresetTitle) vrrPresetTitle.textContent = `VRR (${refreshRate}Hz)`;
      }
    }

    initEvents() {
      const applyVrr = (btn) => {
        const cap = this.currentDisplay.vrrCap || 141;
        updateFpsTarget(cap, true);
        if (btn) {
          const orig = btn.innerHTML;
          btn.textContent = `Calibrated (${cap} FPS)!`;
          setTimeout(() => { btn.innerHTML = orig; }, 1200);
        }
      };

      if (this.btnCalibrateVrr) {
        this.btnCalibrateVrr.addEventListener('click', () => applyVrr(this.btnCalibrateVrr));
      }

      if (this.btnApplyVrrSync) {
        this.btnApplyVrrSync.addEventListener('click', () => applyVrr(this.btnApplyVrrSync));
      }

      if (this.btnLockHalfVrr) {
        this.btnLockHalfVrr.addEventListener('click', () => {
          const half = this.currentDisplay.halfRate || 72;
          updateFpsTarget(half, true);
        });
      }

      if (this.btnLockThirdVrr) {
        this.btnLockThirdVrr.addEventListener('click', () => {
          const third = this.currentDisplay.thirdRate || 48;
          updateFpsTarget(third, true);
        });
      }
    }
  }

  const displayManager = new DisplayManager();

  // VRR Mode Toggle
  if (vrrToggle) {
    vrrToggle.addEventListener('change', (e) => {
      if (e.target.checked) {
        const cap = displayManager.currentDisplay.vrrCap || 141;
        updateFpsTarget(cap, true);
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
  const miniCapInput = document.getElementById('mini-cap-input');
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

  // Mini Mode Direct Click-to-Type FPS Input Handler
  if (miniCapBox && miniFpsVal && miniCapInput) {
    miniCapBox.addEventListener('click', (e) => {
      if (e.target === miniCapInput) return;
      miniFpsVal.style.display = 'none';
      miniCapInput.style.display = 'block';
      miniCapInput.value = Math.round(parseFloat(fpsInput.value) || 60);
      miniCapInput.focus();
      miniCapInput.select();
    });

    const commitMiniInput = () => {
      let val = parseFloat(miniCapInput.value);
      if (!isNaN(val) && val >= 10 && val <= 360) {
        updateFpsTarget(val);
      }
      miniCapInput.style.display = 'none';
      miniFpsVal.style.display = 'block';
      miniFpsVal.textContent = Math.round(parseFloat(fpsInput.value) || 60);
    };

    miniCapInput.addEventListener('keydown', (e) => {
      if (e.key === 'Enter') {
        commitMiniInput();
      } else if (e.key === 'Escape') {
        miniCapInput.style.display = 'none';
        miniFpsVal.style.display = 'block';
      }
    });

    miniCapInput.addEventListener('blur', () => {
      commitMiniInput();
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
