// Medicion de tiempo y FPS reales basada en el reloj del sistema (microsegundos).
#pragma once

#include <psptypes.h>

namespace core {

// Tiempo absoluto del sistema en microsegundos.
u64 nowMicros();

class FrameTimer {
public:
    void reset();

    // Llamar una vez al principio de cada frame.
    void tick();

    float deltaSeconds() const { return m_delta; }
    float uptimeSeconds() const { return static_cast<float>(m_now - m_start) / 1e6f; }
    u32 frameCount() const { return m_frames; }

    // Estadisticas del ultimo intervalo completo (kWindowMicros).
    float fps() const { return m_fps; }
    float frameMsAvg() const { return m_msAvg; }
    float frameMsMin() const { return m_msMin; }
    float frameMsMax() const { return m_msMax; }

private:
    static constexpr u64 kWindowMicros = 500000;  // 0.5 s
    // Limite de delta para que una pausa (p.ej. menu HOME) no dispare la logica.
    static constexpr float kMaxDelta = 0.25f;

    u64 m_start = 0;
    u64 m_now = 0;
    u64 m_last = 0;
    float m_delta = 0.0f;
    u32 m_frames = 0;

    u64 m_windowStart = 0;
    u32 m_windowFrames = 0;
    u64 m_windowMinUs = 0;
    u64 m_windowMaxUs = 0;

    float m_fps = 0.0f;
    float m_msAvg = 0.0f;
    float m_msMin = 0.0f;
    float m_msMax = 0.0f;
};

}  // namespace core
