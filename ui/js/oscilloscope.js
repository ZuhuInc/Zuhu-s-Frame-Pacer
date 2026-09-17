/**
 * FramePacer - High-Performance Real-Time Frametime Oscilloscope
 * Renders smooth 60/120 FPS timeline and computes variance & cadence statistics.
 */

class FrametimeOscilloscope {
  constructor(canvasId, options = {}) {
    this.canvas = document.getElementById(canvasId);
    if (!this.canvas) return;
    this.ctx = this.canvas.getContext('2d');
    
    this.bufferSize = options.bufferSize || 180;
    this.frametimes = new Array(this.bufferSize).fill(16.667);
    this.targetFps = options.targetFps || 60;
    this.targetFrametime = 1000 / this.targetFps;
    
    this.maxScaleMs = options.maxScaleMs || 40.0;
    this.isPacingActive = true;
    this.simulationNoise = true;
    
    // Telemetry DOM elements
    this.fpsElem = document.getElementById('metric-fps');
    this.frametimeElem = document.getElementById('metric-frametime');
    this.jitterElem = document.getElementById('metric-jitter');
    this.low1Elem = document.getElementById('metric-1low');
    this.cadenceStatusElem = document.getElementById('cadence-status');

    // Mini Mode Elements
    this.miniCanvas = document.getElementById('mini-frametime-canvas');
    this.miniCtx = this.miniCanvas ? this.miniCanvas.getContext('2d') : null;
    this.miniFpsElem = document.getElementById('mini-stat-fps');
    this.miniFtElem = document.getElementById('mini-stat-ft');
    this.miniJitterElem = document.getElementById('mini-stat-jitter');
    this.miniFooterFt = document.getElementById('mini-footer-ft');

    this.resizeCanvas();
    window.addEventListener('resize', () => this.resizeCanvas());

    this.lastFrameTime = performance.now();
    this.animationFrameId = null;
  }

  resizeCanvas() {
    const dpr = window.devicePixelRatio || 1;
    if (this.canvas) {
      const rect = this.canvas.getBoundingClientRect();
      if (rect.width > 0 && rect.height > 0) {
        this.canvas.width = rect.width * dpr;
        this.canvas.height = rect.height * dpr;
        this.ctx.scale(dpr, dpr);
        this.width = rect.width;
        this.height = rect.height;
      }
    }
    if (this.miniCanvas) {
      const rect = this.miniCanvas.getBoundingClientRect();
      if (rect.width > 0 && rect.height > 0) {
        this.miniCanvas.width = rect.width * dpr;
        this.miniCanvas.height = rect.height * dpr;
        this.miniCtx.scale(dpr, dpr);
        this.miniWidth = rect.width;
        this.miniHeight = rect.height;
      }
    }
  }

  setTargetFps(fps) {
    this.targetFps = Math.max(10, Math.min(fps, 360));
    this.targetFrametime = 1000.0 / this.targetFps;
    
    // Auto-scale canvas height to fit target comfortable in view
    if (this.targetFrametime > 28) {
      this.maxScaleMs = 50.0;
    } else if (this.targetFrametime > 18) {
      this.maxScaleMs = 35.0;
    } else {
      this.maxScaleMs = 25.0;
    }
  }

  setPacingState(enabled) {
    this.isPacingActive = enabled;
  }

  pushFrametime(ms) {
    this.frametimes.push(ms);
    if (this.frametimes.length > this.bufferSize) {
      this.frametimes.shift();
    }
  }

  // Generate synthetic telemetry for testing / demo when live game is not yet hooked
  generateSimulatedFrame() {
    let frameMs;
    if (this.isPacingActive) {
      // Sub-millisecond precision jitter (< 0.04 ms standard deviation)
      const microJitter = (Math.random() - 0.5) * 0.06;
      frameMs = this.targetFrametime + microJitter;
    } else {
      // Unpaced engine jitter: wild swings between fast frames and late frames
      const baseWorkload = this.targetFrametime;
      const engineSpike = (Math.random() > 0.88) ? (Math.random() * 12.0) : (Math.random() * 4.0 - 2.0);
      frameMs = Math.max(4.0, baseWorkload + engineSpike);
    }
    this.pushFrametime(frameMs);
  }

