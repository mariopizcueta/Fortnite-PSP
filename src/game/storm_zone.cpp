#include "game/storm_zone.h"

#include <math.h>

namespace game {

using math3d::Vec3;

namespace {

constexpr float kStartRadius = 125.0f;  // cubre todo el mapa (la tormenta del borde, a 88 m)
constexpr float kRadii[StormZone::kPhases] = {92.0f, 68.0f, 50.0f, 35.0f, 23.0f, 13.0f, 6.0f, 0.0f};
// Dano por segundo segun los cierres empezados (sube como en Fortnite).
constexpr float kDamage[StormZone::kPhases + 1] = {1.0f, 2.0f, 4.0f, 6.0f, 8.0f, 10.0f, 12.0f, 15.0f, 15.0f};
constexpr float kCenterLimit = 66.0f;  // los centros no se acercan al limite del mapa

}  // namespace

void StormZone::reset(const StormConfig& config, core::Random& rng, const NavGrid* nav) {
    m_config = config;
    m_nav = nav;
    m_phase = 0;
    m_shrinking = false;
    m_center = m_nextCenter = m_fromCenter = config.center;
    m_radius = m_nextRadius = m_fromRadius = config.closes ? kStartRadius : config.startRadius;
    m_timeLeft = config.closes ? config.firstClose : -1.0f;
    if (config.closes) chooseNext(rng);
}

void StormZone::chooseNext(core::Random& rng) {
    // Un circulo menor dentro del actual, con el centro al azar en suelo transitable.
    const float r = kRadii[m_phase];
    const float maxOffset = m_radius - r;
    m_nextRadius = r;
    m_nextCenter = m_center;
    for (int attempt = 0; attempt < 16; ++attempt) {
        const float a = rng.range(0.0f, 2.0f * math3d::kPi);
        const float d = rng.range(0.0f, maxOffset);
        const Vec3 c{m_center.x + cosf(a) * d, 0.0f, m_center.z + sinf(a) * d};
        if (fabsf(c.x) > kCenterLimit || fabsf(c.z) > kCenterLimit) continue;
        int ix, iz;
        if (m_nav && !(m_nav->nearestNode(c, 0, &ix, &iz) && m_nav->walkable(ix, iz))) continue;
        m_nextCenter = c;
        return;
    }
}

bool StormZone::update(float time, core::Random& rng) {
    if (!m_config.closes) {
        const float t = math3d::clampf(time / m_config.endSeconds, 0.0f, 1.0f);
        m_radius = m_nextRadius = m_config.startRadius + (m_config.endRadius - m_config.startRadius) * t;
        return false;
    }
    bool started = false;
    if (!m_shrinking && m_phase < kPhases && time >= startOf(m_phase)) {
        m_fromCenter = m_center;
        m_fromRadius = m_radius;
        m_shrinking = true;
        ++m_phase;
        started = true;
    }
    if (m_shrinking) {
        const float s = (time - startOf(m_phase - 1)) / m_config.shrinkSeconds;
        if (s >= 1.0f) {
            m_center = m_nextCenter;
            m_radius = m_nextRadius;
            m_shrinking = false;
            if (m_phase < kPhases) chooseNext(rng);
        } else {
            m_center = m_fromCenter + (m_nextCenter - m_fromCenter) * s;
            m_radius = m_fromRadius + (m_nextRadius - m_fromRadius) * s;
        }
    }
    m_timeLeft = m_shrinking ? startOf(m_phase - 1) + m_config.shrinkSeconds - time
                             : (m_phase < kPhases ? startOf(m_phase) - time : -1.0f);
    return started;
}

float StormZone::depth(float x, float z) const {
    if (!m_config.closes) return -1000.0f;
    const float dx = x - m_center.x, dz = z - m_center.z;
    return sqrtf(dx * dx + dz * dz) - m_radius;
}

float StormZone::damage() const { return kDamage[m_phase]; }

}  // namespace game
