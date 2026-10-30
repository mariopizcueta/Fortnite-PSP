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
        case BotState::Explore: return i18n::tr(i18n::T::BotExplore);
        case BotState::Travel: return i18n::tr(i18n::T::BotTravel);
        case BotState::Search: return i18n::tr(i18n::T::BotSearch);
        case BotState::Attack: return i18n::tr(i18n::T::BotAttack);
        case BotState::Loot: return i18n::tr(i18n::T::BotLoot);
        case BotState::Harvest: return i18n::tr(i18n::T::BotHarvest);
        case BotState::Flee: return i18n::tr(i18n::T::BotFlee);
        case BotState::Camp: return i18n::tr(i18n::T::BotCamp);
        case BotState::Idle: return i18n::tr(i18n::T::BotIdle);
        case BotState::Downed: return i18n::tr(i18n::T::BotDowned);
        case BotState::Revive: return i18n::tr(i18n::T::BotRevive);
        case BotState::Eliminated: return i18n::tr(i18n::T::BotEliminated);
    }
    return "?";
}

const char* useErrorName(UseError error) {
    switch (error) {
        case UseError::None: return "";
        case UseError::FullHealth: return i18n::tr(i18n::T::UseFullHealth);
        case UseError::FullShield: return i18n::tr(i18n::T::UseFullShield);
        case UseError::FullFood: return i18n::tr(i18n::T::UseFullFood);
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
    if (m_config.teamSize < 1 || m_config.mode == GameMode::Zombies) m_config.teamSize = 1;
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
    m_pendingSelect = -1;
    m_pendingSwapPickup = m_pendingSwapSlot = -1;
    m_recoilBloom = 0.0f;
    m_buildMode = false;
    m_buildMaterial = world::Resource::Wood;
    m_playerCrouched = false;
    m_playerHeadshots = 0;
    m_inventory.clear();
    for (const MatchConfig::StartItem& item : m_config.startItems) {
        if (item.type != ItemType::None) m_inventory.add(item.type, item.rarity, item.count);
    }
    for (int a = 0; a < static_cast<int>(AmmoType::Count); ++a) {
        m_inventory.addAmmo(static_cast<AmmoType>(a), m_config.startAmmo[a]);
    }
    m_inventory.select(0);
    // Sin nada en las manos (como al saltar del autobus en Fortnite): el pico. Tampoco se
    // empieza con el saco de dormir en la mano (modo zombi, v0.15).
    if (m_inventory.slot(0).type == ItemType::None || itemDef(m_inventory.slot(0).type).kind == ItemKind::Furniture) {
        m_inventory.setPickaxe(true);
    }
    m_materials = Materials{};
    for (int r = 0; r < static_cast<int>(world::Resource::Count); ++r) {
        m_materials.add(static_cast<world::Resource>(r), m_config.startMaterials[r]);
    }
    m_useSlot = -1;
    m_useError = UseError::None;
    m_sinceUseError = 99.0f;
    m_swingCooldown = 0.0f;
    // Objetos del mapa: todo en pie (lo que rompio la partida anterior, otra vez), salvo los
    // del modo zombi fuera de el y, en el modo zombi, las ruinas (v0.14).
    applyMapState();
    m_playerAiming = false;
    m_playerKills = 0;
    for (DamageNumber& d : m_damageNumbers) d.age = 99.0f;
    m_nextDamageNumber = 0;
    m_eventCount = 0;
    m_assistTarget = -1;
    snprintf(m_names[0], sizeof(m_names[0]), "%s", i18n::tr(i18n::T::PlayerDefaultName));

    m_bots.assign(m_config.botCount, Bot{});
    for (int i = 0; i < m_config.botCount; ++i) snprintf(m_names[i + 1], sizeof(m_names[0]), i18n::tr(i18n::T::BotName), i + 1);
    m_alive = 1 + m_config.botCount;
    splitIntoTeams();
    // Modo zombi: zombis (aparecen con el tiempo) y supervivientes en sus campamentos.
    if (zombieMode()) startZombies();
    else spawnBots(playerSpawn);

    m_phase = MatchPhase::Playing;
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
    for (Projectile& p : m_projectiles) p = Projectile{};
    for (Explosion& e : m_explosions) e.age = 99.0f;
    m_nextExplosion = 0;
    m_throwCooldown = 0.0f;
    m_llama = -1;
    m_damageSource = -1;
    for (Trap& t : m_traps) t = Trap{};
    m_trapStock = 0;
    m_pendingTrap = false;
    // Botin repartido por el mapa y, si se empieza en el autobus, todos dentro. La llama se
    // tira una sola vez aqui (5 %): reiniciar la partida es otra tirada con otra semilla.
    spawnLoot();
    if (m_config.loot && (m_config.forceLlama || m_rng.chance(m_config.llamaChance))) spawnLlama();
    if (m_config.startInBus) startBus();
    if (teamsLeft() <= 1 && !zombieMode()) {
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
void Match::setPlayerName(const char* name) {
    if (name && name[0]) snprintf(m_names[0], sizeof(m_names[0]), "%s", name);
}

const char* Match::name(int id) const { return (id >= 0 && id < entityCount()) ? m_names[id] : "?"; }

Vec3 Match::eyeOf(int id) const { return body(id).state().position + Vec3{0.0f, kEyeHeight, 0.0f}; }
Vec3 Match::chestOf(int id) const {
    const float height = m_downed[id] ? kDownedChestHeight : (id == 0 && m_playerCrouched ? kCrouchChestHeight : kChestHeight);
    return body(id).state().position + Vec3{0.0f, height, 0.0f};
}

world::Aabb Match::hitBox(int id) const {
    const Vec3 p = body(id).state().position;
    // Los brutos del modo zombi son mas grandes (v0.14).
    const float scale = entityScale(id);
    const float h = (m_downed[id] ? kDownedHitHeight : (id == 0 && m_playerCrouched ? kCrouchHitHeight : kHitHeight)) * scale;
    const float w = kHitHalfWidth * scale;
    return {{p.x - w, p.y, p.z - w}, {p.x + w, p.y + h, p.z + w}};
}

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
        if (!alive(id) || m_air[id] == AirState::Bus || !hostile(0, id)) continue;
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
    if (commands.placeTrap) m_pendingTrap = true;
    m_buildPiece = commands.buildPiece;
    // Curandose no se puede agachar ni levantar (la animacion de uso no lo permite).
    if (commands.toggleCrouch && m_air[0] == AirState::Ground && alive(0) && m_useSlot < 0 && !m_config.cheats.fly) {
        m_playerCrouched = !m_playerCrouched;
    }
    if (commands.swapPickup >= 0 && commands.swapSlot >= 0) {
        m_pendingSwapPickup = commands.swapPickup;
        m_pendingSwapSlot = commands.swapSlot;
    }
    if (commands.cycleMaterial) {
        m_buildMaterial = static_cast<world::Resource>((static_cast<int>(m_buildMaterial) + 1) %
                                                       static_cast<int>(world::Resource::Count));
    }
    if (commands.removeWall) m_pendingRemove = true;
    if (commands.reload) m_pendingReload = true;
    if (commands.togglePickaxe) m_pendingPickaxe = !m_pendingPickaxe;
    if (commands.useItem) m_pendingUse = true;
    m_pendingCycle += commands.cycleSlot;
    if (commands.selectSlot >= 0) m_pendingSelect = commands.selectSlot;
}

void Match::step(float dt) {
    m_time += dt;
    ++m_stats.steps;
    m_pathBudget = 1;  // como mucho un A* por paso de fisica (60 por segundo)
    m_sincePlayerHit += dt;
    m_sincePlayerDamaged += dt;
    for (Tracer& t : m_tracers) t.age += dt;
    for (DamageNumber& d : m_damageNumbers) d.age += dt;
    for (Explosion& e : m_explosions) e.age += dt;
    if (m_throwCooldown > 0.0f) m_throwCooldown -= dt;
    m_structures.update(dt);
    updateAirdrops(dt);

    updatePlayer(dt);
    autoPickupAmmo(0);
    if (zombieMode()) updateZombieMode(dt);
    for (int i = 0; i < static_cast<int>(m_bots.size()); ++i) updateBot(i, dt);
    if (teamMode()) updateDowned(dt);
    updateProjectiles(dt);
    updateTraps(dt);
    separateCharacters();
    if (m_storm.update(m_time, m_stormRng)) pushEvent(GameEventType::StormClosing, -1, m_storm.phase(), m_storm.nextCenter());
    updateStorm(dt);
    if (m_structures.revision() != m_seenStructureRevision) {
        m_seenStructureRevision = m_structures.revision();
        onStructuresChanged();
    }
}

void Match::updatePlayer(float dt) {
    m_playerReviving = -1;
    const bool place = m_pendingPlace, remove = m_pendingRemove, reload = m_pendingReload;
    const bool trap = m_pendingTrap;
    m_pendingTrap = false;
    const bool togglePickaxe = m_pendingPickaxe;
    const int cycle = m_pendingCycle;
    const int select = m_pendingSelect;
    const int swapIndex = m_pendingSwapPickup, swapTo = m_pendingSwapSlot;
    m_pendingPlace = m_pendingRemove = m_pendingReload = m_pendingPickaxe = false;
    m_pendingCycle = 0;
    m_pendingSelect = -1;
    m_pendingSwapPickup = m_pendingSwapSlot = -1;
    m_sinceUseError += dt;
    if (m_swingCooldown > 0.0f) m_swingCooldown -= dt;
    // El retroceso acumulado se recupera al dejar de disparar (0.25 s despues del ultimo
    // disparo, al ritmo del arma en la mano): disparando seguido se va acumulando.
    {
        const Weapon* held = m_inventory.activeWeapon();
        if (!held || held->sinceShot() > 0.25f) {
            const float recovery = held ? held->config().recoilRecovery : 8.0f;
            m_recoilBloom = fmaxf(0.0f, m_recoilBloom - recovery * dt);
        }
    }
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
    const bool control = (m_phase == MatchPhase::Playing || m_phase == MatchPhase::Finished) && !m_sleeping;
    if (!control) in = PlayerInput{};

    // En el autobus o cayendo: X salta / abre el ala delta y el stick dirige la caida.
    if (m_air[0] != AirState::Ground) {
        updateAirState(0, control && in.jumpPressed);
        m_buildMode = false;
        m_playerAiming = false;
        m_assistTarget = -1;
        m_pendingUse = false;
        cancelUse();
        m_playerCrouched = false;
        if (m_air[0] == AirState::Bus) return;
        in.jumpPressed = false;
        const u64 t0 = core::nowMicros();
        m_player.step(airIntent(0, playerIntent(in).wish, false), dt, *m_world);
        m_stats.physicsMicros += core::nowMicros() - t0;
        updateAirState(0, false);  // aterrizaje
        return;
    }

    // Derribado (equipos, v0.16): solo se arrastra. No dispara, no construye y no usa nada.
    if (m_downed[0]) {
        m_buildMode = false;
        m_playerAiming = false;
        m_assistTarget = -1;
        m_pendingUse = false;
        in.jumpPressed = in.jumpHeld = in.aiming = false;
        MoveIntent crawl = playerIntent(in);
        crawl.speedScale *= m_config.downCrawlSpeed;
        crawl.jumpPressed = crawl.jumpHeld = false;
        const u64 t0 = core::nowMicros();
        m_player.step(crawl, dt, *m_world);
        m_stats.physicsMicros += core::nowMicros() - t0;
        return;
    }
    // Reanimando a un companero (CUADRADO mantenido): quieto y sin hacer nada mas.
    if (control && m_commands.reviveHeld) {
        const int mate = reviveTarget();
        if (mate >= 0 && markRevive(0, mate)) {
            m_playerReviving = mate;
            cancelUse();
            if (Weapon* w = m_inventory.activeWeapon()) w->cancelReload();
            m_playerAiming = false;
            m_assistTarget = -1;
            m_pendingUse = false;
            m_buildMode = false;
            const u64 t0 = core::nowMicros();
            m_player.step(MoveIntent{}, dt, *m_world);
            m_stats.physicsMicros += core::nowMicros() - t0;
            return;
        }
    }
    // Cambiar lo que se lleva en la mano interrumpe la recarga y el consumible en uso.
    if (control && (cycle != 0 || togglePickaxe || select >= 0)) {
        Weapon* before = m_inventory.activeWeapon();
        const int slotBefore = m_inventory.selected();
        const bool pickaxeBefore = m_inventory.pickaxe();
        if (togglePickaxe) m_inventory.setPickaxe(!m_inventory.pickaxe());
        if (cycle != 0) m_inventory.cycle(cycle);
        // Una ranura vacia no se elige (como al recorrerlas con la cruceta).
        if (select >= 0 && select < m_inventory.slotCount() && m_inventory.slot(select).type != ItemType::None) {
            m_inventory.select(select);
        }
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

    // Usando un consumible: quieto y sin saltar (la camara puede girar). Recibir dano no lo
    // interrumpe salvo con MatchConfig::interruptUseOnDamage.
    if (m_useSlot >= 0) {
        in.moveX = in.moveY = 0.0f;
        in.jumpPressed = false;
        in.jumpHeld = false;
    }
    // Saltar levanta; agachado se anda mas despacio.
    if (in.jumpPressed) m_playerCrouched = false;
    MoveIntent intent = playerIntent(in);
    if (m_playerCrouched) intent.speedScale *= m_config.crouchSpeed;
    if (m_stumbleTimer > 0.0f) intent.speedScale *= 0.45f;  // modo zombi: tropieza de cansancio
    // Trucos de movimiento (menu de administrador).
    const Cheats& cheats = m_config.cheats;
    if (cheats.superSpeed) {
        intent.speedScale *= 2.0f;
        intent.maxSpeedScale = 2.0f;
    }
    if (cheats.superJump) intent.jumpScale = 1.75f;  // la altura crece con el cuadrado: x3
    if (cheats.lowGravity) intent.gravityScale = 0.35f;
    if (cheats.fly) {
        // Volar: saltar (mantenido) sube y agacharse (mantenido) baja.
        m_playerCrouched = false;
        intent.fly = true;
        intent.flyVertical = (in.jumpHeld ? 1.0f : 0.0f) - (in.crouchHeld ? 1.0f : 0.0f);
        intent.jumpPressed = false;
    }
    const u64 t0 = core::nowMicros();
    m_player.step(intent, dt, *m_world);
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
        if (trap) placeTrap(m_aim);
        if (remove) removeWallAt(m_aim);
        return;
    }
    if (swapIndex >= 0 && m_useSlot < 0) swapPickup(swapIndex, swapTo);
    updatePlayerItems(dt, reload);
}

void Match::updatePlayerItems(float dt, bool reload) {
    const bool use = m_pendingUse;
    m_pendingUse = false;
    // CUADRADO: abrir / recoger lo que haya al alcance; si no hay nada, recargar. Mientras
    // se usa un consumible no se recoge nada.
    if (reload && m_useSlot >= 0) reload = false;
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
        if (m_config.cheats.noReload) weapon->finishReload();
        // Ayuda de punteria: bot visible mas centrado en un cono estrecho alrededor de la mira.
        const float range = weapon->config().range;
        const bool sniperScope = hasScope(m_inventory.activeType()) && m_input.aiming;
        const float assist = sniperScope ? m_config.sniperAssistDegrees : m_config.aimAssistDegrees;
        m_assistTarget = assist > 0.0f ? findLockTarget(m_aim.eye, m_aim.direction, assist, range) : -1;
        // tryFire() aplica municion y cadencia: mantener R da como mucho un disparo por
        // intervalo (y por paso), nunca uno por frame. Una rafaga empezada sigue sola.
        if ((m_aim.fire || weapon->burstPending()) && m_phase != MatchPhase::Finished) {
            const float spread = playerSpread(weapon->config());  // antes de tryFire (primer disparo)
            if (weapon->tryFire()) {
                const WeaponConfig& w = weapon->config();
                if (!m_config.cheats.noSpread) m_recoilBloom = fminf(m_recoilBloom + w.recoil, fmaxf(w.maxRecoil, m_recoilBloom));
                if (m_config.cheats.infiniteAmmo) weapon->fillMagazine();
                if (m_config.cheats.rapidFire) weapon->scaleCooldown(0.33f);
                const int type = static_cast<int>(m_inventory.activeType());
                if (w.projectile != ProjectileKind::None) {
                    playerLaunch(w, spread, type);
                } else {
                    for (int pellet = 0; pellet < w.pellets; ++pellet) {
                        playerShot(m_aim, w.damage / static_cast<float>(w.pellets), range, spread, &w, pellet == 0, type);
                    }
                }
            }
        }
        if (!wasReloading && weapon->reloading()) {
            pushEvent(GameEventType::Reload, 0, static_cast<int>(m_inventory.activeType()), chestOf(0));
        }
        m_inventory.setAmmo(ammo, weapon->reserve());
        return;
    }
    // Granada: R la lanza hacia la mira (una cada 0.8 s).
    m_assistTarget = -1;
    if (itemDef(m_inventory.activeType()).kind == ItemKind::Throwable) {
        if (use) throwGrenade();
        return;
    }
    // Cama y saco de dormir (v0.15): R coloca la cama delante o duerme en el saco.
    if (itemDef(m_inventory.activeType()).kind == ItemKind::Furniture) {
        if (use && m_inventory.activeType() == ItemType::Bed) placeBed();
        else if (use) trySleep(-1);
        return;
    }
    // Consumible: R lo empieza a usar; termina tras su tiempo de uso si no se cambia de
    // ranura ni se construye (moverse y recibir dano no lo interrumpen, como en Fortnite).
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
    if (def.food > 0.0f) {
        m_food = fminf(100.0f, m_food + def.food);
        m_hungerStage = m_food <= 0.0f ? 2 : (m_food < 25.0f ? 1 : 0);
        objectiveEvent(ObjectiveType::EatFood);
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
    if (def.food > 0.0f) {
        // Comida: solo importa el hambre (la racion cura un poco de paso).
        if (m_food >= 99.0f) error = UseError::FullFood;
    } else {
        if (def.health > 0.0f && s.health >= def.healthCap) error = UseError::FullHealth;
        if (def.shield > 0.0f && s.shield >= def.shieldCap) error = UseError::FullShield;
    }
    if (error != UseError::None) {
        m_useError = error;
        m_sinceUseError = 0.0f;
        return false;
    }
    m_useSlot = slot;
    m_useTimer = 0.0f;
    m_useDuration = useSecondsFor(m_inventory.slot(slot).type);
    pushEvent(GameEventType::UseStarted, 0, static_cast<int>(m_inventory.slot(slot).type), chestOf(0));
    return true;
}

void Match::cancelUse() {
    if (m_useSlot < 0) return;
    pushEvent(GameEventType::UseCanceled, 0, static_cast<int>(m_inventory.slot(m_useSlot).type), chestOf(0));
    m_useSlot = -1;
}

bool Match::moveSlot(int from, int to) {
    if (from < 0 || to < 0 || from >= m_inventory.slotCount() || to >= m_inventory.slotCount() || from == to || !alive(0)) return false;
    if (m_inventory.slot(from).type == ItemType::None && m_inventory.slot(to).type == ItemType::None) return false;
    cancelUse();
    if (Weapon* w = m_inventory.activeWeapon()) w->cancelReload();
    m_inventory.swapSlots(from, to);
    // Lo que se llevaba en la mano sigue en la mano.
    if (m_inventory.selected() == from) m_inventory.select(to);
    else if (m_inventory.selected() == to) m_inventory.select(from);
    return true;
}

bool Match::selectSlot(int slot) {
    if (slot < 0 || slot >= m_inventory.slotCount() || !alive(0)) return false;
    if (slot == m_inventory.selected() && !m_inventory.pickaxe()) return true;
    cancelUse();
    if (Weapon* w = m_inventory.activeWeapon()) w->cancelReload();
    m_inventory.setPickaxe(false);
    m_inventory.select(slot);
    return true;
}

bool Match::dropInventorySlot(int slot) {
    if (slot < 0 || slot >= m_inventory.slotCount() || !alive(0) || m_air[0] != AirState::Ground) return false;
    if (m_inventory.slot(slot).type == ItemType::None) return false;
    if (m_useSlot == slot) cancelUse();
    if (Weapon* w = m_inventory.activeWeapon()) w->cancelReload();
    const InventorySlot dropped = m_inventory.take(slot);
    // Delante del jugador, como al soltarlo en Fortnite (se puede volver a recoger).
    dropSlot(dropped, body(0).state().position + forwardFromYaw(m_input.cameraYaw) * 1.0f);
    return true;
}

void Match::cheatHeal() {
    if (!alive(0)) return;
    CharacterState& s = m_player.state();
    s.health = s.maxHealth;
    s.shield = s.maxShield;
}

void Match::cheatEliminateBots() {
    for (int id = 1; id < entityCount(); ++id) {
        if (!alive(id)) continue;
        m_damageSource = -1;
        applyDamage(id, 100000.0f, 0, true);
    }
}

void Match::cheatRefill() {
    for (int a = 0; a < static_cast<int>(AmmoType::Count); ++a) m_inventory.setAmmo(static_cast<AmmoType>(a), Inventory::kMaxAmmo);
    for (int i = 0; i < m_inventory.slotCount(); ++i) {
        InventorySlot& slot = m_inventory.mutableSlot(i);
        if (isWeapon(slot.type)) slot.weapon.fillMagazine();
    }
    for (int r = 0; r < static_cast<int>(world::Resource::Count); ++r) {
        m_materials.add(static_cast<world::Resource>(r), Materials::kMax);
    }
    m_trapStock = m_config.maxTrapsCarried;
}

float Match::useSecondsFor(ItemType type) const {
    const int i = static_cast<int>(type);
    const float configured = i >= 0 && i < static_cast<int>(ItemType::Count) ? m_config.useSeconds[i] : 0.0f;
    const float seconds = configured > 0.0f ? configured : itemDef(type).useSeconds;
    return seconds > 0.05f ? seconds : 0.05f;
}

float Match::playerSpread(const WeaponConfig& w) const {
    // Dispersion (grados): apuntando con L, la de apuntar; si no, la de cadera. Moverse y
    // saltar la aumentan. Agachado en el suelo se reduce segun el arma (crouchSpreadScale).
    // El retroceso de los ultimos disparos se suma despues: agacharse no lo quita.
    // Quieto, apuntando y sin retroceso pendiente, el primer disparo va exacto (como en
    // Fortnite). La escopeta siempre abre su cono.
    if (!m_config.weaponSpread || m_config.cheats.noSpread) return 0.0f;
    const CharacterState& s = m_player.state();
    const bool aiming = m_input.aiming;
    float spread = aiming ? w.aimSpread : w.hipSpread;
    if (s.horizontalSpeed() > 1.0f) spread += 1.0f;
    if (!s.grounded()) spread += 3.0f;
    if (m_playerCrouched && s.grounded()) {
        const float bonus = math3d::clampf(m_config.crouchAccuracyBonus, 0.0f, 1.0f);
        spread *= 1.0f + (w.crouchSpreadScale - 1.0f) * bonus;
    }
    const Weapon* weapon = m_inventory.activeWeapon();
    // Modo zombi (v0.15): con sueno, el pulso tiembla (y mas cuanto mas cansado).
    const float tiredSpread = tired() ? 5.0f * (1.0f - m_rest / fmaxf(m_config.zombie.tiredLevel, 1.0f)) + 1.0f : 0.0f;
    if (w.pellets == 1 && aiming && s.horizontalSpeed() < 0.5f && s.grounded() && weapon && weapon->sinceShot() > 0.35f &&
        m_recoilBloom < 0.05f && tiredSpread <= 0.0f) {
        return 0.0f;
    }
    return spread + m_recoilBloom + tiredSpread;
}

float Match::playerSpreadNow() const {
    const Weapon* w = m_inventory.activeWeapon();
    return w ? playerSpread(w->config()) : 0.0f;
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
        if (!alive(id) || !hostile(0, id)) continue;
        float t;
        if (world::segmentVsAabb(chest, end - chest, bodyBounds(id), &t) && t < bestT) {
            bestT = t;
            victim = id;
        }
    }
    bool llamaHit = false;
    if (llamaStanding()) {
        float t;
        if (world::segmentVsAabb(chest, end - chest, llamaBox(), &t) && t < bestT) {
            bestT = t;
            victim = -1;
            llamaHit = true;
        }
    }
    world::RayHit hit;
    const bool worldHit = m_world->raycast(chest, end, world::kBlocksShots, &hit) && hit.t < bestT;
    if (worldHit) llamaHit = false;
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

    const bool any = victim >= 0 || worldHit || resource >= 0 || llamaHit;
    pushEvent(GameEventType::PickaxeSwing, 0, any ? 1 : 0, point);
    m_damageSource = -1;  // el pico
    if (llamaHit) {
        m_sincePlayerHit = 0.0f;
        addDamageNumber(point, m_config.pickaxeStructureDamage, true);
        damageContainer(m_llama, m_config.pickaxeStructureDamage, 0, point);
    } else if (victim >= 0) {
        m_sincePlayerHit = 0.0f;
        const float damage = m_config.pickaxeDamage * (m_config.cheats.oneShot ? 10.0f : 1.0f);
        addDamageNumber(chestOf(victim), damage, false, false, body(victim).state().shield > 0.0f);
        pushEvent(GameEventType::HitEntity, victim, 0, chestOf(victim));
        applyDamage(victim, damage, 0);
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
    if (added > 0) objectiveEvent(ObjectiveType::Harvest, added);
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
    // Ayuda de punteria: solo si la mira no le da ya (asi se puede apuntar a la cabeza).
    float tAssist;
    if (m_assistTarget >= 0 && !world::segmentVsAabb(aim.eye, aim.direction * range, hitBox(m_assistTarget), &tAssist)) {
        dir = math3d::normalize(chestOf(m_assistTarget) - aim.eye);
    }
    // Truco "aimbot": a la cabeza del rival visible mas centrado, sin dispersion.
    if (m_config.cheats.aimbot) {
        const int target = aimbotTarget(aim.eye, aim.direction, range);
        if (target >= 0) {
            const world::Aabb box = hitBox(target);
            const Vec3 head{(box.min.x + box.max.x) * 0.5f, box.max.y - 0.12f, (box.min.z + box.max.z) * 0.5f};
            dir = math3d::normalize(head - aim.eye);
            spreadDegrees = 0.0f;
        }
    }
    // Dispersion del arma (despues de la ayuda de punteria: la ayuda no la anula).
    if (spreadDegrees > 0.0f) dir = perturb(dir, math3d::toRadians(spreadDegrees), m_rng);

    // 1) Rayo desde la camara por el centro de la mira (ver aimPoint).
    const Vec3 target = aimPoint(aim.eye, dir, range);

    // 2) La bala sale del arma: si entre el pecho del jugador y ese punto hay un solido
    //    (p. ej. un muro que la camara ve por encima), el disparo termina en el solido.
    Vec3 toTarget = target - chest;
    const float len = math3d::length(toTarget);
    if (len < 0.05f) return resolveShot(0, chest, chest + dir * range, damage, weapon, emitShot, weaponType);
    toTarget = toTarget * (1.0f / len);
    return resolveShot(0, chest, target + toTarget * 0.1f, damage, weapon, emitShot, weaponType);
}

int Match::aimbotTarget(const Vec3& eye, const Vec3& direction, float range) const {
    int best = -1;
    float bestCos = cosf(math3d::toRadians(35.0f));
    for (int id = 1; id < entityCount(); ++id) {
        if (!alive(id) || m_air[id] == AirState::Bus || !hostile(0, id)) continue;
        const world::Aabb box = hitBox(id);
        const Vec3 head{(box.min.x + box.max.x) * 0.5f, box.max.y - 0.12f, (box.min.z + box.max.z) * 0.5f};
        const Vec3 v = head - eye;
        const float d = math3d::length(v);
        if (d < 0.5f || d > range) continue;
        const float c = math3d::dot(v * (1.0f / d), direction);
        if (c <= bestCos) continue;
        if (!m_config.cheats.wallbang && !lineOfSight(eye, head) && !lineOfSight(chestOf(0), head)) continue;
        bestCos = c;
        best = id;
    }
    return best;
}

int Match::objectFromCollider(int collider) const {
    if (collider < 0 || collider >= static_cast<int>(m_harvestByCollider.size())) return -1;
    const int index = m_harvestByCollider[collider];
    return index >= 0 && !m_objectDestroyed[index] ? index : -1;
}

Vec3 Match::aimPoint(const Vec3& eye, const Vec3& dir, float range) const {
    // Empieza a la altura del jugador: lo que haya entre la camara y el jugador no cuenta.
    const float t0 = fmaxf(0.0f, math3d::dot(chestOf(0) - eye, dir));
    const Vec3 start = eye + dir * t0;
    const Vec3 end = start + dir * range;
    world::RayHit wallHit;
    float best = !m_config.cheats.wallbang && m_world->raycast(start, end, world::kBlocksShots, &wallHit) ? wallHit.t : 1.0f;
    const Vec3 seg = end - start;
    for (int id = 1; id < entityCount(); ++id) {
        if (!alive(id) || m_air[id] == AirState::Bus || !hostile(0, id)) continue;
        float t;
        if (world::segmentVsAabb(start, seg, hitBox(id), &t) && t < best) best = t;
    }
    return start + seg * best;
}

void Match::fireWeapon(int shooter, const Vec3& origin, const Vec3& direction, const WeaponConfig& weapon,
                       float spreadDegrees, float damageScale, int weaponType) {
    if (weapon.projectile != ProjectileKind::None) {
        const Vec3 dir = perturb(direction, math3d::toRadians(spreadDegrees), m_rng);
        spawnProjectile(shooter, origin, dir, weapon, damageScale, weaponType);
        ++m_stats.shotsFired;
        pushEvent(GameEventType::Shot, shooter, weaponType, origin);
        hearShot(shooter, origin);
        return;
    }
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
    // Truco "atravesar paredes": las balas del jugador no se paran en nada del mundo.
    const bool wallbang = shooter == 0 && m_config.cheats.wallbang;
    const bool wall = !wallbang && m_world->raycast(origin, end, world::kBlocksShots, &wallHit);
    if (shooter == 0 && m_config.cheats.oneShot) damage *= 10.0f;
    float best = wall ? wallHit.t : 1.0f;
    int victim = -1;
    for (int id = 0; id < entityCount(); ++id) {
        if (id == shooter || !alive(id) || m_air[id] == AirState::Bus) continue;
        if (shooter >= 0 && !hostile(shooter, id)) continue;  // los aliados no se dan (modo zombi)
        const world::Aabb box = hitBox(id);
        float t;
        if (world::segmentVsAabb(origin, seg, box, &t) && t < best) {
            best = t;
            victim = id;
        }
    }
    // La llama (si esta en pie) tambien para las balas.
    bool llamaHit = false;
    if (llamaStanding()) {
        float t;
        if (world::segmentVsAabb(origin, seg, llamaBox(), &t) && t < best) {
            best = t;
            victim = -1;
            llamaHit = true;
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
    if (llamaHit) {
        result.blockedByWorld = true;
        if (shooter == 0) {
            m_sincePlayerHit = 0.0f;
            addDamageNumber(result.end, damage, true);
        }
        damageContainer(m_llama, damage, shooter, result.end);
    } else if (victim >= 0) {
        m_damageSource = weaponType;
        // En la cabeza (lo mas alto de la caja de impacto): dano doble.
        const world::Aabb box = hitBox(victim);
        bool headshot = result.end.y >= box.max.y - kHeadSize;
        if (headshot && victim == 0 && shooter > 0) headshot = m_rng.chance(m_config.botHeadshotChance);
        if (headshot) damage *= m_config.headshotMultiplier;
        const bool shielded = body(victim).state().shield > 0.0f;
        pushEvent(GameEventType::HitEntity, victim, shooter, result.end);
        m_events[(m_eventCount - 1) % kMaxEvents].amount = headshot ? 1 : 0;
        ++m_stats.shotsHitEntity;
        result.hitEntity = true;
        result.headshot = headshot;
        result.victim = victim;
        if (shooter == 0) {
            m_sincePlayerHit = 0.0f;
            if (headshot) ++m_playerHeadshots;
            if (headshot && isZombie(victim)) objectiveEvent(ObjectiveType::Headshots);
            addDamageNumber(result.end, damage, false, headshot, shielded);
        }
        applyDamage(victim, damage, shooter);
        m_damageSource = -1;
    } else if (wall) {
        ++m_stats.shotsBlockedByWorld;
        result.blockedByWorld = true;
        // Las paredes construidas pierden vida con cada impacto, y los objetos del mapa
        // (paneles de las naves, contenedores, cajas, arboles...) tambien (v0.13).
        const int built = m_structures.wallFromCollider(wallHit.dynamicHandle);
        const int object = built < 0 ? objectFromCollider(wallHit.collider) : -1;
        if (built >= 0) {
            damageWall(built, damage, shooter, result.end);
        } else if (object >= 0 && m_config.shotObjectDamageScale > 0.0f) {
            const float amount = damage * m_config.shotObjectDamageScale;
            ++m_stats.objectsShot;
            pushEvent(GameEventType::HitStructure, -1, shooter, result.end);
            if (shooter == 0) addDamageNumber(result.end, amount, true);
            damageObject(object, amount, result.end, shooter);
        }
    }
    // Truco "balas explosivas": cada disparo del jugador (no cada perdigon) explota donde da.
    if (shooter == 0 && emitShot && m_config.cheats.explosiveAmmo && (!weapon || weapon->projectile == ProjectileKind::None)) {
        const Vec3 back = math3d::length(seg) > 0.01f ? math3d::normalize(seg) * -0.25f : Vec3{0.0f, 0.0f, 0.0f};
        explode(result.end + back, 0, 45.0f, 2.2f, 150.0f, weaponType);
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

void Match::addDamageNumber(const Vec3& position, float amount, bool structure, bool headshot, bool shield) {
    // Los perdigones de un mismo disparo de escopeta suman en un solo numero.
    DamageNumber& last = m_damageNumbers[(m_nextDamageNumber + kMaxDamageNumbers - 1) % kMaxDamageNumbers];
    if (last.age == 0.0f && last.structure == structure && math3d::length(last.position - position) < 1.6f) {
        last.amount += amount;
        last.headshot |= headshot;
        last.shield |= shield;
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
    d.headshot = headshot;
    d.shield = shield;
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
    if (p.valid && m_materials.get(p.material) < cost && !m_config.cheats.infiniteMaterials) {
        p.valid = false;
        p.error = PlacementError::NoMaterials;
    }
    return p;
}

int Match::buildWall(float yaw, PieceType type) {
    const int wall = m_structures.place(planWall(yaw, type));
    if (wall >= 0) {
        if (!m_config.cheats.infiniteMaterials) m_materials.spend(m_structures.wall(wall).material, m_config.build.wallCost);
        ++m_stats.wallsBuilt;
        objectiveEvent(ObjectiveType::BuildPieces);
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
    // Sin fuego amigo (modo zombi y equipos): explosiones y trampas no hieren a los aliados.
    if (attacker >= 0 && attacker != victim && !hostile(attacker, victim)) return;
    // Un aliado durmiendo en su cama no recibe dano de nada (v0.16).
    if (isSurvivor(victim) && m_bots[victim - 1].sleeping) return;
    if (victim == 0 && (m_config.invulnerablePlayer || m_config.cheats.godMode)) return;
    CharacterState& s = mutableBody(victim).state();
    // El escudo absorbe primero; lo que sobra pasa a la vida.
    const float absorbed = ignoreShield ? 0.0f : (s.shield < amount ? s.shield : amount);
    s.shield -= absorbed;
    s.health -= amount - absorbed;
    if (victim == 0) {
        m_sincePlayerDamaged = 0.0f;
        pushEvent(GameEventType::PlayerHurt, 0, attacker, chestOf(0));
        if (m_config.interruptUseOnDamage) cancelUse();
    } else if (attacker >= 0 && attacker != victim && alive(attacker) &&
               !(attacker == 0 && m_config.cheats.invisible)) {
        // Un bot que recibe un disparo se cubre con una pared (a veces, en su siguiente
        // paso: no a mitad del disparo) y se gira hacia quien le ha dado. Con poca vida no:
        // en su siguiente decision huye de el (v0.13).
        Bot& b = m_bots[victim - 1];
        b.coverFrom = attacker;
        b.sinceHurt = 0.0f;
        b.threat = attacker;
        if ((m_config.botsFlee || b.kind == BotKind::Survivor) && s.health > 0.0f && botWeak(b)) {
            if (b.decisionTimer > 0.05f) b.decisionTimer = 0.05f;
        } else if (b.state == BotState::Downed || b.state == BotState::Revive) {
            // Derribado (se arrastra) o levantando a un companero: sigue con ello.
        } else if (b.state != BotState::Attack || !b.targetVisible) {
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
        // Equipos: con un companero en pie no se cae del todo (si ya estaba derribado, si).
        if (teamMode() && !m_downed[victim] && teamStanding(victim)) goDown(victim, attacker);
        else eliminate(victim, attacker >= 0 || !m_downed[victim] ? attacker : m_downedBy[victim]);
    }
}

void Match::eliminate(int victim, int killer) {
    mutableBody(victim).state().health = 0.0f;
    m_downed[victim] = false;
    pushEvent(GameEventType::Eliminated, victim, killer, body(victim).state().position);
    m_events[(m_eventCount - 1) % kMaxEvents].amount = m_damageSource + 1;
    if (victim > 0) {
        Bot& b = m_bots[victim - 1];
        // Su inventario (y materiales) quedan en el suelo, una sola vez por eliminacion. Los
        // zombis sueltan a veces chatarra, municion o vendas.
        if (b.kind == BotKind::Zombie) {
            zombieDrops(victim - 1);
            if (killer == 0) {
                ++m_zombieKills;
                objectiveEvent(ObjectiveType::KillZombies);
                if (b.zombieType == ZombieType::Runner) objectiveEvent(ObjectiveType::KillRunners);
                if (b.zombieType == ZombieType::Brute) objectiveEvent(ObjectiveType::KillBrute);
            }
            b.dormant = false;
        } else if (b.kind == BotKind::Survivor && zombieMode() && b.rescued) {
            // Aliado caido (v0.15): no suelta nada; al rato se levanta como zombi armado.
            b.riseTimer = m_config.zombie.fallenRiseSeconds;
            b.sleeping = false;
            b.allyPiece = 0;
            if (b.bed >= 0 && b.bed < static_cast<int>(m_containers.size())) m_containers[b.bed].claimedBy = -1;
            b.bed = -1;
            pushEvent(GameEventType::AllyDown, victim, killer, b.body.state().position);
        } else if (m_config.loot && !b.lootDropped) {
            b.lootDropped = true;
            dropBotWeapon(victim - 1);
        }
        b.state = BotState::Eliminated;
        b.eliminatedAt = m_time;
        b.aiming = false;
        b.body.state().velocity = {0.0f, 0.0f, 0.0f};
    } else if (!teamMode()) {
        m_playerPlace = m_alive;  // puesto = vivos antes de caer
    }
    --m_alive;
    if (killer == 0) ++m_playerKills;
    else if (killer > 0 && killer <= static_cast<int>(m_bots.size())) ++m_bots[killer - 1].kills;

    for (int i = kMaxKillFeed - 1; i > 0; --i) m_killFeed[i] = m_killFeed[i - 1];
    m_killFeed[0] = {killer, victim, m_time};
    if (m_killFeedCount < kMaxKillFeed) ++m_killFeedCount;

    if (zombieMode()) {
        // Supervivencia: la partida acaba cuando cae el jugador (no hay ganador).
        if (victim == 0) {
            m_phase = MatchPhase::Finished;
            m_winner = -1;
            m_sleeping = 0;
        }
        return;
    }
    if (victim == 0 && m_phase == MatchPhase::Playing) m_phase = MatchPhase::Spectating;
    if (teamMode()) {
        // Sin nadie en pie en el equipo, los derribados caen con el (como en Fortnite). El
        // puesto es el del equipo: se decide al caer el ultimo.
        if (!teamStanding(victim)) {
            for (int id = 0; id < entityCount(); ++id) {
                if (m_downed[id] && sameTeam(id, victim) && alive(id)) eliminate(id, m_downedBy[id]);
            }
        }
        if (sameTeam(victim, 0) && !playerTeamAlive() && !alive(0) && m_playerPlace == 0) m_playerPlace = teamsLeft() + 1;
    }
    if (m_phase != MatchPhase::Finished && teamsLeft() <= 1) {
        m_phase = MatchPhase::Finished;
        m_winner = -1;
        for (int id = entityCount() - 1; id >= 0; --id) {
            if (alive(id)) m_winner = id;
        }
        if (m_winner >= 0 && sameTeam(m_winner, 0)) m_playerPlace = 1;
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
    m_damageSource = -1;
    for (int id = 0; id < entityCount(); ++id) {
        if (!alive(id) || m_air[id] == AirState::Bus) {
            m_inStorm[id] = false;
            continue;
        }
        if (id == 0 && m_config.cheats.noStorm) {
            m_inStorm[0] = false;
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
    // La llama es solida: quien la toca se aparta (no es una caja del mundo para no tocar
    // la navegacion ni las cajas dinamicas de las construcciones).
    if (llamaStanding()) {
        const world::Aabb box = llamaBox();
        const Vec3 center = (box.min + box.max) * 0.5f;
        for (int id = 0; id < n; ++id) {
            if (!alive(id) || m_air[id] != AirState::Ground) continue;
            CharacterState& st = mutableBody(id).state();
            if (st.position.y > box.max.y - 0.1f || st.position.y + kBodyHeight < box.min.y) continue;
            const float hx = (box.max.x - box.min.x) * 0.5f + kBodyRadius, hz = (box.max.z - box.min.z) * 0.5f + kBodyRadius;
            const float dx = st.position.x - center.x, dz = st.position.z - center.z;
            if (fabsf(dx) >= hx || fabsf(dz) >= hz) continue;
            // Sale por el lado mas cercano.
            if (hx - fabsf(dx) < hz - fabsf(dz)) st.position.x = center.x + (dx < 0.0f ? -hx : hx);
            else st.position.z = center.z + (dz < 0.0f ? -hz : hz);
            mutableBody(id).depenetrate(*m_world);
        }
    }
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
