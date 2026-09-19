const { app, BrowserWindow, globalShortcut, Tray, Menu, ipcMain, nativeImage, dialog, screen } = require('electron');
const path = require('path');
const { spawn } = require('child_process');
const fs = require('fs');

let mainWindow = null;
let tray = null;
let bridgeProcess = null;
let isPinned = false;
let isQuitting = false;

// Ensure single instance
const gotTheLock = app.requestSingleInstanceLock();
if (!gotTheLock) {
  app.quit();
} else {
  app.on('second-instance', () => {
    if (mainWindow) {
      if (mainWindow.isMinimized()) mainWindow.restore();
      mainWindow.setSkipTaskbar(false);
      mainWindow.show();
      mainWindow.focus();
    }
  });
}

function getBinPath(filename) {
  if (app.isPackaged) {
    const resPath = path.join(process.resourcesPath, 'bin', filename);
    if (fs.existsSync(resPath)) return resPath;
    const appUnpackPath = path.join(process.resourcesPath, 'app.asar.unpacked', 'bin', filename);
    if (fs.existsSync(appUnpackPath)) return appUnpackPath;
  }
  return path.join(__dirname, 'bin', filename);
}

function startBridgeDaemon() {
  const bridgePath = getBinPath('FramePacerBridge_v10.exe');
  const binDir = path.dirname(bridgePath);
  if (fs.existsSync(bridgePath)) {
    try {
      bridgeProcess = spawn(bridgePath, [], {
        cwd: binDir,
        detached: false,
        stdio: 'ignore'
      });
      console.log('[App] Started FramePacerBridge daemon PID:', bridgeProcess.pid);
    } catch (e) {
      console.error('[App] Failed to start bridge daemon:', e);
    }
  }
}

function getAppIcon() {
  const iconPath = path.join(__dirname, 'assets', 'framepacer-logo.png');
  if (fs.existsSync(iconPath)) {
    return nativeImage.createFromPath(iconPath);
  }
  return nativeImage.createEmpty();
}

function createTrayIcon() {
  const trayIconPath = path.join(__dirname, 'assets', 'tray-icon.png');
  let icon = fs.existsSync(trayIconPath) 
    ? nativeImage.createFromPath(trayIconPath)
    : getAppIcon();

  tray = new Tray(icon);
  tray.setToolTip('FramePacer - Universal Frame Pacer & Jitter Eliminator');

  const contextMenu = Menu.buildFromTemplate([
    {
      label: 'Open FramePacer [Insert]',
      click: () => {
        if (mainWindow) {
          mainWindow.setSkipTaskbar(false);
          mainWindow.show();
          mainWindow.focus();
        }
      }
    },
    {
      label: 'Always On Top [Pin]',
      type: 'checkbox',
      checked: isPinned,
      click: (item) => {
        isPinned = item.checked;
        if (mainWindow) {
          mainWindow.setAlwaysOnTop(isPinned, 'screen-saver');
          mainWindow.webContents.send('pin-state-changed', isPinned);
        }
      }
    },
    { type: 'separator' },
    {
      label: 'Exit FramePacer',
      click: () => {
        isQuitting = true;
        app.quit();
      }
    }
  ]);

  tray.setContextMenu(contextMenu);

  tray.on('click', () => {
    if (mainWindow) {
      if (mainWindow.isVisible()) {
        mainWindow.hide();
        mainWindow.setSkipTaskbar(true);
      } else {
        mainWindow.setSkipTaskbar(false);
        mainWindow.show();
        mainWindow.focus();
      }
    }
  });
}

