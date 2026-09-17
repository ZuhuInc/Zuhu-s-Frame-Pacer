@echo off
echo ================================================================
echo        FRAMEPACER - STANDALONE EXECUTABLE BUILD SCRIPT
echo ================================================================

taskkill /F /IM FramePacer.exe /T >nul 2>&1
taskkill /F /IM FramePacerBridge_v10.exe /T >nul 2>&1
taskkill /F /IM FramePacerOverlay_v3.exe /T >nul 2>&1

echo 1. Compiling high-performance C++ presentation engines and bridge...
call build.bat
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] C++ build failed.
    pause
    exit /b %ERRORLEVEL%
)

echo.
echo 2. Packaging Electron Desktop Application into Standalone EXE...
call npm run package
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Packaging failed.
    pause
    exit /b %ERRORLEVEL%
)

echo.
echo ================================================================
echo  SUCCESS! Standalone FramePacer executable is ready at:
echo  release\FramePacer-win32-x64\FramePacer.exe
echo ================================================================
pause
