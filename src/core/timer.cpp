#include "core/timer.h"

#include <pspthreadman.h>

namespace core {

u64 nowMicros() { return static_cast<u64>(sceKernelGetSystemTimeWide()); }

void FrameTimer::reset() {
    m_start = m_now = m_last = m_windowStart = nowMicros();
    m_delta = 0.0f;
    m_frames = 0;
    m_windowFrames = 0;
    m_windowMinUs = ~0ull;
    m_windowMaxUs = 0;
    m_fps = m_msAvg = m_msMin = m_msMax = 0.0f;
}

void FrameTimer::tick() {
    m_now = nowMicros();
    const u64 elapsed = m_now - m_last;
    m_last = m_now;

    m_delta = static_cast<float>(elapsed) / 1e6f;
    if (m_delta > kMaxDelta) m_delta = kMaxDelta;

    // El primer tick tras reset() no corresponde a un frame completo.
    if (m_frames++ == 0) return;

    ++m_windowFrames;
    if (elapsed < m_windowMinUs) m_windowMinUs = elapsed;
    if (elapsed > m_windowMaxUs) m_windowMaxUs = elapsed;

    const u64 window = m_now - m_windowStart;
    if (window >= kWindowMicros) {
        m_fps = static_cast<float>(m_windowFrames) * 1e6f / static_cast<float>(window);
        m_msAvg = static_cast<float>(window) / 1000.0f / static_cast<float>(m_windowFrames);
        m_msMin = static_cast<float>(m_windowMinUs) / 1000.0f;
        m_msMax = static_cast<float>(m_windowMaxUs) / 1000.0f;

        m_windowStart = m_now;
        m_windowFrames = 0;
        m_windowMinUs = ~0ull;
        m_windowMaxUs = 0;
    }
}

}  // namespace core