function createWindow() {
  const appIcon = getAppIcon();
  mainWindow = new BrowserWindow({
    width: 1130,
    height: 730,
    minWidth: 1040,
    minHeight: 660,
    frame: false,
    transparent: true,
    backgroundColor: '#00000000',
    hasShadow: true,
    roundedCorners: true,
    icon: appIcon,
    webPreferences: {
      preload: path.join(__dirname, 'preload.js'),
      nodeIntegration: false,
      contextIsolation: true
    }
  });

  mainWindow.loadFile(path.join(__dirname, 'ui', 'index.html'));

  mainWindow.on('close', (event) => {
    // When window is closed, quit everything
    isQuitting = true;
    app.quit();
  });

  // Register Global Hotkey [Insert] to toggle menu visibility
  globalShortcut.register('Insert', () => {
    if (mainWindow) {
      if (mainWindow.isVisible()) {
        mainWindow.hide();
        mainWindow.setSkipTaskbar(true);
      } else {
        mainWindow.setSkipTaskbar(false);
        mainWindow.show();
        mainWindow.focus();
      }
    }
  });

  // IPC Handlers from Frontend UI
  let isMiniMode = false;
  let normalBounds = { width: 1130, height: 730 };

  ipcMain.handle('window-toggle-mini-mode', () => {
    isMiniMode = !isMiniMode;
    if (isMiniMode) {
      const bounds = mainWindow.getBounds();
      normalBounds = { width: bounds.width, height: bounds.height };
      mainWindow.setMinimumSize(440, 270);
      mainWindow.setSize(480, 290);
    } else {
      mainWindow.setMinimumSize(1040, 660);
      mainWindow.setSize(normalBounds.width || 1130, normalBounds.height || 730);
    }
    mainWindow.webContents.send('mini-mode-changed', isMiniMode);
    return isMiniMode;
  });

  ipcMain.handle('window-get-mini-mode', () => {
    return isMiniMode;
  });

  ipcMain.handle('window-toggle-pin', () => {
    isPinned = !isPinned;
    mainWindow.setAlwaysOnTop(isPinned, 'screen-saver');
    mainWindow.webContents.send('pin-state-changed', isPinned);
    return isPinned;
  });

  ipcMain.handle('window-get-pin-state', () => {
    return isPinned;
  });

  ipcMain.handle('window-minimize', () => {
    mainWindow.hide();
    mainWindow.setSkipTaskbar(true);
  });

  ipcMain.handle('window-close', () => {
    isQuitting = true;
    app.quit();
  });

  // Profile Storage IPC
  const getProfilesPath = () => {
    const userDataDir = app.getPath('userData');
    return path.join(userDataDir, 'profiles.json');
  };

  ipcMain.handle('select-game-exe', async () => {
    if (!mainWindow) return null;
    const result = await dialog.showOpenDialog(mainWindow, {
      title: 'Select Game Executable',
      filters: [
        { name: 'Executable Files', extensions: ['exe'] },
        { name: 'All Files', extensions: ['*'] }
      ],
      properties: ['openFile']
    });
    if (!result.canceled && result.filePaths.length > 0) {
      const selectedPath = result.filePaths[0];
      const exeName = path.basename(selectedPath);
      // Generate clean display name (e.g., "cyberpunk2077.exe" -> "Cyberpunk 2077")
      let cleanName = exeName.replace(/\.exe$/i, '');
      cleanName = cleanName.replace(/[-_]/g, ' ').replace(/\b\w/g, c => c.toUpperCase());
      return {
        path: selectedPath,
        exe: exeName.toLowerCase(),
        name: cleanName
      };
    }
    return null;
  });

  ipcMain.handle('save-profiles', async (_event, data) => {
    try {
      const filePath = getProfilesPath();
      const dir = path.dirname(filePath);
      if (!fs.existsSync(dir)) {
        fs.mkdirSync(dir, { recursive: true });
      }
      fs.writeFileSync(filePath, JSON.stringify(data, null, 2), 'utf8');
      return true;
    } catch (e) {
      console.error('[App] Failed to save profiles:', e);
      return false;
    }
  });

  ipcMain.handle('load-profiles', async () => {
    try {
      const filePath = getProfilesPath();
      if (fs.existsSync(filePath)) {
        const raw = fs.readFileSync(filePath, 'utf8');
        return JSON.parse(raw);
      }
    } catch (e) {
      console.error('[App] Failed to load profiles:', e);
    }
    return null;
  });

  // Display & Refresh Rate Telemetry IPC
  const formatDisplayData = (disp, index, isPrimary) => {
    const hz = disp.displayFrequency || 60;
    let vrrCap;
    if (hz >= 200) {
      vrrCap = Math.floor(hz - 4);
    } else if (hz >= 100) {
      vrrCap = Math.floor(hz - 3);
    } else {
      vrrCap = Math.floor(hz - 2);
    }

    return {
      id: disp.id,
      index: index,
      name: disp.label || (isPrimary ? 'Primary Display (UltraGear/Gaming Display)' : `Display ${index + 1}`),
      width: disp.bounds.width,
      height: disp.bounds.height,
      resolution: `${disp.bounds.width}x${disp.bounds.height}`,
      refreshRate: hz,
      isPrimary: isPrimary,
      vrrCap: vrrCap,
      halfRate: Math.round(hz / 2),
      thirdRate: Math.round(hz / 3)
    };
  };

  ipcMain.handle('get-display-info', () => {
    try {
      const primary = screen.getPrimaryDisplay();
      const all = screen.getAllDisplays();
      const primaryData = formatDisplayData(primary, 0, true);
      const allData = all.map((d, i) => formatDisplayData(d, i, d.id === primary.id));
      return {
        primary: primaryData,
        displays: allData
      };
    } catch (e) {
      console.error('[App] Failed to get display info:', e);
      return {
        primary: {
          id: 1,
          index: 0,
          name: 'Primary Gaming Display',
          width: 1920,
          height: 1080,
          resolution: '1920x1080',
          refreshRate: 144,
          isPrimary: true,
          vrrCap: 141,
          halfRate: 72,
          thirdRate: 48
        },
        displays: []
      };
    }
  });

  screen.on('display-metrics-changed', () => {
    if (mainWindow && !mainWindow.isDestroyed()) {
      try {
        const primary = screen.getPrimaryDisplay();
        const all = screen.getAllDisplays();
        const primaryData = formatDisplayData(primary, 0, true);
        const allData = all.map((d, i) => formatDisplayData(d, i, d.id === primary.id));
        mainWindow.webContents.send('display-changed', {
          primary: primaryData,
          displays: allData
        });
      } catch (e) {}
    }
  });
}

app.whenReady().then(() => {
  startBridgeDaemon();
  createTrayIcon();
  createWindow();

  app.on('activate', () => {
    if (BrowserWindow.getAllWindows().length === 0) createWindow();
  });
});

app.on('will-quit', () => {
  globalShortcut.unregisterAll();
  if (bridgeProcess) {
    try {
      bridgeProcess.kill();
    } catch (e) { }
  }
});