  updateMetrics() {
    if (!this.frametimes.length) return;
    
    const count = this.frametimes.length;
    const recentSlice = this.frametimes.slice(-60);
    const sum = recentSlice.reduce((a, b) => a + b, 0);
    const avgMs = sum / recentSlice.length;
    const currentFps = 1000.0 / avgMs;

    // Standard deviation (Jitter)
    const variance = recentSlice.reduce((acc, val) => acc + Math.pow(val - avgMs, 2), 0) / recentSlice.length;
    const stdDevMs = Math.sqrt(variance);
    const stdDevUs = stdDevMs * 1000.0; // microseconds

    // 1% Low computation
    const sorted = [...recentSlice].sort((a, b) => b - a); // descending frametimes = slowest frames
    const idx1Percent = Math.max(0, Math.floor(sorted.length * 0.01));
    const low1PercentMs = sorted[idx1Percent];
    const low1PercentFps = 1000.0 / low1PercentMs;

    // Update DOM
    if (this.fpsElem) {
      this.fpsElem.textContent = `${currentFps.toFixed(1)} FPS`;
    }
    if (this.frametimeElem) {
      this.frametimeElem.textContent = `${avgMs.toFixed(2)} ms`;
    }
    if (this.jitterElem) {
      if (stdDevUs < 100) {
        this.jitterElem.textContent = `±${stdDevUs.toFixed(0)} µs`;
      } else {
        this.jitterElem.textContent = `±${stdDevMs.toFixed(2)} ms`;
      }
    }
    if (this.low1Elem) {
      this.low1Elem.textContent = `${low1PercentFps.toFixed(1)} FPS`;
    }

    // Sync Mini Mode Header Metrics
    if (this.miniFpsElem) {
      this.miniFpsElem.textContent = currentFps.toFixed(1);
    }
    if (this.miniFtElem) {
      this.miniFtElem.textContent = avgMs.toFixed(2);
    }
    if (this.miniJitterElem) {
      if (stdDevUs < 100) {
        this.miniJitterElem.textContent = `±${stdDevUs.toFixed(0)} µs`;
      } else {
        this.miniJitterElem.textContent = `±${stdDevMs.toFixed(2)} ms`;
      }
    }
    if (this.miniFooterFt) {
      this.miniFooterFt.textContent = `${this.targetFrametime.toFixed(2)} ms / frame`;
    }

    if (this.cadenceStatusElem) {
      if (this.isPacingActive && stdDevMs < 0.15) {
        this.cadenceStatusElem.className = 'cadence-badge';
        this.cadenceStatusElem.innerHTML = `
          <svg width="12" height="12" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5">
            <polyline points="20 6 9 17 4 12"></polyline>
          </svg>
          Cadence: Flat (Perfect Lock)
        `;
      } else {
        this.cadenceStatusElem.className = 'cadence-badge judder';
        this.cadenceStatusElem.innerHTML = `
          <svg width="12" height="12" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5">
            <line x1="18" y1="6" x2="6" y2="18"></line>
            <line x1="6" y1="6" x2="18" y2="18"></line>
          </svg>
          Cadence: Irregular Judder
        `;
      }
    }
  }

