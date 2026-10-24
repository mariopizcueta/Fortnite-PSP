// Trampas de game::Match (parche, fase 8; docs/TRAMPAS.md).
//
// Se colocan en modo construccion con la cuarta pieza (la trampa) apuntando con la mira a
// una superficie a menos de MatchConfig::trapReach:
//   - piezas construidas: pared (en cualquiera de sus dos caras), suelo (encima = trampa de
//     suelo, debajo = trampa de techo); en las rampas no;
//   - el mapa: el suelo y las caras de las cajas de los edificios (techos, paredes, suelos
//     de los altillos), si la cara es lo bastante grande.
// La placa se ajusta a la cara: en una pieza construida la cubre entera (como en Fortnite);
// en el mapa mide como mucho 2.4 x 2.4 m centrada en el punto apuntado y dentro de la cara.
// No se puede poner donde ya hay otra trampa ni si delante hay algo solido.
//
// Saltan cuando un personaje (que no sea quien la puso) entra en el volumen de delante de
// la placa: dano a todos los que esten dentro, una vez, y despues trapCooldown s de
// espera. Si la pieza o el objeto en que estaba desaparece, la trampa tambien.
#include "game/match.h"

#include "game/match_internal.h"

namespace game {

using math3d::Vec3;
using namespace tuning;
using namespace detail;

namespace {

constexpr float kMapTrapHalf = 1.2f;    // placa en el mapa: 2.4 x 2.4 m como mucho
constexpr float kMinTrapHalf = 0.5f;    // cara mas pequena que 1 x 1 m: no cabe
constexpr float kPieceInset = 0.25f;    // en una pieza construida, margen hasta el borde
constexpr float kFloorReach = 1.2f;     // alto del volumen que activa una trampa de suelo
constexpr float kWallReach = 1.0f;      // y lo que sobresale de una pared o de un techo
constexpr float kCeilingReach = 1.7f;

// Cara de la caja que contiene el punto (la mas cercana), con su normal hacia fuera.
Vec3 faceNormal(const world::Aabb& b, const Vec3& p, const Vec3& rayDir) {
    struct Face {
        float d;
        Vec3 n;
    };
    const Face faces[6] = {{fabsf(p.x - b.min.x), {-1, 0, 0}}, {fabsf(p.x - b.max.x), {1, 0, 0}},
                           {fabsf(p.y - b.min.y), {0, -1, 0}}, {fabsf(p.y - b.max.y), {0, 1, 0}},
                           {fabsf(p.z - b.min.z), {0, 0, -1}}, {fabsf(p.z - b.max.z), {0, 0, 1}}};
    int best = -1;
    for (int i = 0; i < 6; ++i) {
        // Solo las caras que miran hacia quien apunta (el rayo entra por ellas).
        if (math3d::dot(faces[i].n, rayDir) >= 0.0f) continue;
        if (best < 0 || faces[i].d < faces[best].d) best = i;
    }
    return best >= 0 ? faces[best].n : Vec3{0.0f, 1.0f, 0.0f};
}

// Ejes de la placa: U (horizontal) y V = U x N.
void trapAxes(const Vec3& n, Vec3* u) {
    if (fabsf(n.y) > 0.5f) *u = {1.0f, 0.0f, 0.0f};
    else if (fabsf(n.x) > 0.5f) *u = {0.0f, 0.0f, 1.0f};
    else *u = {1.0f, 0.0f, 0.0f};
}

// Caja que engloba la placa y lo que sobresale `depth` metros por delante.
world::Aabb plateBox(const TrapPlacement& p, float depth, float offset) {
    const Vec3 v = math3d::cross(p.axisU, p.normal);
    const Vec3 corners[2] = {p.center + p.axisU * p.halfU + v * p.halfV + p.normal * offset,
                             p.center - p.axisU * p.halfU - v * p.halfV + p.normal * (offset + depth)};
    world::Aabb b;
    b.min = {fminf(corners[0].x, corners[1].x), fminf(corners[0].y, corners[1].y), fminf(corners[0].z, corners[1].z)};
    b.max = {fmaxf(corners[0].x, corners[1].x), fmaxf(corners[0].y, corners[1].y), fmaxf(corners[0].z, corners[1].z)};
    return b;
}

float reachFor(TrapSurface s) {
    return s == TrapSurface::Floor ? kFloorReach : (s == TrapSurface::Wall ? kWallReach : kCeilingReach);
}

}  // namespace

const char* trapErrorName(TrapError error) {
    switch (error) {
        case TrapError::None: return "";
        case TrapError::NoTraps: return "No te quedan trampas";
        case TrapError::NoSurface: return "Apunta a un suelo, una pared o un techo";
        case TrapError::Ramp: return "En una rampa no se puede";
        case TrapError::TooSmall: return "No cabe en esa superficie";
        case TrapError::Occupied: return "Ya hay una trampa ahi";
        case TrapError::Obstructed: return "Hay algo en medio";
        case TrapError::LimitReached: return "Limite de trampas alcanzado";
    }
    return "?";
}

void Match::giveTraps(int n) {
    m_trapStock += n;
    if (m_trapStock > m_config.maxTrapsCarried) m_trapStock = m_config.maxTrapsCarried;
    if (m_trapStock < 0) m_trapStock = 0;
}

TrapPlacement Match::planTrap(const PlayerAim& aim) const {
    TrapPlacement p;
    if (!alive(0) || m_air[0] != AirState::Ground) return p;
    // El rayo de la mira desde la altura del jugador, hasta trapReach del pecho.
    const Vec3 chest = chestOf(0);
    const Vec3 dir = aim.direction;
    const float t0 = fmaxf(0.0f, math3d::dot(chest - aim.eye, dir));
    const Vec3 start = aim.eye + dir * t0;
    const Vec3 end = start + dir * m_config.trapReach;
    world::RayHit hit;
    bool any = m_world->raycast(start, end, world::kBlocksShots, &hit);
    const float groundY = 0.0f;  // world::groundHeightAt: el suelo es plano
    float groundT = dir.y < -1e-3f ? (groundY - start.y) / ((end - start).y) : 2.0f;
    Vec3 rayDir = dir;
    if ((!any || hit.t > 1.0f) && (groundT < 0.0f || groundT > 1.0f)) {
        // La mira no toca nada al alcance (la camara mira casi horizontal): como las piezas
        // de construccion, la trampa va al suelo justo delante, sobre lo que haya debajo.
        const Vec3 feet = body(0).state().position;
        const Vec3 ahead = feet + forwardFromYaw(m_input.cameraYaw) * 2.4f;
        const Vec3 top{ahead.x, feet.y + 1.2f, ahead.z}, bottom{ahead.x, feet.y - 2.0f, ahead.z};
        rayDir = {0.0f, -1.0f, 0.0f};
        any = m_world->raycast(top, bottom, world::kBlocksShots, &hit);
        groundT = 2.0f;
        if (!any) {
            if (feet.y > 0.6f) return p;  // en lo alto de algo sin nada delante
            hit.point = {ahead.x, groundY, ahead.z};
            groundT = 0.0f;
        }
    }

    world::Aabb face;  // caja cuya cara se usa (o el suelo)
    bool piece = false;
    if (any && hit.t <= groundT) {
        const world::Aabb* box = hit.dynamicHandle >= 0 ? m_world->dynamicBox(hit.dynamicHandle)
                                                        : (hit.collider >= 0 ? &m_world->collider(hit.collider).box : nullptr);
        if (!box) return p;
        face = *box;
        p.normal = faceNormal(face, hit.point, rayDir);
        p.center = hit.point;
        if (hit.dynamicHandle >= 0) {
            p.wall = m_structures.wallFromCollider(hit.dynamicHandle);
            if (p.wall < 0) return p;  // otra caja dinamica (no una construccion)
            const BuiltWall& w = m_structures.wall(p.wall);
            if (w.type == PieceType::Ramp) {
                p.error = TrapError::Ramp;
                return p;
            }
            face = w.box;
            piece = true;
        } else if (hit.collider >= 0 && hit.collider < static_cast<int>(m_harvestByCollider.size())) {
            p.object = m_harvestByCollider[hit.collider];
        }
    } else {
        p.center = groundT <= 1.0f && groundT > 0.0f ? start + (end - start) * groundT : hit.point;
        p.center.y = groundY;
        p.normal = {0.0f, 1.0f, 0.0f};
        face = {{p.center.x - kMapTrapHalf, groundY - 1.0f, p.center.z - kMapTrapHalf},
                {p.center.x + kMapTrapHalf, groundY, p.center.z + kMapTrapHalf}};
    }
    p.surface = p.normal.y > 0.5f ? TrapSurface::Floor : (p.normal.y < -0.5f ? TrapSurface::Ceiling : TrapSurface::Wall);
    trapAxes(p.normal, &p.axisU);
    const Vec3 v = math3d::cross(p.axisU, p.normal);
    // Medidas de la cara a lo largo de U y V, y la placa ajustada a ella.
    auto extent = [&](const Vec3& axis, float* lo, float* hi) {
        const Vec3 a{fabsf(axis.x), fabsf(axis.y), fabsf(axis.z)};
        *lo = math3d::dot(face.min, a);
        *hi = math3d::dot(face.max, a);
    };
    float u0, u1, v0, v1;
    extent(p.axisU, &u0, &u1);
    extent(v, &v0, &v1);
    const Vec3 au{fabsf(p.axisU.x), fabsf(p.axisU.y), fabsf(p.axisU.z)}, av{fabsf(v.x), fabsf(v.y), fabsf(v.z)};
    float cu = math3d::dot(p.center, au), cv = math3d::dot(p.center, av);
    if (piece) {
        // La pieza entera (menos un margen): centrada en su cara.
        p.halfU = 0.5f * (u1 - u0) - kPieceInset;
        p.halfV = 0.5f * (v1 - v0) - kPieceInset;
        cu = 0.5f * (u0 + u1);
        cv = 0.5f * (v0 + v1);
    } else {
        p.halfU = fminf(kMapTrapHalf, 0.5f * (u1 - u0) - 0.05f);
        p.halfV = fminf(kMapTrapHalf, 0.5f * (v1 - v0) - 0.05f);
        cu = math3d::clampf(cu, u0 + p.halfU, u1 - p.halfU);
        cv = math3d::clampf(cv, v0 + p.halfV, v1 - p.halfV);
    }
    // Coloca el centro con las coordenadas ajustadas (la de la normal queda en la cara).
    const Vec3 nAbs{fabsf(p.normal.x), fabsf(p.normal.y), fabsf(p.normal.z)};
    p.center = au * cu + av * cv + nAbs * math3d::dot(p.center, nAbs);
    if (p.halfU < kMinTrapHalf || p.halfV < kMinTrapHalf) {
        p.error = TrapError::TooSmall;
        return p;
    }
    // Ya hay una trampa en esa cara, o algo solido justo delante.
    for (const Trap& t : m_traps) {
        if (!t.active || math3d::dot(t.place.normal, p.normal) < 0.9f) continue;
        if (plateBox(t.place, 0.1f, -0.05f).overlaps(plateBox(p, 0.1f, -0.05f))) {
            p.error = TrapError::Occupied;
            return p;
        }
    }
    world::Aabb front = plateBox(p, 0.35f, 0.06f);
    front.min = front.min + Vec3{0.1f, 0.1f, 0.1f} * 0.5f;
    front.max = front.max - Vec3{0.1f, 0.1f, 0.1f} * 0.5f;
    if (m_world->overlapsAny(front, world::kBlocksMovement)) {
        p.error = TrapError::Obstructed;
        return p;
    }
    int active = 0;
    for (const Trap& t : m_traps) active += t.active ? 1 : 0;
    if (active >= kMaxTraps) p.error = TrapError::LimitReached;
    else if (m_trapStock <= 0) p.error = TrapError::NoTraps;
    else p.error = TrapError::None;
    p.valid = p.error == TrapError::None;
    return p;
}

int Match::placeTrap(const PlayerAim& aim) {
    const TrapPlacement p = planTrap(aim);
    if (!p.valid) return -1;
    for (int i = 0; i < kMaxTraps; ++i) {
        Trap& t = m_traps[i];
        if (t.active) continue;
        t = Trap{};
        t.active = true;
        t.place = p;
        t.owner = 0;
        if (p.wall >= 0) t.wallBox = m_structures.wall(p.wall).box;
        t.trigger = plateBox(p, reachFor(p.surface), 0.0f);
        --m_trapStock;
        pushEvent(GameEventType::TrapPlaced, 0, i, p.center);
        return i;
    }
    return -1;
}

void Match::updateTraps(float dt) {
    for (int i = 0; i < kMaxTraps; ++i) {
        Trap& t = m_traps[i];
        if (!t.active) continue;
        // La pieza (o el objeto del mapa) en que estaba ya no esta: la trampa cae con ella.
        bool gone = false;
        if (t.place.wall >= 0) {
            const BuiltWall& w = m_structures.wall(t.place.wall);
            gone = !w.active || math3d::length(w.box.min - t.wallBox.min) > 0.01f ||
                   math3d::length(w.box.max - t.wallBox.max) > 0.01f;
        }
        if (t.place.object >= 0 && m_objectDestroyed[t.place.object]) gone = true;
        if (gone) {
            t.active = false;
            pushEvent(GameEventType::TrapRemoved, t.owner, i, t.place.center);
            continue;
        }
        t.sinceTrigger += dt;
        if (t.cooldown > 0.0f) {
            t.cooldown -= dt;
            continue;
        }
        // Quien este dentro del volumen (salvo quien la puso): dano una vez a cada uno.
        int victims[kMaxBots + 1];
        int n = 0;
        for (int id = 0; id < entityCount(); ++id) {
            if (id == t.owner || !alive(id) || m_air[id] == AirState::Bus) continue;
            if (hitBox(id).overlaps(t.trigger)) victims[n++] = id;
        }
        if (n == 0) continue;
        t.cooldown = m_config.trapCooldown;
        t.sinceTrigger = 0.0f;
        pushEvent(GameEventType::TrapTriggered, t.owner, i, t.place.center);
        m_events[(m_eventCount - 1) % kMaxEvents].amount = n;
        for (int k = 0; k < n; ++k) {
            const int id = victims[k];
            if (t.owner == 0) {
                m_sincePlayerHit = 0.0f;
                addDamageNumber(chestOf(id), m_config.trapDamage, false);
            }
            pushEvent(GameEventType::HitEntity, id, t.owner, chestOf(id));
            applyDamage(id, m_config.trapDamage, t.owner);
        }
    }
}

}  // namespace game
