const { contextBridge, ipcRenderer } = require('electron');

contextBridge.exposeInMainWorld('electronAPI', {
  toggleMiniMode: () => ipcRenderer.invoke('window-toggle-mini-mode'),
  getMiniMode: () => ipcRenderer.invoke('window-get-mini-mode'),
  onMiniModeChanged: (callback) => ipcRenderer.on('mini-mode-changed', (_event, value) => callback(value)),
  togglePin: () => ipcRenderer.invoke('window-toggle-pin'),
  minimizeWindow: () => ipcRenderer.invoke('window-minimize'),
  closeWindow: () => ipcRenderer.invoke('window-close'),
  getPinState: () => ipcRenderer.invoke('window-get-pin-state'),
  onPinStateChanged: (callback) => ipcRenderer.on('pin-state-changed', (_event, value) => callback(value))
});
