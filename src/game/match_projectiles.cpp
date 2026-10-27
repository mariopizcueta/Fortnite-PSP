// Proyectiles y explosiones de game::Match (parche, fase 8): cohetes del lanzacohetes,
// granadas del lanzagranadas y granadas de mano.
//
//   - Cohete: vuela recto a projectileSpeed y explota al tocar algo (mundo, construccion,
//     personaje, llama) o al recorrer el alcance del arma.
//   - Granada del lanzagranadas: vuela con gravedad, rebota en el mundo y explota al tocar
//     a un personaje o cuando se acaba su espoleta.
//   - Granada de mano: igual, pero solo explota con la espoleta.
//
// La explosion hace dano decreciente con la distancia (100 % en el centro, 40 % en el
// borde) a los personajes que se ven desde el centro (una pared en medio protege, como en
// Fortnite), incluido quien la lanza; y structureDamage a las construcciones, los objetos
// del mapa y la llama dentro del radio.
#include "game/match.h"

#include "game/match_internal.h"

namespace game {

using math3d::Vec3;
using namespace tuning;
using namespace detail;

namespace {

constexpr float kBounce = 0.45f;           // velocidad que conserva al rebotar
constexpr float kGrenadeRadius = 0.08f;    // para no atravesar el suelo
constexpr float kOwnerGrace = 0.15f;       // s en que el proyectil no toca a quien lo lanza
constexpr float kExplosionEdge = 0.4f;     // fraccion del dano en el borde del radio

}  // namespace

int Match::spawnProjectile(int owner, const Vec3& from, const Vec3& direction, const WeaponConfig& weapon,
                           float damageScale, int weaponType) {
    for (int i = 0; i < kMaxProjectiles; ++i) {
        Projectile& p = m_projectiles[i];
        if (p.active) continue;
        p = Projectile{};
        p.active = true;
        p.kind = weapon.projectile;
        p.owner = owner;
        p.weaponType = weaponType;
        p.position = from;
        p.velocity = math3d::normalize(direction) * weapon.projectileSpeed;
        p.damage = weapon.damage * damageScale;
        p.blastRadius = weapon.blastRadius;
        p.structureDamage = weapon.structureDamage;
        p.fuse = weapon.fuseSeconds;
        p.maxDistance = weapon.range;
        return i;
    }
    return -1;  // demasiados en el aire a la vez: no sale (el arma ya ha gastado la municion)
}

void Match::playerLaunch(const WeaponConfig& w, float spreadDegrees, int weaponType) {
    // El proyectil sale del arma hacia donde apunta la mira (el mismo punto que una bala),
    // con la dispersion del arma. Las granadas salen algo hacia arriba (hacen parabola).
    Vec3 dir = m_aim.direction;
    if (spreadDegrees > 0.0f) dir = perturb(dir, math3d::toRadians(spreadDegrees), m_rng);
    const Vec3 chest = chestOf(0);
    const Vec3 target = aimPoint(m_aim.eye, dir, w.range);
    Vec3 launch = math3d::normalize(target - chest);
    if (math3d::length(target - chest) < 0.5f) launch = dir;
    if (w.projectile == ProjectileKind::LauncherGrenade) launch = math3d::normalize(launch + Vec3{0.0f, 0.12f, 0.0f});
    spawnProjectile(0, chest + launch * 0.4f, launch, w, 1.0f, weaponType);
    ++m_stats.shotsFired;
    pushEvent(GameEventType::Shot, 0, weaponType, chest);
    hearShot(0, chest);
}

bool Match::throwGrenade() {
    if (!alive(0) || m_air[0] != AirState::Ground || m_throwCooldown > 0.0f || m_inventory.pickaxe()) return false;
    const int slot = m_inventory.selected();
    if (itemDef(m_inventory.slot(slot).type).kind != ItemKind::Throwable) return false;
    WeaponConfig g = grenadeConfig();
    // Cae donde apunta la mira (como mucho a g.range): parabola con 25 grados mas de
    // elevacion que la mira y la velocidad justa para llegar (entre 6 y 20 m/s).
    const Vec3 from = chestOf(0) + Vec3{0.0f, 0.45f, 0.0f};
    Vec3 target = aimPoint(m_aim.eye, m_aim.direction, g.range);
    // Al suelo bajo ese punto (si apunta a alguien, a sus pies): asi no se pasa rodando.
    target.y = m_world->supportHeight(target.x, target.z, 0.1f, target.y + 0.1f);
    const Vec3 flat = flatten(target - from);
    const float d = math3d::length(flat), h = target.y - from.y;
    const float aimElevation = atan2f(m_aim.direction.y, sqrtf(m_aim.direction.x * m_aim.direction.x +
                                                                m_aim.direction.z * m_aim.direction.z));
    const float theta = math3d::clampf(aimElevation + math3d::toRadians(25.0f), math3d::toRadians(10.0f), math3d::toRadians(60.0f));
    const float c = cosf(theta), denom = 2.0f * c * c * (d * tanf(theta) - h);
    float speed = 20.0f;
    if (d > 0.5f && denom > 0.01f) speed = math3d::clampf(sqrtf(kGrenadeGravity * d * d / denom), 6.0f, 20.0f);
    const Vec3 horizontal = d > 0.01f ? flat * (1.0f / d) : forwardFromYaw(m_input.cameraYaw);
    const Vec3 dir = horizontal * c + Vec3{0.0f, sinf(theta), 0.0f};
    g.projectileSpeed = speed;
    if (spawnProjectile(0, from + dir * 0.3f, dir, g, 1.0f, static_cast<int>(ItemType::Grenade)) < 0) return false;
    if (!m_config.cheats.infiniteAmmo) m_inventory.consumeOne(slot);
    m_throwCooldown = 0.8f;
    pushEvent(GameEventType::Thrown, 0, static_cast<int>(ItemType::Grenade), from);
    return true;
}

void Match::updateProjectiles(float dt) {
    for (Projectile& p : m_projectiles) {
        if (!p.active) continue;
        p.age += dt;
        const bool grenade = p.kind != ProjectileKind::Rocket;
        if (grenade) p.velocity.y -= kGrenadeGravity * dt;
        const Vec3 from = p.position;
        const Vec3 to = from + p.velocity * dt;
        const Vec3 seg = to - from;

        // Personajes (y la llama): un cohete o una granada del lanzagranadas explotan al
        // tocarlos; la granada de mano rebota (lo hace el mundo) y espera a su espoleta.
        float best = 2.0f;
        if (p.kind != ProjectileKind::HandGrenade) {
            for (int id = 0; id < entityCount(); ++id) {
                if (!alive(id) || m_air[id] == AirState::Bus || (id == p.owner && p.age < kOwnerGrace)) continue;
                float t;
                if (world::segmentVsAabb(from, seg, hitBox(id), &t) && t < best) best = t;
            }
            if (llamaStanding()) {
                float t;
                if (world::segmentVsAabb(from, seg, llamaBox(), &t) && t < best) best = t;
            }
        }
        world::RayHit hit;
        const bool wall = m_world->raycast(from, to, world::kBlocksShots, &hit) && hit.t < best;
        if (!wall && best <= 1.0f) {
            explode(from + seg * best, p.owner, p.damage, p.blastRadius, p.structureDamage, p.weaponType);
            p.active = false;
            continue;
        }
        if (!grenade) {
            p.traveled += math3d::length(seg);
            if (wall) {
                // Un poco antes de la pared: la explosion no se mete dentro.
                explode(from + seg * fmaxf(0.0f, hit.t - 0.05f), p.owner, p.damage, p.blastRadius, p.structureDamage,
                        p.weaponType);
                p.active = false;
            } else if (p.traveled >= p.maxDistance) {
                explode(to, p.owner, p.damage, p.blastRadius, p.structureDamage, p.weaponType);
                p.active = false;
            } else {
                p.position = to;
            }
            continue;
        }

        // Granadas: rebotan eje a eje (la velocidad del eje que choca se invierte y se frena).
        Vec3 pos = from;
        const float axes[3] = {seg.x, seg.y, seg.z};
        for (int axis = 0; axis < 3; ++axis) {
            Vec3 next = pos;
            if (axis == 0) next.x += axes[0];
            if (axis == 1) next.y += axes[1];
            if (axis == 2) next.z += axes[2];
            bool blocked = m_world->raycast(pos, next, world::kBlocksShots, nullptr);
            if (axis == 1 && !blocked && axes[1] < 0.0f) {
                // El suelo (no es una caja) y los techos de las cajas bajo la granada.
                const float floor = m_world->supportHeight(next.x, next.z, kGrenadeRadius, pos.y + 0.05f);
                blocked = next.y < floor + kGrenadeRadius;
                if (blocked) pos.y = floor + kGrenadeRadius;
            }
            if (blocked) {
                float* v = axis == 0 ? &p.velocity.x : (axis == 1 ? &p.velocity.y : &p.velocity.z);
                *v = -*v * kBounce;
                // Al botar en el suelo tambien frena en horizontal (rueda poco).
                if (axis == 1) {
                    p.velocity.x *= 0.4f;
                    p.velocity.z *= 0.4f;
                    if (fabsf(p.velocity.y) < 1.0f) p.velocity.y = 0.0f;
                }
            } else {
                pos = next;
            }
        }
        p.position = pos;
        // Rodando por el suelo: rozamiento.
        if (p.velocity.y == 0.0f) {
            const float k = expf(-6.0f * dt);
            p.velocity.x *= k;
            p.velocity.z *= k;
        }
        p.fuse -= dt;
        if (p.fuse <= 0.0f) {
            explode(p.position + Vec3{0.0f, 0.1f, 0.0f}, p.owner, p.damage, p.blastRadius, p.structureDamage, p.weaponType);
            p.active = false;
        } else if (fabsf(p.position.x) > 150.0f || fabsf(p.position.z) > 150.0f || p.position.y < -5.0f) {
            p.active = false;
        }
    }
}

void Match::explode(const Vec3& at, int owner, float damage, float radius, float structureDamage, int weaponType) {
    pushEvent(GameEventType::Explosion, owner, weaponType, at);
    m_events[(m_eventCount - 1) % kMaxEvents].amount = static_cast<int>(radius * 10.0f);
    Explosion& visual = m_explosions[m_nextExplosion];
    m_nextExplosion = (m_nextExplosion + 1) % kMaxExplosions;
    visual.position = at;
    visual.radius = radius;
    visual.age = 0.0f;
    if (radius <= 0.0f) return;
    if (owner == 0 && m_config.cheats.oneShot) damage *= 10.0f;

    // Personajes: dano segun la distancia a su caja, si se ven desde el centro.
    for (int id = 0; id < entityCount(); ++id) {
        if (!alive(id) || m_air[id] == AirState::Bus) continue;
        const world::Aabb box = hitBox(id);
        const float d = distanceToBox(at, box);
        if (d > radius) continue;
        const Vec3 chest = chestOf(id);
        const Vec3 toChest = chest - at;
        const float len = math3d::length(toChest);
        // Empieza unos cm fuera de la superficie que toco el proyectil.
        const Vec3 start = len > 0.2f ? at + toChest * (0.1f / len) : at;
        if (len > 0.3f && m_world->raycast(start, chest, world::kBlocksShots, nullptr)) continue;
        const float amount = damage * (1.0f - (1.0f - kExplosionEdge) * (d / radius));
        if (owner == 0) {
            m_sincePlayerHit = 0.0f;
            if (id != 0) addDamageNumber(chest, amount, false, false, body(id).state().shield > 0.0f);
        }
        pushEvent(GameEventType::HitEntity, id, owner, chest);
        m_damageSource = weaponType;
        applyDamage(id, amount, owner);
        m_damageSource = -1;
    }
    // Construcciones (todas las piezas dentro del radio).
    for (int i = 0; i < m_structures.capacity(); ++i) {
        const BuiltWall& w = m_structures.wall(i);
        if (!w.active || distanceToBox(at, w.box) > radius) continue;
        damageWall(i, structureDamage, owner, at);
    }
    // Objetos del mapa.
    if (m_harvestables) {
        for (size_t i = 0; i < m_harvestables->size(); ++i) {
            if (m_objectDestroyed[i] || distanceToBox(at, (*m_harvestables)[i].box) > radius) continue;
            damageObject(static_cast<int>(i), structureDamage, at, owner);
        }
    }
    // La llama.
    if (llamaStanding() && distanceToBox(at, llamaBox()) <= radius) damageContainer(m_llama, structureDamage, owner, at);
}

}  // namespace game
