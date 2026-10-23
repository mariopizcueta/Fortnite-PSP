// Paso fijo para la fisica: separa la simulacion de la velocidad de renderizado.
//
// Cada frame se acumula su delta time y se simulan tantos pasos de kStep como quepan.
// El resto queda acumulado y alpha() indica cuanto se ha avanzado hacia el siguiente
// paso, para interpolar lo que se dibuja entre el estado anterior y el actual.
//
// Limite: un frame nunca aporta mas de kMaxFrameTime (como mucho 6 pasos). Si el juego
// se atasca (carga, caida fuerte de FPS), la simulacion se ralentiza en vez de dar un
// salto de varios metros o atravesar el suelo.
//
// Ajuste a VSync: un frame que dura casi exactamente N pasos (a menos del 2 %) cuenta
// como N pasos exactos. La PSP refresca a 59.94 Hz, no a 60: sin este ajuste el
// sobrante se acumularia y de vez en cuando un frame haria 2 pasos (o 0), y a 30 FPS
// el redondeo de float alternaria 1 y 3 pasos en vez de 2 y 2.
#pragma once

#include <math.h>

namespace game {

class FixedTimestep {
public:
    static constexpr float kStep = 1.0f / 60.0f;
    static constexpr float kMaxFrameTime = 0.1f;
    static constexpr float kSnapTolerance = 0.02f;  // fraccion de un paso

    // Anade el tiempo del frame y devuelve cuantos pasos fijos hay que simular.
    int advance(float frameSeconds) {
        if (frameSeconds < 0.0f) frameSeconds = 0.0f;
        if (frameSeconds > kMaxFrameTime) {
            m_discarded += frameSeconds - kMaxFrameTime;
            frameSeconds = kMaxFrameTime;
        }
        const float nearest = roundf(frameSeconds / kStep);
        if (nearest >= 1.0f && fabsf(frameSeconds - nearest * kStep) < kStep * kSnapTolerance) {
            frameSeconds = nearest * kStep;
        }
        m_accumulator += frameSeconds;
        int steps = 0;
        // Margen de 1 us para que 3 * kStep (redondeado en float) cuente como 3 pasos.
        while (m_accumulator >= kStep - kEpsilon) {
            m_accumulator -= kStep;
            ++steps;
        }
        if (m_accumulator < 0.0f) m_accumulator = 0.0f;
        m_lastSteps = steps;
        return steps;
    }

    // Fraccion [0, 1) del siguiente paso ya transcurrida.
    float alpha() const { return m_accumulator / kStep; }
    int lastSteps() const { return m_lastSteps; }
    // Tiempo total descartado por frames demasiado largos (diagnostico).
    float discardedSeconds() const { return m_discarded; }

    void reset() {
        m_accumulator = 0.0f;
        m_lastSteps = 0;
        m_discarded = 0.0f;
    }

private:
    static constexpr float kEpsilon = 1e-6f;

    float m_accumulator = 0.0f;
    int m_lastSteps = 0;
    float m_discarded = 0.0f;
};

}  // namespace game
