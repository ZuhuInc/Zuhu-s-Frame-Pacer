#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <cstdint>
#include <cstring>

namespace FramePacer {

constexpr const wchar_t* IPC_SHARED_MEMORY_NAME = L"Local\\FramePacer_IPC_SharedMemory";
constexpr uint32_t IPC_MAGIC = 0x50414345; // 'PACE'
constexpr uint32_t IPC_HISTORY_SIZE = 120;

#pragma pack(push, 1)
struct FramePacerControlBlock {
    uint32_t magic;                 // Verification magic (0x50414345)
    uint32_t version;               // Protocol version (1)
    
    // Commands from UI -> Hook DLL
    uint32_t target_fps;            // Target FPS (e.g. 60)
    uint32_t pacing_enabled;        // 1 = Active, 0 = Passthrough
    uint32_t queue_depth_clamp;     // 1 = SetMaximumFrameLatency(1)
    uint32_t vrr_mode_enabled;      // 1 = VRR Mode
    uint32_t overlay_enabled;       // 1 = In-game overlay active, 0 = hidden
    uint32_t overlay_position;      // 0 = Top-Left, 1 = Top-Right, 2 = Bottom-Left, 3 = Bottom-Right
    
    // Telemetry from Hook DLL -> UI
    uint32_t process_id;            // Injected game PID
    char process_name[128];         // Executable name (e.g. "Game.exe")
    char api_name[64];              // Graphics API (e.g. "DirectX 12 (DXGI)")
    float current_fps;              // Rolling FPS
    float current_frametime_ms;     // Rolling frametime (ms)
    float jitter_us;                // Standard deviation (microseconds)
    float low_1_percent_fps;        // 1% Low FPS
    
    uint32_t frame_index;           // Monotonically increasing frame counter
    float frametime_history[IPC_HISTORY_SIZE]; // Rolling frame buffer
};
#pragma pack(pop)

class SharedMemoryChannel {
public:
    SharedMemoryChannel() : m_hMapFile(NULL), m_pControlBlock(nullptr), m_isOwner(false) {}

    ~SharedMemoryChannel() {
        Close();
    }

    bool OpenOrCreate(bool asOwner = false) {
        m_isOwner = asOwner;
        if (asOwner) {
            m_hMapFile = CreateFileMappingW(
                INVALID_HANDLE_VALUE,
                NULL,
                PAGE_READWRITE,
                0,
                sizeof(FramePacerControlBlock),
                IPC_SHARED_MEMORY_NAME
            );
        } else {
            m_hMapFile = OpenFileMappingW(
                FILE_MAP_ALL_ACCESS,
                FALSE,
                IPC_SHARED_MEMORY_NAME
            );
            if (!m_hMapFile) {
                // If not found, create it
                m_hMapFile = CreateFileMappingW(
                    INVALID_HANDLE_VALUE,
                    NULL,
                    PAGE_READWRITE,
                    0,
                    sizeof(FramePacerControlBlock),
                    IPC_SHARED_MEMORY_NAME
                );
            }
        }

        if (!m_hMapFile) {
            return false;
        }

        m_pControlBlock = static_cast<FramePacerControlBlock*>(
            MapViewOfFile(m_hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(FramePacerControlBlock))
        );

        if (!m_pControlBlock) {
            CloseHandle(m_hMapFile);
            m_hMapFile = NULL;
            return false;
        }

        if (m_pControlBlock->magic != IPC_MAGIC) {
            m_pControlBlock->magic = IPC_MAGIC;
            m_pControlBlock->version = 1;
            m_pControlBlock->target_fps = 60;
            m_pControlBlock->pacing_enabled = 1;
            m_pControlBlock->queue_depth_clamp = 1;
            m_pControlBlock->vrr_mode_enabled = 1;
            m_pControlBlock->overlay_enabled = 1;
            m_pControlBlock->overlay_position = 0; // Top-Left default
        }

        return true;
    }

    FramePacerControlBlock* Get() { return m_pControlBlock; }

    void Close() {
        if (m_pControlBlock) {
            UnmapViewOfFile(m_pControlBlock);
            m_pControlBlock = nullptr;
        }
        if (m_hMapFile) {
            CloseHandle(m_hMapFile);
            m_hMapFile = NULL;
        }
    }

private:
    HANDLE m_hMapFile;
    FramePacerControlBlock* m_pControlBlock;
    bool m_isOwner;
};

} // namespace FramePacer
