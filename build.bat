@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"
cl.exe /LD /O2 /EHsc /std:c++17 /Isrc\minhook\include /Fe:bin\FramePacerHook64_v9.dll src\dllmain.cpp src\dxgi_hook.cpp src\vulkan_hook.cpp src\minhook\src\buffer.c src\minhook\src\hook.c src\minhook\src\trampoline.c src\minhook\src\hde\hde64.c
cl.exe /O2 /EHsc /std:c++17 /Fe:bin\FramePacerOverlay_v3.exe src\transparent_overlay.cpp
cl.exe /O2 /EHsc /std:c++17 /Fe:bin\FramePacerBridge_v10.exe src\bridge_server.cpp
echo Build completed successfully.

