#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <wincrypt.h>
#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <thread>
#include <atomic>
#include <chrono>
#include "ipc_shared_memory.h"
#include "game_watcher.h"

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "user32.lib")

constexpr int BRIDGE_PORT = 28472;
constexpr const char* WS_MAGIC_GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

std::string Base64Encode(const unsigned char* data, size_t len) {
    DWORD outLen = 0;
    CryptBinaryToStringA(data, static_cast<DWORD>(len), CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, NULL, &outLen);
    std::string out(outLen, '\0');
    CryptBinaryToStringA(data, static_cast<DWORD>(len), CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, &out[0], &outLen);
    if (!out.empty() && out.back() == '\0') out.pop_back();
    return out;
}

std::string ComputeWebSocketAccept(const std::string& key) {
    std::string combined = key + WS_MAGIC_GUID;
    HCRYPTPROV hProv = 0;
    HCRYPTHASH hHash = 0;
    unsigned char hash[20];
    DWORD hashLen = 20;

    if (CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) {
        if (CryptCreateHash(hProv, CALG_SHA1, 0, 0, &hHash)) {
            CryptHashData(hHash, reinterpret_cast<const BYTE*>(combined.c_str()), static_cast<DWORD>(combined.length()), 0);
            CryptGetHashParam(hHash, HP_HASHVAL, hash, &hashLen, 0);
            CryptDestroyHash(hHash);
        }
        CryptReleaseContext(hProv, 0);
    }
    return Base64Encode(hash, hashLen);
}

void SendWebSocketText(SOCKET sock, const std::string& text) {
    std::vector<uint8_t> frame;
    frame.push_back(0x81); // FIN + Text opcode

    size_t len = text.length();
    if (len <= 125) {
        frame.push_back(static_cast<uint8_t>(len));
    } else if (len <= 65535) {
        frame.push_back(126);
        frame.push_back(static_cast<uint8_t>((len >> 8) & 0xFF));
        frame.push_back(static_cast<uint8_t>(len & 0xFF));
    } else {
        frame.push_back(127);
        for (int i = 7; i >= 0; --i) {
            frame.push_back(static_cast<uint8_t>((len >> (i * 8)) & 0xFF));
        }
    }

    frame.insert(frame.end(), text.begin(), text.end());
    send(sock, reinterpret_cast<const char*>(frame.data()), static_cast<int>(frame.size()), 0);
}

void ProcessWebSocketMessage(const std::string& msg, FramePacer::SharedMemoryChannel& ipc) {
    auto* pBlock = ipc.Get();
    if (!pBlock) return;

    // Parse simple JSON commands without external dependencies
    if (msg.find("SET_TARGET_FPS") != std::string::npos) {
        size_t fpsPos = msg.find("\"fps\":");
        if (fpsPos != std::string::npos) {
            double fps = std::stod(msg.substr(fpsPos + 6));
            if (fps >= 10.0 && fps <= 360.0) {
                pBlock->target_fps = static_cast<uint32_t>(fps);
                std::cout << "[Bridge] Updated Target FPS -> " << pBlock->target_fps << " FPS" << std::endl;
            }
        }
    } else if (msg.find("SET_PACING_ENABLED") != std::string::npos) {
        size_t enPos = msg.find("\"enabled\":");
        if (enPos != std::string::npos) {
            bool en = (msg.substr(enPos + 10, 4) == "true");
            pBlock->pacing_enabled = en ? 1 : 0;
            std::cout << "[Bridge] Pacing State -> " << (en ? "ENABLED" : "DISABLED") << std::endl;
        }
    } else if (msg.find("SET_QUEUE_CLAMP") != std::string::npos) {
        size_t enPos = msg.find("\"enabled\":");
        if (enPos != std::string::npos) {
            bool en = (msg.substr(enPos + 10, 4) == "true");
            pBlock->queue_depth_clamp = en ? 1 : 0;
        }
    } else if (msg.find("SET_AUTO_ATTACH") != std::string::npos) {
        size_t enPos = msg.find("\"enabled\":");
        if (enPos != std::string::npos) {
            bool en = (msg.substr(enPos + 10, 4) == "true");
            FramePacer::GameWatcher::Instance().SetEnabled(en);
        }
    } else if (msg.find("SET_OVERLAY_ENABLED") != std::string::npos) {
        size_t enPos = msg.find("\"enabled\":");
        if (enPos != std::string::npos) {
            bool en = (msg.substr(enPos + 10, 4) == "true");
            pBlock->overlay_enabled = en ? 1 : 0;
            std::cout << "[Bridge] Overlay State -> " << (en ? "ENABLED" : "DISABLED") << std::endl;
        }
    } else if (msg.find("SET_OVERLAY_POSITION") != std::string::npos) {
        size_t posVal = msg.find("\"position\":");
        if (posVal != std::string::npos) {
            uint32_t pos = static_cast<uint32_t>(std::stoul(msg.substr(posVal + 11)));
            pBlock->overlay_position = pos;
            std::cout << "[Bridge] Overlay Position -> " << pos << std::endl;
        }
    }
}

