#include "game/match.h"

#include <math.h>
#include <stdio.h>

#include "core/timer.h"
#include "game/match_internal.h"
#include "world/storm.h"

namespace game {

using math3d::Vec3;
using namespace tuning;
using namespace detail;


const char* botStateName(BotState state) {
    switch (state) {
        case BotState::Explore: return "Explorar";
        case BotState::Travel: return "Desplazarse";
        case BotState::Search: return "Buscar obj.";
        case BotState::Attack: return "Atacar";
        case BotState::Loot: return "Botin";
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
    m_buildMaterial = world::Resource::Wood;
    m_inventory.clear();
    for (const MatchConfig::StartItem& item : m_config.startItems) {
        if (item.type != ItemType::None) m_inventory.add(item.type, item.rarity, item.count);
    }
    for (int a = 0; a < static_cast<int>(AmmoType::Count); ++a) {
        m_inventory.addAmmo(static_cast<AmmoType>(a), m_config.startAmmo[a]);
    }
    m_inventory.select(0);
    // Sin nada en las manos (como al saltar del autobus en Fortnite): el pico.
    if (m_inventory.slot(0).type == ItemType::None) m_inventory.setPickaxe(true);
    m_materials = Materials{};
    for (int r = 0; r < static_cast<int>(world::Resource::Count); ++r) {
        m_materials.add(static_cast<world::Resource>(r), m_config.startMaterials[r]);
    }
    m_useSlot = -1;
    m_useError = UseError::None;
    m_sinceUseError = 99.0f;
    m_swingCooldown = 0.0f;
    if (m_harvestables) {
        // Todo vuelve a estar en pie: las cajas de lo destruido colisionan otra vez y la
        // navegacion de esas zonas se recalcula.
        for (size_t i = 0; i < m_harvestables->size(); ++i) {
            const world::Harvestable& h = (*m_harvestables)[i];
            m_objectHealth[i] = h.health;
            if (!m_objectDestroyed[i]) continue;
            m_objectDestroyed[i] = 0;
            for (int k = 0; k < h.colliderCount; ++k) m_world->setStaticEnabled(h.collider + k, true);
            m_nav->updateRegion(*m_world, h.box);
        }
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
    // La tormenta tiene su propio generador: no cambia el resto de la partida.
    StormConfig storm;
    storm.closes = m_config.stormCloses;
    storm.firstClose = m_config.stormFirstClose;
    storm.phaseSeconds = m_config.stormPhaseSeconds;
    storm.shrinkSeconds = m_config.stormShrinkSeconds;
    storm.center = m_config.zoneCenter;
    storm.startRadius = m_config.zoneStartRadius;
    storm.endRadius = m_config.zoneEndRadius;
    storm.endSeconds = m_config.zoneShrinkSeconds;
    m_stormRng.reseed(config.seed * 2654435761u + 17u);
    m_storm.reset(storm, m_stormRng, &nav);
    m_killFeedCount = 0;
    for (Tracer& t : m_tracers) t.age = kTracerLife;
    m_nextTracer = 0;
    m_sincePlayerHit = m_sincePlayerDamaged = 99.0f;
    m_stats = MatchStats{};
    for (int i = 0; i <= kMaxBots; ++i) {
        m_inStorm[i] = false;
        m_stormTimer[i] = 0.0f;
        m_air[i] = AirState::Ground;
    }
    // Botin repartido por el mapa y, si se empieza en el autobus, todos dentro.
    spawnLoot();
    if (m_config.startInBus) startBus();
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
        b.armed = m_config.botsStartArmed;  // si no, aterriza solo con el pico y busca botin
        // Decisiones escalonadas: no piensan todos en el mismo paso.
        b.decisionTimer = m_config.aiDecisionInterval * static_cast<float>(i + 1) / static_cast<float>(n);
        b.palette = i % 3;
        b.progressAnchor = spawn;
        b.strafeSign = m_rng.chance(0.5f) ? 1.0f : -1.0f;
    }
}

// --- Consultas --------------------------------------------------------------------------

float Match::stormDepthAt(float x, float z) const { return fmaxf(world::stormDepth(x, z), m_storm.depth(x, z)); }

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
    const float shield = mutableBody(id).state().shield;
    mutableBody(id).reset(position, yaw, *m_world);
    mutableBody(id).state().health = health;
    mutableBody(id).state().shield = shield;
    m_air[id] = AirState::Ground;
    if (id > 0) m_bots[id - 1].progressAnchor = position;
}

int Match::findLockTarget(const Vec3& eye, const Vec3& forward, float maxAngleDegrees,
                          float maxDistance) const {
    int best = -1;
    float bestCos = cosf(math3d::toRadians(maxAngleDegrees));
    for (int id = 1; id < entityCount(); ++id) {
        if (!alive(id) || m_air[id] == AirState::Bus) continue;
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
    m_buildPiece = commands.buildPiece;
    if (commands.cycleMaterial) {
        m_buildMaterial = static_cast<world::Resource>((static_cast<int>(m_buildMaterial) + 1) %
                                                       static_cast<int>(world::Resource::Count));
    }
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
    updateAirdrops(dt);

    updatePlayer(dt);
    autoPickupAmmo(0);
    for (int i = 0; i < static_cast<int>(m_bots.size()); ++i) updateBot(i, dt);
    separateCharacters();
    if (m_storm.update(m_time, m_stormRng)) pushEvent(GameEventType::StormClosing, -1, m_storm.phase(), m_storm.nextCenter());
    updateStorm(dt);
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
    if (Weapon* w = m_inventory.activeWeapon()) {
        // La reserva es la municion comun del inventario (la recarga la gasta).
        const AmmoType ammo = static_cast<AmmoType>(w->config().ammoType);
        w->setReserve(m_inventory.ammo(ammo));
        w->update(dt);
        m_inventory.setAmmo(ammo, w->reserve());
    }
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

    // En el autobus o cayendo: X salta / abre el ala delta y el stick dirige la caida.
    if (m_air[0] != AirState::Ground) {
        updateAirState(0, control && in.jumpPressed);
        m_buildMode = false;
        m_playerAiming = false;
        m_assistTarget = -1;
        m_pendingUse = false;
        cancelUse();
        if (m_air[0] == AirState::Bus) return;
        in.jumpPressed = false;
        const u64 t0 = core::nowMicros();
        m_player.step(airIntent(0, playerIntent(in).wish, false), dt, *m_world);
        m_stats.physicsMicros += core::nowMicros() - t0;
        updateAirState(0, false);  // aterrizaje
        return;
    }

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
        if (place) buildWall(m_input.cameraYaw, m_buildPiece);
        if (remove) removeWallAt(m_aim);
        return;
    }
    updatePlayerItems(dt, reload);
}

void Match::updatePlayerItems(float dt, bool reload) {
    const bool use = m_pendingUse;
    m_pendingUse = false;
    // CUADRADO: abrir / recoger lo que haya al alcance; si no hay nada, recargar.
    if (reload) {
        const Interactable target = interactableFor(0);
        if (target.kind != Interactable::Kind::None) {
            interact(0, target);
            reload = false;
        }
    }
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
        const AmmoType ammo = static_cast<AmmoType>(weapon->config().ammoType);
        weapon->setReserve(m_inventory.ammo(ammo));
        const bool wasReloading = weapon->reloading();
        if (reload) weapon->startReload();
        // Ayuda de punteria: bot visible mas centrado en un cono estrecho alrededor de la mira.
        const float range = weapon->config().range;
        m_assistTarget = m_config.aimAssistDegrees > 0.0f
                             ? findLockTarget(m_aim.eye, m_aim.direction, m_config.aimAssistDegrees, range)
                             : -1;
        // tryFire() aplica municion y cadencia: mantener R da como mucho un disparo por
        // intervalo (y por paso), nunca uno por frame.
        if (m_aim.fire && m_phase != MatchPhase::Finished) {
            const float spread = playerSpread(weapon->config());  // antes de tryFire (primer disparo)
            if (weapon->tryFire()) {
                const WeaponConfig& w = weapon->config();
                const int type = static_cast<int>(m_inventory.activeType());
                for (int pellet = 0; pellet < w.pellets; ++pellet) {
                    playerShot(m_aim, w.damage / static_cast<float>(w.pellets), range, spread, &w, pellet == 0, type);
                }
            }
        }
        if (!wasReloading && weapon->reloading()) pushEvent(GameEventType::Reload, 0, -1, chestOf(0));
        m_inventory.setAmmo(ammo, weapon->reserve());
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
    pushEvent(GameEventType::UseStarted, 0, static_cast<int>(m_inventory.slot(slot).type), chestOf(0));
    return true;
}

void Match::cancelUse() {
    if (m_useSlot < 0) return;
    pushEvent(GameEventType::UseCanceled, 0, static_cast<int>(m_inventory.slot(m_useSlot).type), chestOf(0));
    m_useSlot = -1;
}

float Match::playerSpread(const WeaponConfig& w) const {
    // Dispersion (grados): apuntando con L, la de apuntar; si no, la de cadera. Moverse y
    // saltar la aumentan. Quieto, apuntando y sin haber disparado en 0.35 s, el primer
    // disparo va exacto (como en Fortnite). La escopeta siempre abre su cono.
    if (!m_config.weaponSpread) return 0.0f;
    const CharacterState& s = m_player.state();
    const bool aiming = m_input.aiming;
    float spread = aiming ? w.aimSpread : w.hipSpread;
    if (s.horizontalSpeed() > 1.0f) spread += 1.0f;
    if (!s.grounded()) spread += 3.0f;
    const Weapon* weapon = m_inventory.activeWeapon();
    if (w.pellets == 1 && aiming && s.horizontalSpeed() < 0.5f && s.grounded() && weapon && weapon->sinceShot() > 0.35f) {
        spread = 0.0f;
    }
    return spread;
}

int Match::playerReserve() const {
    const Weapon& w = playerWeapon();
    return m_inventory.ammo(static_cast<AmmoType>(w.config().ammoType));
}

const Weapon& Match::playerWeapon() const {
    static const Weapon kNoWeapon;
    if (const Weapon* w = m_inventory.activeWeapon()) return *w;
    if (const Weapon* w = m_inventory.firstWeapon()) return *w;
    return kNoWeapon;
}

void Match::setHarvestables(const std::vector<world::Harvestable>* list) {
    m_harvestables = list;
    m_harvestByCollider.clear();
    m_objectHealth.clear();
    m_objectDestroyed.clear();
    if (!list) return;
    for (size_t i = 0; i < list->size(); ++i) {
        const world::Harvestable& h = (*list)[i];
        const int last = h.collider + h.colliderCount - 1;
        if (last >= static_cast<int>(m_harvestByCollider.size())) m_harvestByCollider.resize(last + 1, -1);
        for (int c = h.collider; c <= last; ++c) m_harvestByCollider[c] = static_cast<short>(i);
        m_objectHealth.push_back(h.health);
        m_objectDestroyed.push_back(0);
    }
}

int Match::destroyedObjectCount() const {
    int n = 0;
    for (u8 d : m_objectDestroyed) n += d;
    return n;
}

bool Match::damageObject(int index, float amount, const Vec3& point, int by) {
    if (!m_harvestables || index < 0 || index >= static_cast<int>(m_objectHealth.size()) || m_objectDestroyed[index]) {
        return false;
    }
    m_objectHealth[index] -= amount;
    if (m_objectHealth[index] > 0.0f) return false;
    // Sin vida: deja de colisionar y de dibujarse; los bots buscan caminos nuevos.
    const world::Harvestable& h = (*m_harvestables)[index];
    m_objectHealth[index] = 0.0f;
    m_objectDestroyed[index] = 1;
    for (int k = 0; k < h.colliderCount; ++k) m_world->setStaticEnabled(h.collider + k, false);
    m_nav->updateRegion(*m_world, h.box);
    onStructuresChanged();
    pushEvent(GameEventType::ObjectDestroyed, by, index, point);
    return true;
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
                if (m_objectDestroyed[i]) continue;
                world::RayHit blocker;
                if (m_world->raycast(chest, target, world::kBlocksShots, &blocker) && !h.owns(blocker.collider)) {
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
    // Cada golpe da material y quita vida al objeto; sin vida, desaparece.
    if (m_objectDestroyed[index]) return;
    const world::Harvestable& h = (*m_harvestables)[index];
    const int added = m_materials.add(h.resource, h.perHit);
    m_sincePlayerHit = 0.0f;
    pushEvent(GameEventType::Harvest, 0, static_cast<int>(h.resource), point);
    m_events[(m_eventCount - 1) % kMaxEvents].amount = added;
    damageObject(index, m_config.pickaxeObjectDamage, point, 0);
}

// --- Combate --------------------------------------------------------------------------------

ShotResult Match::fireShot(int shooter, const Vec3& origin, const Vec3& direction, float range,
                           float damage) {
    return resolveShot(shooter, origin, origin + direction * range, damage);
}

ShotResult Match::playerShot(const PlayerAim& aim, float damage, float range, float spreadDegrees,
                             const WeaponConfig* weapon, bool emitShot, int weaponType) {
    const Vec3 chest = chestOf(0);
    Vec3 dir = aim.direction;
    if (m_assistTarget >= 0) dir = math3d::normalize(chestOf(m_assistTarget) - aim.eye);
    // Dispersion del arma (despues de la ayuda de punteria: la ayuda no la anula).
    if (spreadDegrees > 0.0f) dir = perturb(dir, math3d::toRadians(spreadDegrees), m_rng);

    // 1) Rayo desde la camara por el centro de la mira. Empieza a la altura del jugador:
    //    lo que haya entre la camara y el jugador no cuenta.
    const float t0 = fmaxf(0.0f, math3d::dot(chest - aim.eye, dir));
    const Vec3 start = aim.eye + dir * t0;
    const Vec3 end = start + dir * range;
    world::RayHit wallHit;
    float best = m_world->raycast(start, end, world::kBlocksShots, &wallHit) ? wallHit.t : 1.0f;
    const Vec3 seg = end - start;
    for (int id = 1; id < entityCount(); ++id) {
        if (!alive(id) || m_air[id] == AirState::Bus) continue;
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
    if (len < 0.05f) return resolveShot(0, chest, chest + dir * range, damage, weapon, emitShot, weaponType);
    toTarget = toTarget * (1.0f / len);
    return resolveShot(0, chest, aimPoint + toTarget * 0.1f, damage, weapon, emitShot, weaponType);
}

void Match::fireWeapon(int shooter, const Vec3& origin, const Vec3& direction, const WeaponConfig& weapon,
                       float spreadDegrees, float damageScale, int weaponType) {
    // Un disparo (un sonido); la escopeta reparte el dano entre sus perdigones.
    const int pellets = weapon.pellets > 0 ? weapon.pellets : 1;
    const float spread = weapon.pellets > 1 ? fmaxf(spreadDegrees, weapon.hipSpread) : spreadDegrees;
    for (int i = 0; i < pellets; ++i) {
        const Vec3 dir = perturb(direction, math3d::toRadians(spread), m_rng);
        resolveShot(shooter, origin, origin + dir * weapon.range, weapon.damage * damageScale / static_cast<float>(pellets),
                    &weapon, i == 0, weaponType);
    }
}

ShotResult Match::resolveShot(int shooter, const Vec3& origin, const Vec3& end, float damage,
                              const WeaponConfig* weapon, bool emitShot, int weaponType) {
    ShotResult result;
    const Vec3 seg = end - origin;
    world::RayHit wallHit;
    const bool wall = m_world->raycast(origin, end, world::kBlocksShots, &wallHit);
    float best = wall ? wallHit.t : 1.0f;
    int victim = -1;
    for (int id = 0; id < entityCount(); ++id) {
        if (id == shooter || !alive(id) || m_air[id] == AirState::Bus) continue;
        const Vec3 p = body(id).state().position;
        const world::Aabb box{{p.x - kHitHalfWidth, p.y, p.z - kHitHalfWidth},
                              {p.x + kHitHalfWidth, p.y + kHitHeight, p.z + kHitHalfWidth}};
        float t;
        if (world::segmentVsAabb(origin, seg, box, &t) && t < best) {
            best = t;
            victim = id;
        }
    }
    result.end = origin + seg * best;
    if (emitShot) {
        ++m_stats.shotsFired;
        pushEvent(GameEventType::Shot, shooter, weaponType, origin);
        if (shooter >= 0) hearShot(shooter, origin);
    }
    // Caida del dano con la distancia (escopeta y subfusil).
    if (weapon && weapon->falloffEnd > weapon->falloffStart) {
        const float d = math3d::length(result.end - origin);
        const float t = math3d::clampf((d - weapon->falloffStart) / (weapon->falloffEnd - weapon->falloffStart), 0.0f, 1.0f);
        damage *= 1.0f + (weapon->falloffMin - 1.0f) * t;
    }
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
    // Los perdigones de un mismo disparo de escopeta suman en un solo numero.
    DamageNumber& last = m_damageNumbers[(m_nextDamageNumber + kMaxDamageNumbers - 1) % kMaxDamageNumbers];
    if (last.age == 0.0f && last.structure == structure && math3d::length(last.position - position) < 1.6f) {
        last.amount += amount;
        return;
    }
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

WallPlacement Match::planWall(float yaw, PieceType type) const {
    world::Aabb bots[kMaxBots];
    int n = 0;
    for (int id = 1; id < entityCount(); ++id) {
        if (alive(id)) bots[n++] = bodyBounds(id);
    }
    WallPlacement p = m_structures.plan(body(0).state().position, yaw, bodyBounds(0), bots, n, 0, type);
    // Con el material elegido o, si no queda bastante, con el primero que si haya.
    const int cost = m_config.build.wallCost;
    p.material = m_buildMaterial;
    if (m_materials.get(p.material) < cost) {
        for (int r = 0; r < static_cast<int>(world::Resource::Count); ++r) {
            if (m_materials.get(static_cast<world::Resource>(r)) < cost) continue;
            p.material = static_cast<world::Resource>(r);
            break;
        }
    }
    if (p.valid && m_materials.get(p.material) < cost) {
        p.valid = false;
        p.error = PlacementError::NoMaterials;
    }
    return p;
}

int Match::buildWall(float yaw, PieceType type) {
    const int wall = m_structures.place(planWall(yaw, type));
    if (wall >= 0) {
        m_materials.spend(m_structures.wall(wall).material, m_config.build.wallCost);
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

void Match::applyDamage(int victim, float amount, int attacker, bool ignoreShield) {
    if (!alive(victim) || amount <= 0.0f) return;
    if (victim == 0 && m_config.invulnerablePlayer) return;
    CharacterState& s = mutableBody(victim).state();
    // El escudo absorbe primero; lo que sobra pasa a la vida.
    const float absorbed = ignoreShield ? 0.0f : (s.shield < amount ? s.shield : amount);
    s.shield -= absorbed;
    s.health -= amount - absorbed;
    if (victim == 0) {
        m_sincePlayerDamaged = 0.0f;
        pushEvent(GameEventType::PlayerHurt, 0, attacker, chestOf(0));
    } else if (attacker >= 0 && attacker != victim && alive(attacker)) {
        // Un bot que recibe un disparo se cubre con una pared (a veces, en su siguiente
        // paso: no a mitad del disparo) y se gira hacia quien le ha dado.
        Bot& b = m_bots[victim - 1];
        b.coverFrom = attacker;
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
        if (m_config.loot) dropBotWeapon(victim - 1);  // su arma queda en el suelo
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

void Match::updateStorm(float dt) {
    // Tormenta del limite del mapa: al entrar, un golpe; despues, uno cada stormTickSeconds.
    if (m_phase == MatchPhase::Finished) return;
    for (int id = 0; id < entityCount(); ++id) {
        if (!alive(id) || m_air[id] == AirState::Bus) {
            m_inStorm[id] = false;
            continue;
        }
        const Vec3& p = body(id).state().position;
        // La del borde quita stormDamage; la que se cierra, segun la fase.
        const float edge = world::stormDepth(p.x, p.z);
        const float damage = edge > 0.0f ? m_config.stormDamage : m_storm.damage();
        if (edge <= 0.0f && m_storm.depth(p.x, p.z) <= 0.0f) {
            m_inStorm[id] = false;
            continue;
        }
        if (!m_inStorm[id]) {
            m_inStorm[id] = true;
            m_stormTimer[id] = 0.0f;
            pushEvent(GameEventType::StormEnter, id, -1, p);
            applyDamage(id, damage, kStormKiller, true);
            continue;
        }
        m_stormTimer[id] += dt;
        // Margen de 1 ms: 60 pasos de 1/60 s suman 0.99999 s en float.
        if (m_stormTimer[id] >= m_config.stormTickSeconds - 0.001f) {
            m_stormTimer[id] -= m_config.stormTickSeconds;
            applyDamage(id, damage, kStormKiller, true);
        }
    }
}

void Match::separateCharacters() {
    const int n = entityCount();
    for (int a = 0; a < n; ++a) {
        if (!alive(a) || m_air[a] != AirState::Ground) continue;
        for (int c = a + 1; c < n; ++c) {
            if (!alive(c) || m_air[c] != AirState::Ground) continue;
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
