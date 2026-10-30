// Utilidades internas de game::Match, compartidas por match.cpp (nucleo y combate),
// match_bots.cpp (IA), match_air.cpp (autobus y caida) y match_loot.cpp (botin).
#pragma once

#include <math.h>

#include "core/random.h"
#include "game/character.h"
#include "game/inventory.h"
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
constexpr float kCrouchHitHeight = 1.3f;   // jugador agachado
constexpr float kCrouchChestHeight = 0.75f;
constexpr float kDownedHitHeight = 1.0f;   // derribado, a cuatro patas (equipos, v0.16)
constexpr float kDownedChestHeight = 0.6f;
constexpr float kHeadSize = 0.33f;         // lo mas alto de la caja de impacto es la cabeza
constexpr float kTracerLife = 0.12f;
constexpr float kMinSeparation = 2.0f * kBodyRadius + 0.05f;
constexpr float kSensorDistance = 0.9f;    // sensor de obstaculos delante del bot
constexpr float kSensorTurn = 0.87f;       // ~50 grados

// Puntuacion de un arma para los bots (mejor rareza y tipos mas versatiles primero).
inline float weaponScore(ItemType type, Rarity rarity) {
    float base = 0.0f;
    switch (type) {
        case ItemType::AssaultRifle: base = 30.0f; break;
        case ItemType::BurstRifle: base = 28.0f; break;
        case ItemType::ScopedRifle: base = 27.0f; break;
        case ItemType::Shotgun: base = 27.0f; break;
        case ItemType::TacticalShotgun: base = 26.0f; break;
        case ItemType::Smg: base = 25.0f; break;
        case ItemType::SubmachineGun: base = 24.0f; break;
        case ItemType::RocketLauncher: base = 22.0f; break;
        case ItemType::GrenadeLauncher: base = 20.0f; break;
        case ItemType::Sniper: base = 20.0f; break;
        case ItemType::SemiAutoSniper: base = 21.0f; break;
        case ItemType::Revolver: base = 19.0f; break;
        case ItemType::Pistol: base = 16.0f; break;
        default: return 0.0f;
    }
    return base + 6.0f * static_cast<float>(rarity);
}

// La rareza existente mas cercana (como en el botin).
inline Rarity existingRarity(ItemType type, Rarity rarity) {
    int k = static_cast<int>(rarity);
    while (k < 4 && !weaponHasRarity(type, static_cast<Rarity>(k))) ++k;
    while (k > 0 && !weaponHasRarity(type, static_cast<Rarity>(k))) --k;
    return static_cast<Rarity>(k);
}

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

// Punto de la caja mas cercano a `p` y distancia de `p` a la caja (0 dentro).
inline Vec3 closestOnBox(const Vec3& p, const world::Aabb& b) {
    return {math3d::clampf(p.x, b.min.x, b.max.x), math3d::clampf(p.y, b.min.y, b.max.y),
            math3d::clampf(p.z, b.min.z, b.max.z)};
}
inline float distanceToBox(const Vec3& p, const world::Aabb& b) { return math3d::length(p - closestOnBox(p, b)); }

inline world::Aabb bodyBoxAt(const Vec3& feet) {
    return {{feet.x - kBodyRadius, feet.y + kStepHeight, feet.z - kBodyRadius},
            {feet.x + kBodyRadius, feet.y + kBodyHeight, feet.z + kBodyRadius}};
}


}  // namespace detail
}  // namespace game
