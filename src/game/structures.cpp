#include "game/structures.h"

#include <math.h>

#include "core/i18n.h"
#include "game/character.h"
#include "world/world_units.h"

namespace game {

using math3d::Vec3;

const char* placementErrorName(PlacementError error) {
    switch (error) {
        case PlacementError::None: return "";
        case PlacementError::LimitReached: return i18n::tr(i18n::T::BuildLimit);
        case PlacementError::OverlapsPlayer: return i18n::tr(i18n::T::BuildOverPlayer);
        case PlacementError::OverlapsBot: return i18n::tr(i18n::T::BuildOverBot);
        case PlacementError::OverlapsStructure: return i18n::tr(i18n::T::BuildOverlap);
        case PlacementError::Blocked: return i18n::tr(i18n::T::BuildBlocked);
        case PlacementError::OutOfBounds: return i18n::tr(i18n::T::BuildOutOfBounds);
        case PlacementError::NoMaterials: return i18n::tr(i18n::T::BuildNoMaterials);
    }
    return "?";
}

const char* pieceName(PieceType type) {
    switch (type) {
        case PieceType::Wall: return i18n::tr(i18n::T::PieceWall);
        case PieceType::Floor: return i18n::tr(i18n::T::PieceFloor);
        case PieceType::Ramp: return i18n::tr(i18n::T::PieceRamp);
    }
    return "?";
}

namespace {

world::Aabb shrink(const world::Aabb& b) {
    // Tocar una pieza vecina por el canto no cuenta como choque.
    return {{b.min.x + 0.02f, b.min.y + 0.02f, b.min.z + 0.02f}, {b.max.x - 0.02f, b.max.y - 0.02f, b.max.z - 0.02f}};
}

}  // namespace

int pieceBoxes(const WallPlacement& p, const BuildConfig& config, world::Aabb* out) {
    if (p.type != PieceType::Ramp) {
        out[0] = p.box;
        return 1;
    }
    // Escalon i: el tramo [i, i + 1] de la subida, con el techo a la altura de la rampa en
    // su centro y rampThickness de grosor (sin bajar del suelo, p.box.min.y).
    const float run = config.gridSize / kRampSteps, rise = config.wallHeight / kRampSteps;
    for (int i = 0; i < kRampSteps; ++i) {
        const float top = p.level + (i + 0.5f) * rise;
        const float a = i * run, b = (i + 1) * run;
        world::Aabb s = p.box;
        s.min.y = fmaxf(p.box.min.y, top - config.rampThickness);
        s.max.y = top;
        switch (p.dir) {
            case 0: s.min.x = p.box.min.x + a; s.max.x = p.box.min.x + b; break;
            case 1: s.min.x = p.box.max.x - b; s.max.x = p.box.max.x - a; break;
            case 2: s.min.z = p.box.min.z + a; s.max.z = p.box.min.z + b; break;
            default: s.min.z = p.box.max.z - b; s.max.z = p.box.max.z - a; break;
        }
        out[i] = s;
    }
    return kRampSteps;
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

int StructureSystem::countOwnedBy(int owner) const {
    int n = 0;
    for (const BuiltWall& w : m_walls) {
        if (w.active && (owner < 0 ? w.owner > 0 : w.owner == owner)) ++n;
    }
    return n;
}

bool StructureSystem::limitReached(int owner) const {
    if (owner == 0) return countOwnedBy(0) >= m_config.maxWalls || m_count >= kMaxWallCapacity;
    return countOwnedBy(-1) >= m_config.maxBotWalls || countOwnedBy(owner) >= m_config.maxWallsPerBot ||
           m_count >= kMaxWallCapacity;
}

WallPlacement StructureSystem::plan(const Vec3& feet, float yaw, const world::Aabb& player,
                                    const world::Aabb* bots, int botCount, int owner, PieceType type) const {
    WallPlacement p;
    p.type = type;
    if (type != PieceType::Wall) return planCell(feet, forwardFromYaw(yaw), player, bots, botCount, owner, type);
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
    if (limitReached(owner)) {
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

WallPlacement StructureSystem::planCell(const Vec3& feet, const Vec3& forward, const world::Aabb& player,
                                        const world::Aabb* bots, int botCount, int owner, PieceType type) const {
    // Suelo y rampa: la celda de la rejilla que empieza en la linea donde iria la pared
    // (la primera al menos minDistance por delante), a la altura de los pies redondeada a
    // pisos de wallHeight (desde lo alto de una rampa se sigue un piso mas arriba).
    WallPlacement p;
    p.type = type;
    const float g = m_config.gridSize;
    float x0, z0;
    if (fabsf(forward.x) > fabsf(forward.z)) {
        const float lineX = forward.x > 0.0f ? ceilf((feet.x + m_config.minDistance) / g) * g
                                             : floorf((feet.x - m_config.minDistance) / g) * g;
        x0 = forward.x > 0.0f ? lineX : lineX - g;
        z0 = floorf(feet.z / g) * g;
        p.dir = forward.x > 0.0f ? 0 : 1;
    } else {
        const float lineZ = forward.z > 0.0f ? ceilf((feet.z + m_config.minDistance) / g) * g
                                             : floorf((feet.z - m_config.minDistance) / g) * g;
        z0 = forward.z > 0.0f ? lineZ : lineZ - g;
        x0 = floorf(feet.x / g) * g;
        p.dir = forward.z > 0.0f ? 2 : 3;
    }
    p.alongX = p.dir >= 2;
    const float ground = m_world->supportHeight(x0 + g * 0.5f, z0 + g * 0.5f, 0.1f, feet.y + tuning::kStepHeight);
    const float floors = fmaxf(0.0f, roundf((feet.y - ground) / m_config.wallHeight));
    p.level = ground + floors * m_config.wallHeight;
    if (type == PieceType::Floor) {
        p.box = {{x0, p.level, z0}, {x0 + g, p.level + m_config.floorThickness, z0 + g}};
    } else {
        p.box = {{x0, ground, z0}, {x0 + g, p.level + m_config.wallHeight, z0 + g}};
    }
    world::Aabb boxes[kMaxPieceBoxes];
    const int n = pieceBoxes(p, m_config, boxes);
    p.box = boxes[0];
    for (int i = 1; i < n; ++i) {
        p.box.min = {fminf(p.box.min.x, boxes[i].min.x), fminf(p.box.min.y, boxes[i].min.y), fminf(p.box.min.z, boxes[i].min.z)};
        p.box.max = {fmaxf(p.box.max.x, boxes[i].max.x), fmaxf(p.box.max.y, boxes[i].max.y), fmaxf(p.box.max.z, boxes[i].max.z)};
    }

    const float limit = world::kMapHalfSize - 1.0f;
    if (limitReached(owner)) {
        p.error = PlacementError::LimitReached;
    } else if (p.box.min.x < -limit || p.box.max.x > limit || p.box.min.z < -limit || p.box.max.z > limit) {
        p.error = PlacementError::OutOfBounds;
    } else if (duplicates(p)) {
        p.error = PlacementError::OverlapsStructure;
    } else if (blockedFrom(feet + Vec3{0.0f, tuning::kChestHeight, 0.0f}, p.box)) {
        p.error = PlacementError::Blocked;
    } else {
        // Como en Fortnite, suelos y rampas pueden atravesar el escenario; solo no pueden
        // aparecer encima de nadie.
        for (int i = 0; i < n && p.error == PlacementError::None; ++i) {
            const world::Aabb s = shrink(boxes[i]);
            if (s.overlaps(player)) p.error = PlacementError::OverlapsPlayer;
            for (int b = 0; b < botCount && p.error == PlacementError::None; ++b) {
                if (s.overlaps(bots[b])) p.error = PlacementError::OverlapsBot;
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
    // Suelos y rampas: uno de cada tipo por celda y piso.
    const world::Aabb shrunk = shrink(p.box);
    for (const BuiltWall& w : m_walls) {
        if (!w.active || w.type != p.type || !w.box.overlaps(shrunk)) continue;
        if (p.type != PieceType::Wall || w.alongX == p.alongX) return true;
    }
    return false;
}

int StructureSystem::place(const WallPlacement& placement, int owner) {
    if (!placement.valid || limitReached(owner)) return -1;
    for (int i = 0; i < kMaxWallCapacity; ++i) {
        BuiltWall& w = m_walls[i];
        if (w.active) continue;
        world::Aabb boxes[kMaxPieceBoxes];
        const int n = pieceBoxes(placement, m_config, boxes);
        w.colliderCount = 0;
        for (int b = 0; b < n; ++b) {
            const int collider = m_world->addDynamic(boxes[b], world::kSolid);
            if (collider < 0) {  // no quedan cajas dinamicas: se deshace
                for (int k = 0; k < w.colliderCount; ++k) m_world->removeDynamic(w.colliders[k]);
                w.colliderCount = 0;
                return -1;
            }
            w.colliders[w.colliderCount++] = collider;
        }
        w.active = true;
        w.type = placement.type;
        w.box = placement.box;
        w.alongX = placement.alongX;
        w.dir = placement.dir;
        w.level = placement.level;
        w.material = placement.material;
        w.health = w.maxHealth = placement.material == world::Resource::Metal   ? m_config.metalHealth
                                 : placement.material == world::Resource::Stone ? m_config.stoneHealth
                                                                                : m_config.wallHealth;
        w.sinceHit = 99.0f;
        w.owner = owner;
        ++m_count;
        changed(w.box);
        return i;
    }
    return -1;
}

int StructureSystem::placeForced(const WallPlacement& placement, int owner, float healthScale) {
    if (duplicates(placement) || m_count >= kMaxWallCapacity) return -1;
    WallPlacement p = placement;
    p.valid = true;
    // place() con un dueno negativo no mira los limites de los bots (el de la pieza es suyo).
    const int maxBots = m_config.maxBotWalls;
    m_config.maxBotWalls = kMaxWallCapacity;
    const int maxPerBot = m_config.maxWallsPerBot;
    m_config.maxWallsPerBot = kMaxWallCapacity;
    const int index = place(p, owner);
    m_config.maxBotWalls = maxBots;
    m_config.maxWallsPerBot = maxPerBot;
    if (index >= 0) {
        m_walls[index].maxHealth *= healthScale;
        m_walls[index].health = m_walls[index].maxHealth;
    }
    return index;
}

bool StructureSystem::remove(int index) {
    if (index < 0 || index >= kMaxWallCapacity || !m_walls[index].active) return false;
    BuiltWall& w = m_walls[index];
    for (int k = 0; k < w.colliderCount; ++k) m_world->removeDynamic(w.colliders[k]);
    w.active = false;
    w.colliderCount = 0;
    --m_count;
    changed(w.box);
    return true;
}

void StructureSystem::setHealth(int index, float health) {
    if (index < 0 || index >= kMaxWallCapacity || !m_walls[index].active) return;
    BuiltWall& w = m_walls[index];
    w.health = health < w.maxHealth ? (health > 1.0f ? health : 1.0f) : w.maxHealth;
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
        if (!m_walls[i].active) continue;
        for (int k = 0; k < m_walls[i].colliderCount; ++k) {
            if (m_walls[i].colliders[k] == dynamicHandle) return i;
        }
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