  drawGraph(ctx, w, h) {
    if (!ctx || !w || !h || w <= 0 || h <= 0) return;

    // Clear background
    ctx.clearRect(0, 0, w, h);

    // Draw horizontal grid lines (e.g. 8.33ms, 16.67ms, 33.33ms)
    const gridLines = [8.333, 16.667, 25.0, 33.333];
    ctx.lineWidth = 1;
    gridLines.forEach(ms => {
      if (ms < this.maxScaleMs) {
        const y = h - (ms / this.maxScaleMs) * h;
        ctx.strokeStyle = 'rgba(255, 255, 255, 0.06)';
        ctx.setLineDash([4, 4]);
        ctx.beginPath();
        ctx.moveTo(0, y);
        ctx.lineTo(w, y);
        ctx.stroke();

        // Label (only on larger canvas)
        if (h > 100) {
          ctx.fillStyle = 'rgba(255, 255, 255, 0.25)';
          ctx.font = '9px "JetBrains Mono", monospace';
          ctx.fillText(`${ms.toFixed(1)}ms`, 6, y - 3);
        }
      }
    });
    ctx.setLineDash([]); // Reset line dash

    // Draw Target Pacer Baseline (Cyan / Emerald Glow Line)
    const targetY = h - (this.targetFrametime / this.maxScaleMs) * h;
    ctx.strokeStyle = this.isPacingActive ? 'rgba(0, 240, 255, 0.45)' : 'rgba(255, 255, 255, 0.15)';
    ctx.lineWidth = 1.5;
    ctx.beginPath();
    ctx.moveTo(0, targetY);
    ctx.lineTo(w, targetY);
    ctx.stroke();

    // Draw Frametime Graph Line & Fill Area
    if (this.frametimes.length > 1) {
      const step = w / (this.bufferSize - 1);

      // Gradient Fill below line
      const gradient = ctx.createLinearGradient(0, 0, 0, h);
      if (this.isPacingActive) {
        gradient.addColorStop(0, 'rgba(0, 240, 255, 0.25)');
        gradient.addColorStop(0.7, 'rgba(16, 185, 129, 0.08)');
        gradient.addColorStop(1, 'transparent');
      } else {
        gradient.addColorStop(0, 'rgba(244, 63, 94, 0.3)');
        gradient.addColorStop(0.7, 'rgba(245, 158, 11, 0.08)');
        gradient.addColorStop(1, 'transparent');
      }

      ctx.beginPath();
      for (let i = 0; i < this.frametimes.length; i++) {
        const x = i * step;
        const ms = this.frametimes[i];
        const y = Math.max(2, Math.min(h - 2, h - (ms / this.maxScaleMs) * h));
        if (i === 0) {
          ctx.moveTo(x, y);
        } else {
          ctx.lineTo(x, y);
        }
      }

      // Complete area path for fill
      ctx.lineTo(w, h);
      ctx.lineTo(0, h);
      ctx.closePath();
      ctx.fillStyle = gradient;
      ctx.fill();

      // Stroke line
      ctx.beginPath();
      for (let i = 0; i < this.frametimes.length; i++) {
        const x = i * step;
        const ms = this.frametimes[i];
        const y = Math.max(2, Math.min(h - 2, h - (ms / this.maxScaleMs) * h));
        if (i === 0) {
          ctx.moveTo(x, y);
        } else {
          ctx.lineTo(x, y);
        }
      }
      ctx.strokeStyle = this.isPacingActive ? '#00f0ff' : '#f43f5e';
      ctx.lineWidth = 2;
      ctx.stroke();

      // Glowing dot at the current latest frame point
      const lastX = (this.frametimes.length - 1) * step;
      const lastY = Math.max(2, Math.min(h - 2, h - (this.frametimes[this.frametimes.length - 1] / this.maxScaleMs) * h));
      ctx.fillStyle = this.isPacingActive ? '#10b981' : '#f43f5e';
      ctx.shadowColor = this.isPacingActive ? '#10b981' : '#f43f5e';
      ctx.shadowBlur = 8;
      ctx.beginPath();
      ctx.arc(lastX, lastY, 3.5, 0, Math.PI * 2);
      ctx.fill();
      ctx.shadowBlur = 0; // Reset shadow
    }
  }

  render() {
    if (this.ctx && this.width > 0 && this.height > 0) {
      this.drawGraph(this.ctx, this.width, this.height);
    }
    if (this.miniCtx && this.miniWidth > 0 && this.miniHeight > 0) {
      this.drawGraph(this.miniCtx, this.miniWidth, this.miniHeight);
    }
  }

  startLoop() {
    const loop = (timestamp) => {
      // Feed simulated frame updates
      if (this.simulationNoise) {
        this.generateSimulatedFrame();
      }
      this.updateMetrics();
      this.render();
      this.animationFrameId = requestAnimationFrame(loop);
    };
    this.animationFrameId = requestAnimationFrame(loop);
  }

  stopLoop() {
    if (this.animationFrameId) {
      cancelAnimationFrame(this.animationFrameId);
      this.animationFrameId = null;
    }
  }
}

window.FrametimeOscilloscope = FrametimeOscilloscope;
