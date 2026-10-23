// Utilidades internas de game::Match, compartidas por match.cpp (nucleo y combate),
// match_bots.cpp (IA), match_air.cpp (autobus y caida) y match_loot.cpp (botin).
#pragma once

#include <math.h>

#include "core/random.h"
#include "game/character.h"
#include "world/collision.h"

namespace game {
namespace detail {

using math3d::Vec3;
using namespace tuning;

constexpr float kArriveDistance = 1.2f;    // m para dar por alcanzado un punto del camino
constexpr float kStuckWindow = 1.5f;       // s entre comprobaciones de progreso
constexpr float kStuckMinProgress = 0.6f;  // m minimos en esa ventana si quiere moverse
constexpr float kHitHalfWidth = 0.4f;      // caja de impacto de un personaje
constexpr float kHitHeight = 1.85f;
constexpr float kTracerLife = 0.12f;
constexpr float kMinSeparation = 2.0f * kBodyRadius + 0.05f;
constexpr float kSensorDistance = 0.9f;    // sensor de obstaculos delante del bot
constexpr float kSensorTurn = 0.87f;       // ~50 grados

inline Vec3 flatten(const Vec3& v) { return {v.x, 0.0f, v.z}; }

inline float distXZ(const Vec3& a, const Vec3& b) {
    const float dx = a.x - b.x, dz = a.z - b.z;
    return sqrtf(dx * dx + dz * dz);
}

inline Vec3 rotateY(const Vec3& v, float a) {
    const float c = cosf(a), s = sinf(a);
    return {v.x * c + v.z * s, 0.0f, -v.x * s + v.z * c};
}

// Desvia `dir` un angulo aleatorio dentro de un cono de semiapertura `spread` (rad).
inline Vec3 perturb(const Vec3& dir, float spread, core::Random& rng) {
    const Vec3 up = fabsf(dir.y) < 0.99f ? Vec3{0, 1, 0} : Vec3{1, 0, 0};
    const Vec3 right = math3d::normalize(math3d::cross(dir, up));
    const Vec3 up2 = math3d::cross(right, dir);
    const float angle = spread * sqrtf(rng.uniform());
    const float phi = rng.range(0.0f, 2.0f * math3d::kPi);
    const Vec3 offset = right * cosf(phi) + up2 * sinf(phi);
    return math3d::normalize(dir * cosf(angle) + offset * sinf(angle));
}

inline world::Aabb bodyBoxAt(const Vec3& feet) {
    return {{feet.x - kBodyRadius, feet.y + kStepHeight, feet.z - kBodyRadius},
            {feet.x + kBodyRadius, feet.y + kBodyHeight, feet.z + kBodyRadius}};
}


}  // namespace detail
}  // namespace game