void HandleClient(SOCKET clientSock) {
    FramePacer::SharedMemoryChannel ipc;
    ipc.OpenOrCreate(false);

    char buffer[4096];
    int bytesReceived = recv(clientSock, buffer, sizeof(buffer) - 1, 0);
    if (bytesReceived <= 0) {
        closesocket(clientSock);
        return;
    }
    buffer[bytesReceived] = '\0';
    std::string request(buffer);

    // Perform HTTP WebSocket Handshake
    size_t keyPos = request.find("Sec-WebSocket-Key: ");
    if (keyPos == std::string::npos) {
        closesocket(clientSock);
        return;
    }
    keyPos += 19;
    size_t keyEnd = request.find("\r\n", keyPos);
    std::string wsKey = request.substr(keyPos, keyEnd - keyPos);
    std::string acceptKey = ComputeWebSocketAccept(wsKey);

    std::ostringstream response;
    response << "HTTP/1.1 101 Switching Protocols\r\n"
             << "Upgrade: websocket\r\n"
             << "Connection: Upgrade\r\n"
             << "Sec-WebSocket-Accept: " << acceptKey << "\r\n\r\n";

    std::string respStr = response.str();
    send(clientSock, respStr.c_str(), static_cast<int>(respStr.length()), 0);

    std::cout << "[Bridge] Frontend UI Connected via WebSocket!" << std::endl;

    // Set client socket to non-blocking mode
    u_long mode = 1;
    ioctlsocket(clientSock, FIONBIO, &mode);

    uint32_t lastFrameIdx = 0;

    while (true) {
        // 1. Check for incoming messages from UI
        bytesReceived = recv(clientSock, buffer, sizeof(buffer), 0);
        if (bytesReceived > 0) {
            // Decode WebSocket text frame
            if (static_cast<uint8_t>(buffer[0]) == 0x88) {
                // Close frame
                break;
            }
            if (static_cast<uint8_t>(buffer[0]) == 0x81 && bytesReceived >= 6) {
                bool isMasked = (buffer[1] & 0x80) != 0;
                uint8_t payloadLen = buffer[1] & 0x7F;
                int maskOffset = 2;
                if (payloadLen == 126) maskOffset = 4;

                uint8_t mask[4] = {
                    static_cast<uint8_t>(buffer[maskOffset]),
                    static_cast<uint8_t>(buffer[maskOffset + 1]),
                    static_cast<uint8_t>(buffer[maskOffset + 2]),
                    static_cast<uint8_t>(buffer[maskOffset + 3])
                };

                int dataOffset = maskOffset + 4;
                int dataLen = bytesReceived - dataOffset;
                std::string msg;
                msg.resize(dataLen);

                for (int i = 0; i < dataLen; ++i) {
                    msg[i] = buffer[dataOffset + i] ^ mask[i % 4];
                }

                ProcessWebSocketMessage(msg, ipc);
            }
        } else if (bytesReceived == 0) {
            break;
        }

        // 2. Stream Live Game Telemetry to UI at 60 Hz
        auto* pBlock = ipc.Get();
        if (pBlock) {
            uint32_t activePid = pBlock->process_id;
            std::string procName = pBlock->process_name;
            std::string apiName = pBlock->api_name;
            float fps = pBlock->current_fps;
            float ft = pBlock->current_frametime_ms;
            float jitter = pBlock->jitter_us;

            // Verify if process_id is still a live running game process
            if (activePid > 0) {
                HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, activePid);
                if (hProc) {
                    DWORD exitCode = 0;
                    if (GetExitCodeProcess(hProc, &exitCode) && exitCode != STILL_ACTIVE) {
                        // Game process terminated
                        pBlock->process_id = 0;
                        pBlock->process_name[0] = '\0';
                        pBlock->api_name[0] = '\0';
                        pBlock->current_fps = 0.0f;
                        pBlock->current_frametime_ms = 0.0f;
                        pBlock->jitter_us = 0.0f;
                        activePid = 0;
                        procName = "";
                        apiName = "";
                        fps = 0.0f;
                        ft = 0.0f;
                        jitter = 0.0f;
                    }
                    CloseHandle(hProc);
                } else {
                    // Cannot open PID -> Process no longer exists
                    pBlock->process_id = 0;
                    pBlock->process_name[0] = '\0';
                    pBlock->api_name[0] = '\0';
                    pBlock->current_fps = 0.0f;
                    pBlock->current_frametime_ms = 0.0f;
                    pBlock->jitter_us = 0.0f;
                    activePid = 0;
                    procName = "";
                    apiName = "";
                    fps = 0.0f;
                    ft = 0.0f;
                    jitter = 0.0f;
                }
            }

            std::ostringstream json;
            json << "{\"type\":\"TELEMETRY_FRAME\",\"data\":{"
                 << "\"pid\":" << activePid << ","
                 << "\"process\":\"" << procName << "\","
                 << "\"api\":\"" << apiName << "\","
                 << "\"fps\":" << fps << ","
                 << "\"frametime\":" << ft << ","
                 << "\"jitter\":" << jitter << ","
                 << "\"targetFps\":" << pBlock->target_fps << ","
                 << "\"pacingEnabled\":" << (pBlock->pacing_enabled != 0 ? "true" : "false") << ","
                 << "\"overlayEnabled\":" << (pBlock->overlay_enabled != 0 ? "true" : "false") << ","
                 << "\"overlayPosition\":" << pBlock->overlay_position << ","
                 << "\"frameIndex\":" << pBlock->frame_index
                 << "}}";

            SendWebSocketText(clientSock, json.str());
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    std::cout << "[Bridge] Frontend UI Disconnected." << std::endl;
    closesocket(clientSock);
}

static PROCESS_INFORMATION g_overlayPi = {};
static HANDLE g_hJob = NULL;

void LaunchManagedOverlay() {
    g_hJob = CreateJobObjectW(NULL, NULL);
    if (g_hJob) {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli = {};
        jeli.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        SetInformationJobObject(g_hJob, JobObjectExtendedLimitInformation, &jeli, sizeof(jeli));
    }

    wchar_t exeDir[MAX_PATH] = {};
    GetModuleFileNameW(NULL, exeDir, MAX_PATH);
    wchar_t* pLastSlash = wcsrchr(exeDir, L'\\');
    if (pLastSlash) *pLastSlash = L'\0';

    wchar_t overlayExePath[MAX_PATH] = {};
    swprintf_s(overlayExePath, L"%s\\FramePacerOverlay_v3.exe", exeDir);
    if (GetFileAttributesW(overlayExePath) == INVALID_FILE_ATTRIBUTES) {
        if (GetFileAttributesW(L"bin\\FramePacerOverlay_v3.exe") != INVALID_FILE_ATTRIBUTES) {
            GetFullPathNameW(L"bin\\FramePacerOverlay_v3.exe", MAX_PATH, overlayExePath, NULL);
        } else if (GetFileAttributesW(L"FramePacerOverlay_v3.exe") != INVALID_FILE_ATTRIBUTES) {
            GetFullPathNameW(L"FramePacerOverlay_v3.exe", MAX_PATH, overlayExePath, NULL);
        }
    }

    if (overlayExePath[0] != L'\0' && GetFileAttributesW(overlayExePath) != INVALID_FILE_ATTRIBUTES) {
        STARTUPINFOW si = {};
        si.cb = sizeof(si);
        if (CreateProcessW(overlayExePath, NULL, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &g_overlayPi)) {
            if (g_hJob) {
                AssignProcessToJobObject(g_hJob, g_overlayPi.hProcess);
            }
            std::wcout << L"[Bridge] Auto-launched Managed Overlay [PID: " << g_overlayPi.dwProcessId << L"]" << std::endl;
        }
    }
}

BOOL WINAPI ConsoleCtrlHandler(DWORD signal) {
    if (signal == CTRL_C_EVENT || signal == CTRL_CLOSE_EVENT || signal == CTRL_BREAK_EVENT) {
        std::cout << "\n[Bridge] Shutting down FramePacer Bridge & Managed Overlay..." << std::endl;
        FramePacer::GameWatcher::Instance().Stop();
        if (g_overlayPi.hProcess) {
            TerminateProcess(g_overlayPi.hProcess, 0);
            CloseHandle(g_overlayPi.hProcess);
            CloseHandle(g_overlayPi.hThread);
            g_overlayPi.hProcess = NULL;
        }
        if (g_hJob) {
            CloseHandle(g_hJob);
            g_hJob = NULL;
        }
        WSACleanup();
        ExitProcess(0);
    }
    return TRUE;
}

int main() {
    std::cout << "================================================================" << std::endl;
    std::cout << "        FRAMEPACER - REAL-TIME IPC BRIDGE SERVER (UNIFIED)      " << std::endl;
    std::cout << "================================================================" << std::endl;

    SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE);

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed." << std::endl;
        return 1;
    }

    SOCKET listenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSock == INVALID_SOCKET) {
        std::cerr << "Socket creation failed." << std::endl;
        WSACleanup();
        return 1;
    }

    int opt = 1;
    setsockopt(listenSock, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt), sizeof(opt));

    sockaddr_in serverAddr = {};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK); // 127.0.0.1
    serverAddr.sin_port = htons(BRIDGE_PORT);

    if (bind(listenSock, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "Bind failed on port " << BRIDGE_PORT << std::endl;
        closesocket(listenSock);
        WSACleanup();
        return 1;
    }

    if (listen(listenSock, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "Listen failed." << std::endl;
        closesocket(listenSock);
        WSACleanup();
        return 1;
    }

    std::cout << "[Bridge] Listening on ws://127.0.0.1:" << BRIDGE_PORT << std::endl;
    std::cout << "[Bridge] Open ui/index.html to control games in real-time!" << std::endl;

    // Start background GameWatcher auto-attach daemon
    wchar_t exeDir[MAX_PATH] = {};
    GetModuleFileNameW(NULL, exeDir, MAX_PATH);
    wchar_t* pLastSlash = wcsrchr(exeDir, L'\\');
    if (pLastSlash) *pLastSlash = L'\0';

    wchar_t fullDllPath[MAX_PATH] = {};
    swprintf_s(fullDllPath, L"%s\\FramePacerHook64_v9.dll", exeDir);
    if (GetFileAttributesW(fullDllPath) == INVALID_FILE_ATTRIBUTES) {
        if (GetFileAttributesW(L"bin\\FramePacerHook64_v9.dll") != INVALID_FILE_ATTRIBUTES) {
            GetFullPathNameW(L"bin\\FramePacerHook64_v9.dll", MAX_PATH, fullDllPath, NULL);
        } else if (GetFileAttributesW(L"FramePacerHook64_v9.dll") != INVALID_FILE_ATTRIBUTES) {
            GetFullPathNameW(L"FramePacerHook64_v9.dll", MAX_PATH, fullDllPath, NULL);
        }
    }

    std::wcout << L"[Bridge] Auto-Attach Engine Active with DLL: " << fullDllPath << std::endl;
    FramePacer::GameWatcher::Instance().Start(fullDllPath);

    // Launch Managed Transparent Overlay
    LaunchManagedOverlay();

    while (true) {
        SOCKET clientSock = accept(listenSock, NULL, NULL);
        if (clientSock != INVALID_SOCKET) {
            std::thread(HandleClient, clientSock).detach();
        }
    }

    FramePacer::GameWatcher::Instance().Stop();
    closesocket(listenSock);
    WSACleanup();
    return 0;
}
