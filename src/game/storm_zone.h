// Tormenta que se cierra (ademas de la del limite del mapa, world/storm.h).
//
// Como en Fortnite: la zona segura es un circulo. Cada `phaseSeconds` empieza un cierre
// que dura `shrinkSeconds` y lleva el circulo actual al siguiente, mas pequeno y
// contenido en el (centro al azar). Fuera del circulo hay tormenta, con un dano por
// segundo que sube con cada cierre. El ultimo circulo tiene radio 0.
#pragma once

#include "core/random.h"
#include "game/nav_grid.h"
#include "math/math3d.h"

namespace game {

struct StormConfig {
    bool closes = true;
    float firstClose = 60.0f;     // s hasta que empieza el primer cierre
    float phaseSeconds = 40.0f;   // un cierre cada tantos segundos
    float shrinkSeconds = 20.0f;  // lo que dura cada cierre
    math3d::Vec3 center{0.0f, 0.0f, -6.0f};
    // Sin cierres (reglas clasicas): la "zona" solo guia a los bots, de startRadius a
    // endRadius en endSeconds, sin dano.
    float startRadius = 80.0f;
    float endRadius = 22.0f;
    float endSeconds = 240.0f;
};

class StormZone {
public:
    static constexpr int kPhases = 8;

    // `nav`: los centros nuevos caen en suelo transitable (nunca en el lago).
    void reset(const StormConfig& config, core::Random& rng, const NavGrid* nav);
    // Devuelve true en el paso en que empieza un cierre.
    bool update(float time, core::Random& rng);

    const math3d::Vec3& center() const { return m_center; }
    float radius() const { return m_radius; }
    // Circulo al que se va a cerrar (o el actual, si ya no quedan cierres).
    const math3d::Vec3& nextCenter() const { return m_nextCenter; }
    float nextRadius() const { return m_nextRadius; }
    bool closes() const { return m_config.closes; }
    bool shrinking() const { return m_shrinking; }
    int phase() const { return m_phase; }  // cierres ya empezados
    // Segundos hasta el siguiente cierre (o hasta que acabe el actual); < 0 si no quedan.
    float timeLeft() const { return m_timeLeft; }
    // Metros dentro de la tormenta del circulo (positivo) o distancia hasta ella.
    float depth(float x, float z) const;
    // Dano por segundo dentro de la tormenta del circulo en la fase actual.
    float damage() const;

private:
    void chooseNext(core::Random& rng);
    float startOf(int phase) const { return m_config.firstClose + phase * m_config.phaseSeconds; }

    StormConfig m_config;
    const NavGrid* m_nav = nullptr;
    math3d::Vec3 m_center, m_nextCenter, m_fromCenter;
    float m_radius = 0.0f, m_nextRadius = 0.0f, m_fromRadius = 0.0f;
    int m_phase = 0;
    bool m_shrinking = false;
    float m_timeLeft = 0.0f;
};

}  // namespace game
