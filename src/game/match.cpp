#include "game/match.h"

#include <math.h>
#include <stdio.h>

#include "core/timer.h"

namespace game {

using math3d::Vec3;
using namespace tuning;

namespace {

constexpr float kArriveDistance = 1.2f;    // m para dar por alcanzado un punto del camino
constexpr float kStuckWindow = 1.5f;       // s entre comprobaciones de progreso
constexpr float kStuckMinProgress = 0.6f;  // m minimos en esa ventana si quiere moverse
constexpr float kHitHalfWidth = 0.4f;      // caja de impacto de un personaje
constexpr float kHitHeight = 1.85f;
constexpr float kTracerLife = 0.12f;
constexpr float kMinSeparation = 2.0f * kBodyRadius + 0.05f;
constexpr float kSensorDistance = 0.9f;    // sensor de obstaculos delante del bot
constexpr float kSensorTurn = 0.87f;       // ~50 grados

Vec3 flatten(const Vec3& v) { return {v.x, 0.0f, v.z}; }

float distXZ(const Vec3& a, const Vec3& b) {
    const float dx = a.x - b.x, dz = a.z - b.z;
    return sqrtf(dx * dx + dz * dz);
}

Vec3 rotateY(const Vec3& v, float a) {
    const float c = cosf(a), s = sinf(a);
    return {v.x * c + v.z * s, 0.0f, -v.x * s + v.z * c};
}

// Desvia `dir` un angulo aleatorio dentro de un cono de semiapertura `spread` (rad).
Vec3 perturb(const Vec3& dir, float spread, core::Random& rng) {
    const Vec3 up = fabsf(dir.y) < 0.99f ? Vec3{0, 1, 0} : Vec3{1, 0, 0};
    const Vec3 right = math3d::normalize(math3d::cross(dir, up));
    const Vec3 up2 = math3d::cross(right, dir);
    const float angle = spread * sqrtf(rng.uniform());
    const float phi = rng.range(0.0f, 2.0f * math3d::kPi);
    const Vec3 offset = right * cosf(phi) + up2 * sinf(phi);
    return math3d::normalize(dir * cosf(angle) + offset * sinf(angle));
}

world::Aabb bodyBoxAt(const Vec3& feet) {
    return {{feet.x - kBodyRadius, feet.y + kStepHeight, feet.z - kBodyRadius},
            {feet.x + kBodyRadius, feet.y + kBodyHeight, feet.z + kBodyRadius}};
}

}  // namespace

const char* botStateName(BotState state) {
    switch (state) {
        case BotState::Explore: return "Explorar";
        case BotState::Travel: return "Desplazarse";
        case BotState::Search: return "Buscar obj.";
        case BotState::Attack: return "Atacar";
        case BotState::Eliminated: return "Eliminado";
    }
    return "?";
}

const char* useErrorName(UseError error) {
    switch (error) {
        case UseError::None: return "";
        case UseError::FullHealth: return "Vida al maximo";
        case UseError::FullShield: return "Escudo al maximo";
    }
    return "?";
}

const char* matchPhaseName(MatchPhase phase) {
    switch (phase) {
        case MatchPhase::Playing: return "En curso";
        case MatchPhase::Spectating: return "Eliminado";
        case MatchPhase::Finished: return "Terminada";
    }
    return "?";
}

// --- Arranque ---------------------------------------------------------------------------

void Match::start(const MatchConfig& config, world::CollisionWorld& world, NavGrid& nav,
                  const std::vector<Vec3>& pointsOfInterest, const Vec3& playerSpawn,
                  float playerYaw) {
    m_config = config;
    if (m_config.botCount < 0) m_config.botCount = 0;
    if (m_config.botCount > kMaxBots) m_config.botCount = kMaxBots;
    m_world = &world;
    m_nav = &nav;
    m_pois = pointsOfInterest;
    m_rng.reseed(config.seed);

    // Las paredes de la partida anterior se quitan (y su navegacion se restaura) antes
    // de colocar a nadie.
    m_structures.reset(world, nav, m_config.build);
    m_seenStructureRevision = m_structures.revision();

    m_player.reset(playerSpawn, playerYaw, world);
    m_input = PlayerInput{};
    m_aim = PlayerAim{};
    m_commands = PlayerCommands{};
    m_pendingJump = m_pendingPlace = m_pendingRemove = m_pendingReload = false;
    m_pendingPickaxe = m_pendingUse = false;
    m_pendingCycle = 0;
    m_buildMode = false;
    m_inventory.clear();
    for (const MatchConfig::StartItem& item : m_config.startItems) {
        m_inventory.add(item.type, item.count, &m_config.playerWeapon);
    }
    m_inventory.select(0);
    m_materials = Materials{};
    for (int r = 0; r < static_cast<int>(world::Resource::Count); ++r) {
        m_materials.add(static_cast<world::Resource>(r), m_config.startMaterials[r]);
    }
    m_useSlot = -1;
    m_useError = UseError::None;
    m_sinceUseError = 99.0f;
    m_swingCooldown = 0.0f;
    if (m_harvestables) {
        m_harvestLeft.resize(m_harvestables->size());
        for (size_t i = 0; i < m_harvestables->size(); ++i) m_harvestLeft[i] = (*m_harvestables)[i].total;
    }
    m_playerAiming = false;
    m_playerKills = 0;
    for (DamageNumber& d : m_damageNumbers) d.age = 99.0f;
    m_nextDamageNumber = 0;
    m_eventCount = 0;
    m_assistTarget = -1;
    snprintf(m_names[0], sizeof(m_names[0]), "Jugador");

    m_bots.assign(m_config.botCount, Bot{});
    for (int i = 0; i < m_config.botCount; ++i) snprintf(m_names[i + 1], sizeof(m_names[0]), "Bot %d", i + 1);
    spawnBots(playerSpawn);

    m_phase = MatchPhase::Playing;
    m_alive = 1 + m_config.botCount;
    m_winner = -1;
    m_playerPlace = 0;
    m_time = 0.0f;
    m_killFeedCount = 0;
    for (Tracer& t : m_tracers) t.age = kTracerLife;
    m_nextTracer = 0;
    m_sincePlayerHit = m_sincePlayerDamaged = 99.0f;
    m_stats = MatchStats{};
    if (m_alive <= 1) {
        m_phase = MatchPhase::Finished;
        m_winner = 0;
        m_playerPlace = 1;
    }
}

void Match::spawnBots(const Vec3& playerSpawn) {
    const int n = static_cast<int>(m_bots.size());
    for (int i = 0; i < n; ++i) {
        Bot& b = m_bots[i];
        Vec3 spawn{0.0f, 0.0f, 0.0f};
        for (int attempt = 0; attempt < 80; ++attempt) {
            Vec3 p;
            if (!m_nav->randomPointNear({0.0f, 0.0f, 0.0f}, 86.0f, m_rng, &p)) continue;
            // Lejos del jugador y repartidos (se relaja si cuesta encontrar sitio).
            const float minPlayer = attempt < 50 ? 28.0f : 16.0f;
            const float minBots = attempt < 50 ? 14.0f : 6.0f;
            bool ok = distXZ(p, playerSpawn) >= minPlayer;
            for (int j = 0; j < i && ok; ++j) ok = distXZ(p, m_bots[j].body.state().position) >= minBots;
            spawn = p;
            if (ok) break;
        }
        b.body.reset(spawn, m_rng.range(-math3d::kPi, math3d::kPi), *m_world);
        b.state = BotState::Explore;
        b.weapon.reset(m_config.botWeapon);
        // Decisiones escalonadas: no piensan todos en el mismo paso.
        b.decisionTimer = m_config.aiDecisionInterval * static_cast<float>(i + 1) / static_cast<float>(n);
        b.palette = i % 3;
        b.progressAnchor = spawn;
        b.strafeSign = m_rng.chance(0.5f) ? 1.0f : -1.0f;
    }
}

// --- Consultas --------------------------------------------------------------------------

float Match::zoneRadius() const {
    const float t = math3d::clampf(m_time / m_config.zoneShrinkSeconds, 0.0f, 1.0f);
    return m_config.zoneStartRadius + (m_config.zoneEndRadius - m_config.zoneStartRadius) * t;
}

bool Match::alive(int id) const {
    if (id == 0) return m_player.state().health > 0.0f;
    if (id < 1 || id > static_cast<int>(m_bots.size())) return false;
    return m_bots[id - 1].state != BotState::Eliminated;
}

const CharacterBody& Match::body(int id) const { return id == 0 ? m_player : m_bots[id - 1].body; }
CharacterBody& Match::mutableBody(int id) { return id == 0 ? m_player : m_bots[id - 1].body; }
const char* Match::name(int id) const { return (id >= 0 && id < entityCount()) ? m_names[id] : "?"; }

Vec3 Match::eyeOf(int id) const { return body(id).state().position + Vec3{0.0f, kEyeHeight, 0.0f}; }
Vec3 Match::chestOf(int id) const { return body(id).state().position + Vec3{0.0f, kChestHeight, 0.0f}; }

bool Match::lineOfSight(const Vec3& from, const Vec3& to) const {
    return !m_world->raycast(from, to, world::kBlocksShots, nullptr);
}

void Match::teleport(int id, const Vec3& position, float yaw) {
    const float health = mutableBody(id).state().health;  // mover no cura
    mutableBody(id).reset(position, yaw, *m_world);
    mutableBody(id).state().health = health;
    if (id > 0) m_bots[id - 1].progressAnchor = position;
}

int Match::findLockTarget(const Vec3& eye, const Vec3& forward, float maxAngleDegrees,
                          float maxDistance) const {
    int best = -1;
    float bestCos = cosf(math3d::toRadians(maxAngleDegrees));
    for (int id = 1; id < entityCount(); ++id) {
        if (!alive(id)) continue;
        const Vec3 chest = chestOf(id);
        const Vec3 v = chest - eye;
        const float d = math3d::length(v);
        if (d < 0.5f || d > maxDistance) continue;
        const float c = math3d::dot(v * (1.0f / d), forward);
        if (c > bestCos && lineOfSight(eye, chest)) {
            bestCos = c;
            best = id;
        }
    }
    return best;
}

// --- Paso de simulacion -----------------------------------------------------------------

void Match::setPlayerInput(const PlayerInput& input, const PlayerAim& aim,
                           const PlayerCommands& commands) {
    m_input = input;
    m_aim = aim;
    m_commands = commands;
    // Las pulsaciones se entregan en el siguiente paso aunque este frame no tenga ninguno.
    if (input.jumpPressed) m_pendingJump = true;
    if (commands.placeWall) m_pendingPlace = true;
    if (commands.removeWall) m_pendingRemove = true;
    if (commands.reload) m_pendingReload = true;
    if (commands.togglePickaxe) m_pendingPickaxe = !m_pendingPickaxe;
    if (commands.useItem) m_pendingUse = true;
    m_pendingCycle += commands.cycleSlot;
}

void Match::step(float dt) {
    m_time += dt;
    ++m_stats.steps;
    m_pathBudget = 1;  // como mucho un A* por paso de fisica (60 por segundo)
    m_sincePlayerHit += dt;
    m_sincePlayerDamaged += dt;
    for (Tracer& t : m_tracers) t.age += dt;
    for (DamageNumber& d : m_damageNumbers) d.age += dt;
    m_structures.update(dt);

    updatePlayer(dt);
    for (int i = 0; i < static_cast<int>(m_bots.size()); ++i) updateBot(i, dt);
    separateCharacters();
    if (m_structures.revision() != m_seenStructureRevision) {
        m_seenStructureRevision = m_structures.revision();
        onStructuresChanged();
    }
}

void Match::updatePlayer(float dt) {
    const bool place = m_pendingPlace, remove = m_pendingRemove, reload = m_pendingReload;
    const bool togglePickaxe = m_pendingPickaxe;
    const int cycle = m_pendingCycle;
    m_pendingPlace = m_pendingRemove = m_pendingReload = m_pendingPickaxe = false;
    m_pendingCycle = 0;
    m_sinceUseError += dt;
    if (m_swingCooldown > 0.0f) m_swingCooldown -= dt;
    if (Weapon* w = m_inventory.activeWeapon()) w->update(dt);
    if (!alive(0)) {
        cancelUse();
        m_pendingUse = false;
        m_playerAiming = false;
        m_assistTarget = -1;
        return;  // el cuerpo queda donde cayo
    }
    PlayerInput in = m_input;
    in.jumpPressed = m_pendingJump;
    m_pendingJump = false;
    const bool control = m_phase == MatchPhase::Playing || m_phase == MatchPhase::Finished;
    if (!control) in = PlayerInput{};

    // Cambiar lo que se lleva en la mano interrumpe la recarga y el consumible en uso.
    if (control && (cycle != 0 || togglePickaxe)) {
        Weapon* before = m_inventory.activeWeapon();
        const int slotBefore = m_inventory.selected();
        const bool pickaxeBefore = m_inventory.pickaxe();
        if (togglePickaxe) m_inventory.setPickaxe(!m_inventory.pickaxe());
        if (cycle != 0) m_inventory.cycle(cycle);
        if (slotBefore != m_inventory.selected() || pickaxeBefore != m_inventory.pickaxe()) {
            if (before) before->cancelReload();
            cancelUse();
        }
    }
    m_buildMode = control && m_commands.buildMode;
    if (m_buildMode) cancelUse();
    const bool swinging = m_inventory.pickaxe() && m_aim.fire;
    m_playerAiming = in.aiming || (!m_buildMode && ((m_aim.fire && m_inventory.activeWeapon()) || swinging));
    in.aiming = m_playerAiming;

    const u64 t0 = core::nowMicros();
    m_player.step(playerIntent(in), dt, *m_world);
    m_stats.physicsMicros += core::nowMicros() - t0;

    if (!control) {
        m_assistTarget = -1;
        m_pendingUse = false;
        return;
    }
    if (m_buildMode) {
        // Modo construccion: R coloca una pared, CUADRADO quita la apuntada. No se dispara.
        m_assistTarget = -1;
        m_pendingUse = false;
        if (place) buildWall(m_input.cameraYaw);
        if (remove) removeWallAt(m_aim);
        return;
    }
    updatePlayerItems(dt, reload);
}

void Match::updatePlayerItems(float dt, bool reload) {
    const bool use = m_pendingUse;
    m_pendingUse = false;
    if (m_inventory.pickaxe()) {
        // Pico: un golpe cada pickaxeInterval mientras se mantiene R.
        m_assistTarget = -1;
        if (m_aim.fire && m_swingCooldown <= 0.0f && m_phase != MatchPhase::Finished) {
            swingPickaxe();
            m_swingCooldown = m_config.pickaxeInterval;
        }
        return;
    }
    if (Weapon* weapon = m_inventory.activeWeapon()) {
        const bool wasReloading = weapon->reloading();
        if (reload) weapon->startReload();
        // Ayuda de punteria: bot visible mas centrado en un cono estrecho alrededor de la mira.
        const float range = weapon->config().range;
        m_assistTarget = m_config.aimAssistDegrees > 0.0f
                             ? findLockTarget(m_aim.eye, m_aim.direction, m_config.aimAssistDegrees, range)
                             : -1;
        // tryFire() aplica municion y cadencia: mantener R da como mucho un disparo por
        // intervalo (y por paso), nunca uno por frame.
        if (m_aim.fire && m_phase != MatchPhase::Finished && weapon->tryFire()) {
            playerShot(m_aim, weapon->config().damage, range);
        }
        if (!wasReloading && weapon->reloading()) pushEvent(GameEventType::Reload, 0, -1, chestOf(0));
        return;
    }
    // Consumible: R lo empieza a usar; termina tras su tiempo de uso si no se cambia de
    // ranura ni se construye (moverse y recibir dano no lo interrumpen, como en Fortnite).
    m_assistTarget = -1;
    if (use && m_useSlot < 0) startUse();
    if (m_useSlot < 0) return;
    if (m_inventory.selected() != m_useSlot) {
        cancelUse();
        return;
    }
    m_useTimer += dt;
    if (m_useTimer < m_useDuration) return;
    const ItemType type = m_inventory.slot(m_useSlot).type;
    const ItemDef& def = itemDef(type);
    CharacterState& s = m_player.state();
    if (def.health > 0.0f && s.health < def.healthCap) {
        s.health = s.health + def.health > def.healthCap ? def.healthCap : s.health + def.health;
    }
    if (def.shield > 0.0f && s.shield < def.shieldCap) {
        s.shield = s.shield + def.shield > def.shieldCap ? def.shieldCap : s.shield + def.shield;
    }
    m_inventory.consumeOne(m_useSlot);
    m_useSlot = -1;
    pushEvent(GameEventType::ItemUsed, 0, static_cast<int>(type), chestOf(0));
}

bool Match::startUse() {
    if (!alive(0) || m_inventory.pickaxe()) return false;
    const int slot = m_inventory.selected();
    const ItemDef& def = itemDef(m_inventory.slot(slot).type);
    if (def.kind != ItemKind::Consumable) return false;
    const CharacterState& s = m_player.state();
    UseError error = UseError::None;
    if (def.health > 0.0f && s.health >= def.healthCap) error = UseError::FullHealth;
    if (def.shield > 0.0f && s.shield >= def.shieldCap) error = UseError::FullShield;
    if (error != UseError::None) {
        m_useError = error;
        m_sinceUseError = 0.0f;
        return false;
    }
    m_useSlot = slot;
    m_useTimer = 0.0f;
    m_useDuration = def.useSeconds > 0.05f ? def.useSeconds : 0.05f;
    return true;
}

void Match::cancelUse() { m_useSlot = -1; }

const Weapon& Match::playerWeapon() const {
    static const Weapon kNoWeapon;
    if (const Weapon* w = m_inventory.activeWeapon()) return *w;
    if (const Weapon* w = m_inventory.firstWeapon()) return *w;
    return kNoWeapon;
}

void Match::setHarvestables(const std::vector<world::Harvestable>* list) {
    m_harvestables = list;
    m_harvestByCollider.clear();
    m_harvestLeft.clear();
    if (!list) return;
    for (size_t i = 0; i < list->size(); ++i) {
        const int c = (*list)[i].collider;
        if (c >= static_cast<int>(m_harvestByCollider.size())) m_harvestByCollider.resize(c + 1, -1);
        m_harvestByCollider[c] = static_cast<short>(i);
        m_harvestLeft.push_back((*list)[i].total);
    }
}

void Match::swingPickaxe() {
    if (!alive(0)) return;
    // Golpe desde el pecho en la direccion de la mira (a la altura de cajas, rocas y
    // personajes; el rayo desde la camara pasaria por encima). Lo primero que toque.
    const Vec3 chest = chestOf(0);
    const float reach = m_config.pickaxeReach;
    const Vec3 end = chest + m_aim.direction * reach;
    float bestT = 2.0f;
    int victim = -1;
    for (int id = 1; id < entityCount(); ++id) {
        if (!alive(id)) continue;
        float t;
        if (world::segmentVsAabb(chest, end - chest, bodyBounds(id), &t) && t < bestT) {
            bestT = t;
            victim = id;
        }
    }
    world::RayHit hit;
    const bool worldHit = m_world->raycast(chest, end, world::kBlocksShots, &hit) && hit.t < bestT;
    int wall = -1, resource = -1;
    Vec3 point = chest + (end - chest) * bestT;
    if (worldHit) {
        victim = -1;
        point = hit.point;
        wall = m_structures.wallFromCollider(hit.dynamicHandle);
        if (hit.collider >= 0 && hit.collider < static_cast<int>(m_harvestByCollider.size())) {
            resource = m_harvestByCollider[hit.collider];
        }
    } else if (victim < 0) {
        // Los troncos son finos: si el rayo no toca nada, vale lo que este justo delante.
        const Vec3 forward = forwardFromYaw(m_input.cameraYaw);
        const Vec3 c = chest + forward * 1.1f;
        const world::Aabb zone{c - Vec3{0.6f, 0.9f, 0.6f}, c + Vec3{0.6f, 0.9f, 0.6f}};
        float best = 1e9f;
        if (m_harvestables) {
            for (size_t i = 0; i < m_harvestables->size(); ++i) {
                const world::Harvestable& h = (*m_harvestables)[i];
                if (!h.box.overlaps(zone)) continue;
                const Vec3 center = (h.box.min + h.box.max) * 0.5f;
                const float d = distXZ(center, chest);
                if (d >= best) continue;
                // Sin nada en medio: lo primero que toca el rayo es el propio objeto.
                const Vec3 target{center.x, fminf(chest.y, h.box.max.y - 0.2f), center.z};
                world::RayHit blocker;
                if (m_world->raycast(chest, target, world::kBlocksShots, &blocker) && blocker.collider != h.collider) {
                    continue;
                }
                best = d;
                resource = static_cast<int>(i);
                point = target;
            }
        }
    }

    const bool any = victim >= 0 || worldHit || resource >= 0;
    pushEvent(GameEventType::PickaxeSwing, 0, any ? 1 : 0, point);
    if (victim >= 0) {
        m_sincePlayerHit = 0.0f;
        addDamageNumber(chestOf(victim), m_config.pickaxeDamage, false);
        pushEvent(GameEventType::HitEntity, victim, 0, chestOf(victim));
        applyDamage(victim, m_config.pickaxeDamage, 0);
    } else if (wall >= 0) {
        damageWall(wall, m_config.pickaxeStructureDamage, 0, point);
    } else if (resource >= 0) {
        harvest(resource, point);
    }
}

void Match::harvest(int index, const Vec3& point) {
    const world::Harvestable& h = (*m_harvestables)[index];
    int& left = m_harvestLeft[index];
    if (left <= 0) return;  // agotado
    const int take = h.perHit < left ? h.perHit : left;
    left -= take;
    const int added = m_materials.add(h.resource, take);
    m_sincePlayerHit = 0.0f;
    pushEvent(GameEventType::Harvest, 0, static_cast<int>(h.resource), point);
    m_events[(m_eventCount - 1) % kMaxEvents].amount = added;
}

void Match::updateBot(int index, float dt) {
    Bot& b = m_bots[index];
    if (b.state == BotState::Eliminated) return;

    const u64 t0 = core::nowMicros();
    b.stateTime += dt;
    b.weapon.update(dt);
    b.reaction -= dt;
    b.sidestepTimer -= dt;
    b.strafeTimer -= dt;
    b.pathRefresh -= dt;
    b.decisionTimer -= dt;
    if (b.decisionTimer <= 0.0f) {
        decide(index);
        ++m_stats.decisions;
        b.decisionTimer += m_config.aiDecisionInterval;
        if (b.decisionTimer <= 0.0f) b.decisionTimer = m_config.aiDecisionInterval;
    }
    const MoveIntent intent = steer(index, dt);
    const u64 t1 = core::nowMicros();
    b.body.step(intent, dt, *m_world);
    const u64 t2 = core::nowMicros();
    botShoot(index);
    m_stats.aiMicros += (t1 - t0) + (core::nowMicros() - t2);
    m_stats.physicsMicros += t2 - t1;
}

// --- IA: decisiones (cada aiDecisionInterval) -----------------------------------------------

int Match::perceive(int index) const {
    const Bot& b = m_bots[index];
    const int self = index + 1;
    const CharacterState& s = b.body.state();
    const Vec3 eye = eyeOf(self);
    const Vec3 forward = forwardFromYaw(s.yaw);
    const float fovCos = b.state == BotState::Search ? -2.0f : cosf(math3d::toRadians(m_config.fovDegrees * 0.5f));

    int best = -1;
    float bestScore = 1e9f;
    for (int id = 0; id < entityCount(); ++id) {
        if (id == self || !alive(id)) continue;
        const Vec3 p = body(id).state().position;
        const float d = distXZ(p, s.position);
        if (d > m_config.sightRange) continue;
        // Campo de vision (el objetivo actual se sigue aunque salga del cono).
        if (d > 2.5f && id != b.target) {
            const Vec3 dir = flatten(p - s.position) * (1.0f / d);
            if (math3d::dot(dir, forward) < fovCos) continue;
        }
        if (!lineOfSight(eye, chestOf(id))) continue;
        const float score = d * (id == b.target ? 0.6f : 1.0f);
        if (score < bestScore) {
            bestScore = score;
            best = id;
        }
    }
    return best;
}

void Match::chooseDestination(Bot& b) {
    const float radius = zoneRadius();
    Vec3 dest = m_config.zoneCenter;
    bool chosen = false;
    if (!m_pois.empty() && m_rng.chance(0.55f)) {
        for (int attempt = 0; attempt < 6 && !chosen; ++attempt) {
            const Vec3& poi = m_pois[m_rng.index(static_cast<int>(m_pois.size()))];
            if (distXZ(poi, m_config.zoneCenter) > radius + 5.0f) continue;
            chosen = m_nav->randomPointNear(poi, 5.0f, m_rng, &dest);
        }
    }
    if (!chosen) m_nav->randomPointNear(m_config.zoneCenter, radius, m_rng, &dest);
    b.destination = dest;
}

bool Match::requestPath(Bot& b, const Vec3& to) {
    if (m_pathBudget <= 0) return false;
    --m_pathBudget;
    ++m_stats.pathRequests;
    int expansions = 0;
    const int n = m_nav->findPath(b.body.state().position, to, b.path, kMaxPathPoints, &expansions);
    if (expansions > m_stats.maxPathExpansions) m_stats.maxPathExpansions = expansions;
    if (n == 0) {
        ++m_stats.pathFailures;
        b.pathLength = 0;
        return false;
    }
    b.pathLength = n;
    b.pathIndex = 0;
    return true;
}

void Match::decide(int index) {
    Bot& b = m_bots[index];
    const Vec3 pos = b.body.state().position;
    const float interval = m_config.aiDecisionInterval;

    if (b.target >= 0 && !alive(b.target)) {
        b.target = -1;
        b.targetVisible = false;
        if (b.state == BotState::Attack) {
            b.state = BotState::Search;
            b.stateTime = 0.0f;
            b.searchDuration = m_rng.range(1.5f, 3.0f);
            b.searchYaw = b.body.state().yaw;
        }
    }

    // Percepcion: rango + campo de vision + linea de vision (raycast).
    const int seen = perceive(index);
    if (seen >= 0) {
        if (b.state != BotState::Attack || b.target != seen) {
            b.reaction = m_config.reactionTime;
            b.stateTime = 0.0f;
            b.pathLength = 0;  // el camino de exploracion ya no sirve
            b.pathRefresh = 0.0f;
        }
        b.state = BotState::Attack;
        b.target = seen;
        b.targetVisible = true;
        b.breakingWall = false;
        b.lastSeen = body(seen).state().position;
        b.lostTime = 0.0f;
    } else if (b.state == BotState::Attack) {
        b.targetVisible = false;
        b.lostTime += interval;
        // Objetivo escondido tras una pared construida cercana: disparar a la pared.
        b.breakingWall = false;
        if (b.target >= 0 && alive(b.target) && distXZ(pos, body(b.target).state().position) < m_config.attackRange) {
            world::RayHit hit;
            if (m_world->raycast(eyeOf(index + 1), chestOf(b.target), world::kBlocksShots, &hit) &&
                m_structures.wallFromCollider(hit.dynamicHandle) >= 0) {
                b.breakingWall = true;
                b.wallPoint = hit.point;
                b.lostTime = 0.0f;  // sigue atacando mientras haya pared que romper
            }
        }
        if (b.lostTime > 6.0f || b.target < 0) {
            b.state = BotState::Search;
            b.stateTime = 0.0f;
            b.searchDuration = m_rng.range(2.0f, 4.0f);
            b.searchYaw = b.body.state().yaw;
            b.pathLength = 0;
        } else if (b.pathRefresh <= 0.0f) {
            // Va hacia donde lo vio por ultima vez.
            requestPath(b, b.lastSeen);
            b.pathRefresh = 2.0f;
        }
    }

    switch (b.state) {
        case BotState::Explore:
            chooseDestination(b);
            if (requestPath(b, b.destination)) {
                b.state = BotState::Travel;
                b.stateTime = 0.0f;
                b.progressAnchor = pos;
                b.progressTimer = 0.0f;
                b.stuckCount = 0;
            }
            break;
        case BotState::Travel:
            // Una pared nueva (o destruida) ha cambiado la navegacion: camino nuevo.
            if (b.repath) {
                if (!requestPath(b, b.destination)) break;
                b.repath = false;
            }
            // Atajo: si el punto siguiente al actual ya se ve libre, se salta el actual.
            if (b.pathIndex + 1 < b.pathLength &&
                NavGrid::clearWalk(*m_world, pos, b.path[b.pathIndex + 1], kBodyRadius)) {
                ++b.pathIndex;
            }
            if (b.pathIndex >= b.pathLength) {
                b.state = BotState::Search;
                b.stateTime = 0.0f;
                b.searchDuration = m_rng.range(2.0f, 4.0f);
                b.searchYaw = b.body.state().yaw;
            } else if (distXZ(b.destination, m_config.zoneCenter) > zoneRadius() + 6.0f) {
                b.state = BotState::Explore;  // la zona se ha cerrado: otro destino
            }
            break;
        case BotState::Search:
            if (b.stateTime > b.searchDuration) b.state = BotState::Explore;
            break;
        case BotState::Attack:
            if (b.strafeTimer <= 0.0f) {
                b.strafeSign = m_rng.chance(0.5f) ? 1.0f : -1.0f;
                b.strafeTimer = m_rng.range(0.8f, 1.8f);
            }
            if (b.targetVisible) {
                const float d = distXZ(pos, b.lastSeen);
                if (d > m_config.attackRange && b.pathRefresh <= 0.0f &&
                    !NavGrid::clearWalk(*m_world, pos, b.lastSeen, kBodyRadius)) {
                    requestPath(b, b.lastSeen);
                    b.pathRefresh = 2.0f;
                } else if (d <= m_config.attackRange) {
                    b.pathLength = 0;
                }
            }
            break;
        case BotState::Eliminated:
            return;
    }

    // Deteccion de atascos: si quiere ir a algun sitio y apenas avanza, se aparta y salta;
    // a la segunda replanifica desde donde esta; a la tercera cambia de destino.
    const bool wantsToMove = b.pathIndex < b.pathLength &&
                             (b.state == BotState::Travel || b.state == BotState::Attack);
    b.progressTimer += interval;
    if (b.progressTimer >= kStuckWindow) {
        const float moved = distXZ(pos, b.progressAnchor);
        if (wantsToMove && moved < kStuckMinProgress) {
            ++b.stuckCount;
            ++m_stats.stuckEvents;
            b.sidestepTimer = 0.7f;
            b.sidestepSign = m_rng.chance(0.5f) ? 1.0f : -1.0f;
            b.wantsJump = true;
            if (b.stuckCount >= 3) {
                b.stuckCount = 0;
                b.pathLength = 0;
                if (b.state == BotState::Travel) b.state = BotState::Explore;
            } else if (b.stuckCount == 2) {
                requestPath(b, b.state == BotState::Attack ? b.lastSeen : b.destination);
            }
        } else if (moved > 2.0f) {
            b.stuckCount = 0;
        }
        b.progressAnchor = pos;
        b.progressTimer = 0.0f;
    }
}

// --- IA: ejecucion del plan (cada paso, barato) ---------------------------------------------

MoveIntent Match::steer(int index, float dt) {
    Bot& b = m_bots[index];
    const int self = index + 1;
    const CharacterState& s = b.body.state();
    const Vec3 pos = s.position;
    MoveIntent intent;
    Vec3 desired{0.0f, 0.0f, 0.0f};
    float speed = 0.8f;
    b.aiming = false;

    auto followPath = [&]() {
        while (b.pathIndex < b.pathLength && distXZ(pos, b.path[b.pathIndex]) < kArriveDistance) ++b.pathIndex;
        if (b.pathIndex >= b.pathLength) return false;
        desired = math3d::normalize(flatten(b.path[b.pathIndex] - pos));
        return true;
    };

    switch (b.state) {
        case BotState::Travel:
            followPath();
            break;
        case BotState::Attack: {
            const Vec3 to = flatten(b.lastSeen - pos);
            const float d = math3d::length(to);
            intent.faceYawOverride = true;
            intent.faceYaw = yawTowards(pos, b.breakingWall ? b.wallPoint : b.lastSeen);
            b.aiming = b.targetVisible || b.breakingWall;
            if (b.breakingWall) {
                // Quieto disparando a la pared (se aparta si esta pegado a ella).
                const Vec3 toWall = flatten(b.wallPoint - pos);
                const float dw = math3d::length(toWall);
                if (dw < 2.5f && dw > 0.01f) desired = toWall * (-1.0f / dw);
                speed = 0.5f;
            } else if (!b.targetVisible || d > m_config.attackRange) {
                if (!followPath() && d > 1.5f) desired = to * (1.0f / d);
                speed = 0.9f;
            } else if (d > 0.01f) {
                // A distancia de combate: moverse de lado (y alejarse si esta muy cerca).
                desired = Vec3{-to.z, 0.0f, to.x} * (b.strafeSign / d);
                if (d < 7.0f) desired += to * (-0.7f / d);
                speed = 0.55f;
            }
            break;
        }
        case BotState::Search:
            b.searchYaw = wrapAngle(b.searchYaw + 1.4f * dt);
            intent.faceYawOverride = true;
            intent.faceYaw = b.searchYaw;
            break;
        case BotState::Explore:
        case BotState::Eliminated:
            break;
    }

    // Atascado: moverse de lado un momento.
    if (b.sidestepTimer > 0.0f) {
        const Vec3 base = math3d::length(desired) > 0.01f ? desired : forwardFromYaw(s.yaw);
        desired = Vec3{-base.z, 0.0f, base.x} * b.sidestepSign + base * 0.3f;
    }

    // Separacion: no amontonarse con otros personajes.
    for (int id = 0; id < entityCount(); ++id) {
        if (id == self || !alive(id)) continue;
        const Vec3 other = body(id).state().position;
        const float d = distXZ(pos, other);
        if (d < 1.4f && d > 0.001f) desired += flatten(pos - other) * ((1.4f - d) * 1.5f / d);
    }

    // Sensor: si justo delante hay un obstaculo, probar 50 grados a cada lado.
    const float len = math3d::length(desired);
    if (len > 0.05f) {
        Vec3 dir = desired * (1.0f / len);
        if (m_world->overlapsAny(bodyBoxAt(pos + dir * kSensorDistance), world::kBlocksMovement)) {
            const Vec3 left = rotateY(dir, kSensorTurn);
            const Vec3 right = rotateY(dir, -kSensorTurn);
            const bool leftFree = !m_world->overlapsAny(bodyBoxAt(pos + left * kSensorDistance), world::kBlocksMovement);
            const bool rightFree = !m_world->overlapsAny(bodyBoxAt(pos + right * kSensorDistance), world::kBlocksMovement);
            if (leftFree && (!rightFree || b.sidestepSign > 0.0f)) dir = left;
            else if (rightFree) dir = right;
        }
        desired = dir * (len > 1.0f ? 1.0f : len);
    }

    intent.wish = desired;
    intent.speedScale = speed;
    if (b.wantsJump) {
        intent.jumpPressed = true;
        intent.jumpHeld = true;
        b.wantsJump = false;
    }
    return intent;
}

void Match::botShoot(int index) {
    Bot& b = m_bots[index];
    const int self = index + 1;
    if (b.state != BotState::Attack || b.reaction > 0.0f) return;
    if (!alive(b.target) || m_phase == MatchPhase::Finished) return;
    const CharacterState& s = b.body.state();
    if (!b.targetVisible) {
        if (!b.breakingWall) return;
        // Disparo a la pared que tapa al objetivo.
        if (fabsf(wrapAngle(s.yaw - yawTowards(s.position, b.wallPoint))) > 0.45f) return;
        if (!b.weapon.tryFire()) return;
        const Vec3 eye = eyeOf(self);
        const WeaponConfig& weapon = m_config.botWeapon;
        const Vec3 dir = perturb(math3d::normalize(b.wallPoint - eye), math3d::toRadians(m_config.botSpreadDegrees), m_rng);
        fireShot(self, eye, dir, weapon.range, weapon.damage);
        b.weapon.addCooldown(weapon.fireInterval * m_rng.range(-0.15f, 0.25f));
        return;
    }
    const Vec3 targetPos = body(b.target).state().position;
    // Solo dispara si ya esta encarado (no dispara de espaldas).
    if (fabsf(wrapAngle(s.yaw - yawTowards(s.position, targetPos))) > 0.45f) return;

    const Vec3 eye = eyeOf(self);
    const Vec3 aim = chestOf(b.target);
    const float d = math3d::length(aim - eye);
    const WeaponConfig& weapon = m_config.botWeapon;
    if (d > weapon.range) return;
    // El arma decide si puede disparar (cadencia, municion, recarga).
    if (!b.weapon.tryFire()) return;
    float spreadDeg = m_config.botSpreadDegrees + m_config.botSpreadPerMeter * d;
    if (body(b.target).state().horizontalSpeed() > 2.0f) spreadDeg += 1.5f;
    if (s.horizontalSpeed() > 1.0f) spreadDeg += 1.0f;
    const Vec3 dir = perturb(math3d::normalize(aim - eye), math3d::toRadians(spreadDeg), m_rng);
    fireShot(self, eye, dir, weapon.range, weapon.damage);
    b.weapon.addCooldown(weapon.fireInterval * m_rng.range(-0.15f, 0.25f));  // cadencia algo irregular
}

// --- Combate --------------------------------------------------------------------------------

ShotResult Match::fireShot(int shooter, const Vec3& origin, const Vec3& direction, float range,
                           float damage) {
    return resolveShot(shooter, origin, origin + direction * range, damage);
}

ShotResult Match::playerShot(const PlayerAim& aim, float damage, float range) {
    const Vec3 chest = chestOf(0);
    Vec3 dir = aim.direction;
    if (m_assistTarget >= 0) dir = math3d::normalize(chestOf(m_assistTarget) - aim.eye);

    // 1) Rayo desde la camara por el centro de la mira. Empieza a la altura del jugador:
    //    lo que haya entre la camara y el jugador no cuenta.
    const float t0 = fmaxf(0.0f, math3d::dot(chest - aim.eye, dir));
    const Vec3 start = aim.eye + dir * t0;
    const Vec3 end = start + dir * range;
    world::RayHit wallHit;
    float best = m_world->raycast(start, end, world::kBlocksShots, &wallHit) ? wallHit.t : 1.0f;
    const Vec3 seg = end - start;
    for (int id = 1; id < entityCount(); ++id) {
        if (!alive(id)) continue;
        const Vec3 p = body(id).state().position;
        const world::Aabb box{{p.x - kHitHalfWidth, p.y, p.z - kHitHalfWidth},
                              {p.x + kHitHalfWidth, p.y + kHitHeight, p.z + kHitHalfWidth}};
        float t;
        if (world::segmentVsAabb(start, seg, box, &t) && t < best) best = t;
    }
    const Vec3 aimPoint = start + seg * best;

    // 2) La bala sale del arma: si entre el pecho del jugador y ese punto hay un solido
    //    (p. ej. un muro que la camara ve por encima), el disparo termina en el solido.
    Vec3 toTarget = aimPoint - chest;
    const float len = math3d::length(toTarget);
    if (len < 0.05f) return resolveShot(0, chest, chest + dir * range, damage);
    toTarget = toTarget * (1.0f / len);
    return resolveShot(0, chest, aimPoint + toTarget * 0.1f, damage);
}

ShotResult Match::resolveShot(int shooter, const Vec3& origin, const Vec3& end, float damage) {
    ShotResult result;
    const Vec3 seg = end - origin;
    world::RayHit wallHit;
    const bool wall = m_world->raycast(origin, end, world::kBlocksShots, &wallHit);
    float best = wall ? wallHit.t : 1.0f;
    int victim = -1;
    for (int id = 0; id < entityCount(); ++id) {
        if (id == shooter || !alive(id)) continue;
        const Vec3 p = body(id).state().position;
        const world::Aabb box{{p.x - kHitHalfWidth, p.y, p.z - kHitHalfWidth},
                              {p.x + kHitHalfWidth, p.y + kHitHeight, p.z + kHitHalfWidth}};
        float t;
        if (world::segmentVsAabb(origin, seg, box, &t) && t < best) {
            best = t;
            victim = id;
        }
    }
    ++m_stats.shotsFired;
    result.end = origin + seg * best;
    pushEvent(GameEventType::Shot, shooter, -1, origin);
    if (victim >= 0) {
        pushEvent(GameEventType::HitEntity, victim, shooter, result.end);
        ++m_stats.shotsHitEntity;
        result.hitEntity = true;
        result.victim = victim;
        if (shooter == 0) {
            m_sincePlayerHit = 0.0f;
            addDamageNumber(result.end, damage, false);
        }
        applyDamage(victim, damage, shooter);
    } else if (wall) {
        ++m_stats.shotsBlockedByWorld;
        result.blockedByWorld = true;
        // Las paredes construidas pierden vida con cada impacto.
        const int built = m_structures.wallFromCollider(wallHit.dynamicHandle);
        if (built >= 0) damageWall(built, damage, shooter, result.end);
    }
    addTracer(origin, result.end, shooter == 0, victim >= 0);
    if (m_shotObserver) {
        ShotRecord record;
        record.shooter = shooter;
        record.origin = origin;
        record.end = result.end;
        record.victim = victim;
        record.blockedByWorld = result.blockedByWorld;
        m_shotObserver(m_shotObserverContext, record);
    }
    return result;
}

void Match::damageWall(int wall, float amount, int attacker, const Vec3& point) {
    ++m_stats.shotsHitStructure;
    pushEvent(GameEventType::HitStructure, -1, attacker, point);
    if (attacker == 0) addDamageNumber(point, amount, true);
    if (m_structures.damage(wall, amount)) {
        ++m_stats.wallsDestroyed;
        pushEvent(GameEventType::WallBroken, -1, attacker, point);
    }
}

void Match::pushEvent(GameEventType type, int entity, int other, const Vec3& position) {
    GameEvent& e = m_events[m_eventCount % kMaxEvents];
    e.type = type;
    e.entity = entity;
    e.other = other;
    e.amount = 0;
    e.position = position;
    ++m_eventCount;
}

void Match::addDamageNumber(const Vec3& position, float amount, bool structure) {
    DamageNumber& d = m_damageNumbers[m_nextDamageNumber];
    // Pequeno desplazamiento lateral alterno para que los numeros seguidos no se tapen.
    const float side = (m_nextDamageNumber % 3 - 1) * 0.25f;
    m_nextDamageNumber = (m_nextDamageNumber + 1) % kMaxDamageNumbers;
    d.position = position + Vec3{side, 0.3f, 0.0f};
    d.amount = amount;
    d.age = 0.0f;
    d.structure = structure;
}

world::Aabb Match::bodyBounds(int id) const { return body(id).bounds(); }

WallPlacement Match::planWall(float yaw) const {
    world::Aabb bots[kMaxBots];
    int n = 0;
    for (int id = 1; id < entityCount(); ++id) {
        if (alive(id)) bots[n++] = bodyBounds(id);
    }
    WallPlacement p = m_structures.plan(body(0).state().position, yaw, bodyBounds(0), bots, n);
    if (p.valid && m_materials.get(world::Resource::Wood) < m_config.build.wallCost) {
        p.valid = false;
        p.error = PlacementError::NoMaterials;
    }
    return p;
}

int Match::buildWall(float yaw) {
    const int wall = m_structures.place(planWall(yaw));
    if (wall >= 0) {
        m_materials.spend(world::Resource::Wood, m_config.build.wallCost);
        ++m_stats.wallsBuilt;
        const world::Aabb& b = m_structures.wall(wall).box;
        pushEvent(GameEventType::WallBuilt, 0, -1, (b.min + b.max) * 0.5f);
    }
    return wall;
}

int Match::targetedWall(const PlayerAim& aim) const {
    // Desde la altura del jugador a lo largo de la mira, hasta el alcance de construccion.
    const float t0 = fmaxf(0.0f, math3d::dot(chestOf(0) - aim.eye, aim.direction) - 0.5f);
    const Vec3 start = aim.eye + aim.direction * t0;
    return m_structures.wallAlong(start, start + aim.direction * m_config.build.removeRange);
}

bool Match::removeWallAt(const PlayerAim& aim) {
    const int wall = targetedWall(aim);
    if (wall < 0) return false;
    const world::Aabb box = m_structures.wall(wall).box;
    if (!m_structures.remove(wall)) return false;
    ++m_stats.wallsRemoved;
    pushEvent(GameEventType::WallRemoved, 0, -1, (box.min + box.max) * 0.5f);
    return true;
}

void Match::onStructuresChanged() {
    // Los caminos calculados pueden atravesar una pared nueva (o rodear una que ya no
    // esta): los bots que se mueven piden otro en su proxima decision.
    for (Bot& b : m_bots) {
        if (b.state == BotState::Eliminated) continue;
        if (b.state == BotState::Travel) {
            b.repath = true;
            b.pathLength = 0;
        } else if (b.state == BotState::Attack && b.pathLength > 0) {
            b.pathLength = 0;
            b.pathRefresh = 0.0f;
        }
    }
}

void Match::commandBotTravel(int botIndex, const Vec3& destination) {
    Bot& b = m_bots[botIndex];
    b.destination = destination;
    b.state = BotState::Travel;
    b.stateTime = 0.0f;
    b.target = -1;
    b.targetVisible = false;
    b.repath = true;
    b.pathLength = 0;
    b.decisionTimer = 0.0f;
}

void Match::applyDamage(int victim, float amount, int attacker) {
    if (!alive(victim) || amount <= 0.0f) return;
    if (victim == 0 && m_config.invulnerablePlayer) return;
    CharacterState& s = mutableBody(victim).state();
    // El escudo absorbe primero; lo que sobra pasa a la vida.
    const float absorbed = s.shield < amount ? s.shield : amount;
    s.shield -= absorbed;
    s.health -= amount - absorbed;
    if (victim == 0) {
        m_sincePlayerDamaged = 0.0f;
        pushEvent(GameEventType::PlayerHurt, 0, attacker, chestOf(0));
    } else if (attacker >= 0 && attacker != victim && alive(attacker)) {
        // Un bot que recibe un disparo se gira hacia quien le ha dado.
        Bot& b = m_bots[victim - 1];
        if (b.state != BotState::Attack || !b.targetVisible) {
            b.state = BotState::Attack;
            b.target = attacker;
            b.targetVisible = false;
            b.lastSeen = body(attacker).state().position;
            b.lostTime = 0.0f;
            b.stateTime = 0.0f;
            b.pathRefresh = 0.0f;
            b.pathLength = 0;
            if (b.decisionTimer > 0.05f) b.decisionTimer = 0.05f;
        }
    }
    if (s.health <= 0.0f) {
        s.health = 0.0f;
        eliminate(victim, attacker);
    }
}

void Match::eliminate(int victim, int killer) {
    pushEvent(GameEventType::Eliminated, victim, killer, body(victim).state().position);
    if (victim > 0) {
        Bot& b = m_bots[victim - 1];
        b.state = BotState::Eliminated;
        b.eliminatedAt = m_time;
        b.aiming = false;
        b.body.state().velocity = {0.0f, 0.0f, 0.0f};
    } else {
        m_playerPlace = m_alive;  // puesto = vivos antes de caer
    }
    --m_alive;
    if (killer == 0) ++m_playerKills;
    else if (killer > 0 && killer <= static_cast<int>(m_bots.size())) ++m_bots[killer - 1].kills;

    for (int i = kMaxKillFeed - 1; i > 0; --i) m_killFeed[i] = m_killFeed[i - 1];
    m_killFeed[0] = {killer, victim, m_time};
    if (m_killFeedCount < kMaxKillFeed) ++m_killFeedCount;

    if (victim == 0 && m_phase == MatchPhase::Playing) m_phase = MatchPhase::Spectating;
    if (m_alive <= 1) {
        m_phase = MatchPhase::Finished;
        m_winner = -1;
        for (int id = 0; id < entityCount(); ++id) {
            if (alive(id)) m_winner = id;
        }
        if (m_winner == 0) m_playerPlace = 1;
    }
}

void Match::addTracer(const Vec3& from, const Vec3& to, bool byPlayer, bool hit) {
    Tracer& t = m_tracers[m_nextTracer];
    m_nextTracer = (m_nextTracer + 1) % kMaxTracers;
    t.from = from;
    t.to = to;
    t.age = 0.0f;
    t.byPlayer = byPlayer;
    t.hit = hit;
}

void Match::separateCharacters() {
    const int n = entityCount();
    for (int a = 0; a < n; ++a) {
        if (!alive(a)) continue;
        for (int c = a + 1; c < n; ++c) {
            if (!alive(c)) continue;
            CharacterState& sa = mutableBody(a).state();
            CharacterState& sc = mutableBody(c).state();
            if (fabsf(sa.position.y - sc.position.y) > kBodyHeight - 0.1f) continue;
            const float d = distXZ(sa.position, sc.position);
            if (d >= kMinSeparation) continue;
            const Vec3 dir = d > 1e-4f ? flatten(sa.position - sc.position) * (1.0f / d) : Vec3{1.0f, 0.0f, 0.0f};
            const float push = (kMinSeparation - d) * 0.5f;
            sa.position += dir * push;
            sc.position += dir * -push;
            // Que el empujon no meta a nadie dentro de una pared.
            mutableBody(a).depenetrate(*m_world);
            mutableBody(c).depenetrate(*m_world);
        }
    }
}

}  // namespace game
