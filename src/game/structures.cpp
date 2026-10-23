#include "game/structures.h"

#include <math.h>

#include "game/character.h"
#include "world/world_units.h"

namespace game {

using math3d::Vec3;

const char* placementErrorName(PlacementError error) {
    switch (error) {
        case PlacementError::None: return "";
        case PlacementError::LimitReached: return "Limite de paredes alcanzado";
        case PlacementError::OverlapsPlayer: return "Estas dentro de la pared";
        case PlacementError::OverlapsBot: return "Hay alguien en medio";
        case PlacementError::OverlapsStructure: return "Choca con otra estructura";
        case PlacementError::Blocked: return "Hay un obstaculo en medio";
        case PlacementError::OutOfBounds: return "Fuera del mapa";
        case PlacementError::NoMaterials: return "Falta madera (10 por pared)";
    }
    return "?";
}

void StructureSystem::reset(world::CollisionWorld& world, NavGrid& nav, const BuildConfig& config) {
    m_world = &world;
    m_nav = &nav;
    for (int i = 0; i < kMaxWallCapacity; ++i) {
        if (m_walls[i].active) remove(i);
    }
    m_config = config;
    if (m_config.maxWalls > kMaxWallCapacity) m_config.maxWalls = kMaxWallCapacity;
    if (m_config.maxWalls < 0) m_config.maxWalls = 0;
    m_count = 0;
}

WallPlacement StructureSystem::plan(const Vec3& feet, float yaw, const world::Aabb& player,
                                    const world::Aabb* bots, int botCount) const {
    WallPlacement p;
    const float g = m_config.gridSize;
    const float half = m_config.wallThickness * 0.5f;
    const Vec3 forward = forwardFromYaw(yaw);

    // Orientacion: perpendicular a la direccion cardinal mas cercana a la de la camara.
    float lineX = 0.0f, lineZ = 0.0f;
    if (fabsf(forward.x) > fabsf(forward.z)) {
        // Mirando al este/oeste: pared a lo largo de Z, sobre la primera linea de la
        // rejilla que este al menos minDistance por delante.
        p.alongX = false;
        lineX = forward.x > 0.0f ? ceilf((feet.x + m_config.minDistance) / g) * g
                                 : floorf((feet.x - m_config.minDistance) / g) * g;
        const float z0 = floorf(feet.z / g) * g;
        p.box = {{lineX - half, 0.0f, z0}, {lineX + half, 0.0f, z0 + g}};
    } else {
        p.alongX = true;
        lineZ = forward.z > 0.0f ? ceilf((feet.z + m_config.minDistance) / g) * g
                                 : floorf((feet.z - m_config.minDistance) / g) * g;
        const float x0 = floorf(feet.x / g) * g;
        p.box = {{x0, 0.0f, lineZ - half}, {x0 + g, 0.0f, lineZ + half}};
    }

    // Base: el apoyo bajo el centro de la pared, hasta un escalon por encima de los pies
    // (sobre el altillo se construye sobre el altillo; delante de el, en el suelo).
    const float cx = (p.box.min.x + p.box.max.x) * 0.5f;
    const float cz = (p.box.min.z + p.box.max.z) * 0.5f;
    const float base = m_world->supportHeight(cx, cz, 0.1f, feet.y + tuning::kStepHeight);
    p.box.min.y = base;
    p.box.max.y = base + m_config.wallHeight;
    // Bajo un dintel (puertas de las naves) la pared se recorta para encajar.
    const float ceiling = m_world->ceilingHeight(cx, cz, 0.1f, base + m_config.minHeight);
    if (ceiling < p.box.max.y) p.box.max.y = ceiling;

    const float limit = world::kMapHalfSize - 1.0f;
    // Caja algo encogida: tocar una pared vecina por el canto no cuenta como choque.
    const world::Aabb shrunk{{p.box.min.x + 0.02f, p.box.min.y + 0.02f, p.box.min.z + 0.02f},
                             {p.box.max.x - 0.02f, p.box.max.y - 0.02f, p.box.max.z - 0.02f}};
    if (m_count >= m_config.maxWalls) {
        p.error = PlacementError::LimitReached;
    } else if (p.box.min.x < -limit || p.box.max.x > limit || p.box.min.z < -limit || p.box.max.z > limit) {
        p.error = PlacementError::OutOfBounds;
    } else if (shrunk.overlaps(player)) {
        p.error = PlacementError::OverlapsPlayer;
    } else if (m_world->overlapsAnyStatic(shrunk, world::kBlocksMovement) || duplicates(p)) {
        p.error = PlacementError::OverlapsStructure;
    } else if (blockedFrom(feet + Vec3{0.0f, tuning::kChestHeight, 0.0f}, p.box)) {
        p.error = PlacementError::Blocked;
    } else {
        for (int i = 0; i < botCount; ++i) {
            if (shrunk.overlaps(bots[i])) {
                p.error = PlacementError::OverlapsBot;
                break;
            }
        }
    }
    p.valid = p.error == PlacementError::None;
    return p;
}

bool StructureSystem::blockedFrom(const Vec3& chest, const world::Aabb& box) const {
    // Punto de la pared mas cercano al pecho; si un solido corta el segmento, la pared
    // quedaria al otro lado de una pared u obstaculo.
    const Vec3 target{math3d::clampf(chest.x, box.min.x, box.max.x),
                      math3d::clampf(chest.y, box.min.y, box.max.y),
                      math3d::clampf(chest.z, box.min.z, box.max.z)};
    const Vec3 d = target - chest;
    const float len = math3d::length(d);
    if (len < 0.1f) return false;
    return m_world->raycast(chest, chest + d * ((len - 0.05f) / len), world::kBlocksShots, nullptr);
}

bool StructureSystem::duplicates(const WallPlacement& p) const {
    // Las paredes van sobre las lineas de la rejilla y ocupan una celda: dos paredes
    // perpendiculares solo pueden tocarse por los extremos (esquinas, permitido). Dos
    // paralelas solo chocan si son la misma pared.
    const world::Aabb shrunk{{p.box.min.x + 0.02f, p.box.min.y + 0.02f, p.box.min.z + 0.02f},
                             {p.box.max.x - 0.02f, p.box.max.y - 0.02f, p.box.max.z - 0.02f}};
    for (const BuiltWall& w : m_walls) {
        if (w.active && w.alongX == p.alongX && w.box.overlaps(shrunk)) return true;
    }
    return false;
}

int StructureSystem::place(const WallPlacement& placement) {
    if (!placement.valid || m_count >= m_config.maxWalls) return -1;
    for (int i = 0; i < kMaxWallCapacity; ++i) {
        BuiltWall& w = m_walls[i];
        if (w.active) continue;
        const int collider = m_world->addDynamic(placement.box, world::kSolid);
        if (collider < 0) return -1;
        w.active = true;
        w.box = placement.box;
        w.alongX = placement.alongX;
        w.collider = collider;
        w.health = w.maxHealth = m_config.wallHealth;
        w.sinceHit = 99.0f;
        ++m_count;
        changed(w.box);
        return i;
    }
    return -1;
}

bool StructureSystem::remove(int index) {
    if (index < 0 || index >= kMaxWallCapacity || !m_walls[index].active) return false;
    BuiltWall& w = m_walls[index];
    m_world->removeDynamic(w.collider);
    w.active = false;
    w.collider = -1;
    --m_count;
    changed(w.box);
    return true;
}

bool StructureSystem::damage(int index, float amount) {
    if (index < 0 || index >= kMaxWallCapacity || !m_walls[index].active) return false;
    BuiltWall& w = m_walls[index];
    w.health -= amount;
    w.sinceHit = 0.0f;
    if (w.health > 0.0f) return false;
    remove(index);
    return true;
}

void StructureSystem::update(float dt) {
    for (BuiltWall& w : m_walls) {
        if (w.active) w.sinceHit += dt;
    }
}

int StructureSystem::wallFromCollider(int dynamicHandle) const {
    if (dynamicHandle < 0) return -1;
    for (int i = 0; i < kMaxWallCapacity; ++i) {
        if (m_walls[i].active && m_walls[i].collider == dynamicHandle) return i;
    }
    return -1;
}

int StructureSystem::wallAlong(const Vec3& from, const Vec3& to) const {
    world::RayHit hit;
    if (!m_world->raycast(from, to, world::kBlocksShots, &hit)) return -1;
    return wallFromCollider(hit.dynamicHandle);
}

void StructureSystem::changed(const world::Aabb& box) {
    ++m_revision;
    if (m_nav) m_nav->updateRegion(*m_world, box);
}

}  // namespace game
