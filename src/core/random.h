// Generador pseudoaleatorio pequeno y determinista (xorshift32).
//
// Determinista a proposito: con la misma semilla una partida se repite igual, lo que
// permite reproducir en el Mac (tools/match_sim) lo que pasa en la PSP.
#pragma once

#include <psptypes.h>

namespace core {

class Random {
public:
    explicit Random(u32 seed = 0x12345678u) { reseed(seed); }

    void reseed(u32 seed) { m_state = seed ? seed : 0x9E3779B9u; }

    u32 next() {
        u32 x = m_state;
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        m_state = x;
        return x;
    }

    // [0, 1)
    float uniform() { return static_cast<float>(next() >> 8) * (1.0f / 16777216.0f); }
    // [lo, hi)
    float range(float lo, float hi) { return lo + (hi - lo) * uniform(); }
    // [0, n)
    int index(int n) { return n > 0 ? static_cast<int>(next() % static_cast<u32>(n)) : 0; }
    bool chance(float p) { return uniform() < p; }

private:
    u32 m_state;
};

}  // namespace core
