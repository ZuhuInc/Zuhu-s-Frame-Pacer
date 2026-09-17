@echo off
set "LAYER_PATH=%~dp0bin\VkLayer_FramePacer.json"
reg delete "HKCU\SOFTWARE\Khronos\Vulkan\ImplicitLayers" /v "%LAYER_PATH%" /f
echo.
echo ================================================================
echo   FramePacer Khronos Vulkan Implicit Layer Removed.
echo ================================================================
echo.
pause
