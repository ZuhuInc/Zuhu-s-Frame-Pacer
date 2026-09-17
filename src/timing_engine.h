#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <immintrin.h> // for _mm_pause()
#include <cstdint>
#include <algorithm>

namespace FramePacer {

/**
 * @brief HighPrecisionPacer implements hybrid high-resolution timer + QPC spin-wait
 * presentation-edge synchronization.
 */
class HighPrecisionPacer {
public:
    HighPrecisionPacer() 
        : m_qpcFrequency(0)
        , m_targetIntervalQpc(0)
        , m_nextTargetQpc(0)
        , m_hWaitableTimer(NULL)
        , m_spinYieldThresholdQpc(0)
        , m_isInitialized(false)
        , m_lastPresentQpc(0)
    {
        Initialize();
    }

    ~HighPrecisionPacer() {
        if (m_hWaitableTimer != NULL) {
            CloseHandle(m_hWaitableTimer);
            m_hWaitableTimer = NULL;
        }
    }

    bool Initialize() {
        LARGE_INTEGER freq;
        if (!QueryPerformanceFrequency(&freq)) {
            return false;
        }
        m_qpcFrequency = freq.QuadPart;

        // Threshold before deadline to switch from waitable timer sleep to spin-wait (1.2 ms)
        m_spinYieldThresholdQpc = static_cast<int64_t>((m_qpcFrequency * 12) / 10000); // 0.0012s

        // Attempt to create Windows High-Resolution Waitable Timer (Windows 10 1803+)
        // CREATE_WAITABLE_TIMER_HIGH_RESOLUTION = 0x00000002
        m_hWaitableTimer = CreateWaitableTimerExW(
            NULL,
            NULL,
            0x00000002 | CREATE_WAITABLE_TIMER_MANUAL_RESET,
            TIMER_ALL_ACCESS
        );

        if (m_hWaitableTimer == NULL) {
            m_hWaitableTimer = CreateWaitableTimerW(NULL, TRUE, NULL);
        }

        m_isInitialized = (m_hWaitableTimer != NULL);
        SetTargetFps(60.0);
        Reset();
        return m_isInitialized;
    }

    void SetTargetFps(double targetFps) {
        if (targetFps <= 1.0) targetFps = 1.0;
        m_targetIntervalQpc = static_cast<int64_t>(static_cast<double>(m_qpcFrequency) / targetFps);
    }

    void SetTargetIntervalMs(double intervalMs) {
        if (intervalMs <= 0.1) intervalMs = 0.1;
        m_targetIntervalQpc = static_cast<int64_t>((static_cast<double>(m_qpcFrequency) * intervalMs) / 1000.0);
    }

    void Reset() {
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        m_lastPresentQpc = now.QuadPart;
        m_nextTargetQpc = now.QuadPart + m_targetIntervalQpc;
    }

    /**
     * @brief Waits until the exact presentation deadline for the current frame,
     * and advances the target schedule for the next frame.
     * @return The actual achieved frametime in milliseconds since previous present.
     */
    double WaitAndPace() {
        LARGE_INTEGER qpcNow;
        QueryPerformanceCounter(&qpcNow);
        int64_t now = qpcNow.QuadPart;

        // If game workload heavily exceeded budget (more than 1 full frame behind), resync schedule
        if (now >= m_nextTargetQpc + m_targetIntervalQpc) {
            m_nextTargetQpc = now + m_targetIntervalQpc;
        }

        // 1. Coarse Sleep Phase using High-Resolution Waitable Timer
        int64_t remainingTicks = m_nextTargetQpc - now;
        if (remainingTicks > m_spinYieldThresholdQpc && m_hWaitableTimer != NULL) {
            int64_t sleepTicks = remainingTicks - m_spinYieldThresholdQpc;
            
            // Convert QPC ticks to 100-nanosecond intervals (negative value = relative time)
            int64_t sleep100ns = -((sleepTicks * 10000000LL) / m_qpcFrequency);

            LARGE_INTEGER dueTime;
            dueTime.QuadPart = sleep100ns;

            if (SetWaitableTimer(m_hWaitableTimer, &dueTime, 0, NULL, NULL, FALSE)) {
                WaitForSingleObject(m_hWaitableTimer, INFINITE);
            }
        }

        // 2. Fine Spin-Wait Phase with CPU pipeline yielding (_mm_pause)
        while (true) {
            QueryPerformanceCounter(&qpcNow);
            if (qpcNow.QuadPart >= m_nextTargetQpc) {
                break;
            }
            _mm_pause(); // Low-latency pipeline yield
        }

        int64_t presentedAt = qpcNow.QuadPart;
        double frameTimeMs = static_cast<double>(presentedAt - m_lastPresentQpc) * 1000.0 / static_cast<double>(m_qpcFrequency);
        m_lastPresentQpc = presentedAt;

        // Advance to next presentation deadline
        m_nextTargetQpc += m_targetIntervalQpc;

        return frameTimeMs;
    }

    /**
     * @brief Measures instantaneous unpaced frametime without sleeping or delaying presentation.
     */
    double MeasureUnpacedFrame() {
        LARGE_INTEGER qpcNow;
        QueryPerformanceCounter(&qpcNow);
        int64_t now = qpcNow.QuadPart;

        double frameTimeMs = 0.0;
        if (m_lastPresentQpc > 0) {
            frameTimeMs = static_cast<double>(now - m_lastPresentQpc) * 1000.0 / static_cast<double>(m_qpcFrequency);
        }
        m_lastPresentQpc = now;
        m_nextTargetQpc = now + m_targetIntervalQpc;

        return frameTimeMs;
    }

    int64_t GetQpcFrequency() const { return m_qpcFrequency; }
    int64_t GetTargetIntervalQpc() const { return m_targetIntervalQpc; }
    bool IsInitialized() const { return m_isInitialized; }

private:
    int64_t m_qpcFrequency;
    int64_t m_targetIntervalQpc;
    int64_t m_nextTargetQpc;
    int64_t m_spinYieldThresholdQpc;
    int64_t m_lastPresentQpc;
    HANDLE m_hWaitableTimer;
    bool m_isInitialized;
};

} // namespace FramePacer
