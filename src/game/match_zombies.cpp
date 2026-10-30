// Modo Supervivencia zombi de game::Match (v0.14, docs/ZOMBIS.md): dia y noche, zombis
// (aparicion, IA y golpes), supervivientes (rescate y seguimiento), centros de crafteo y
// planos de la base. Los datos del modo estan en game/zombies.h.
// v0.15: hambre y sueno, camas, lugares de interes con guardianes (y zombis tumbados que se
// levantan), aliados que duermen, construyen y, si caen, se levantan como zombis armados,
// mochilas y objetivos. La partida guardada, en zombie_save.cpp.
#include "game/match.h"

#include <stdio.h>

#include "core/timer.h"
#include "game/match_internal.h"
#include "world/storm.h"
#include "world/water.h"
#include "world/zombie_sites.h"

namespace game {

using math3d::Vec3;
using namespace tuning;
using namespace detail;

namespace {

constexpr float kZombieDecision = 0.3f;   // s entre decisiones de cada zombi
constexpr float kCorpseReuse = 8.5f;      // s hasta reutilizar a un zombi caido (su cuerpo se ve 8 s)
constexpr float kDiscoverRange = 22.0f;   // m: supervivientes y centros de crafteo descubiertos
constexpr float kWorkbenchReach = 2.7f;
const char* const kSurvivorNames[kSurvivorCount] = {"Lola", "Paco", "Nerea", "Iker", "Marta"};

bool healItem(ItemType type) {
    const ItemDef& def = itemDef(type);
    return def.kind == ItemKind::Consumable && (def.health > 0.0f || def.shield > 0.0f);
}

}  // namespace

// --- Arranque ----------------------------------------------------------------------------------

void Match::startZombies() {
    const ZombieConfig& z = m_config.zombie;
    m_zClock = z.startClock;
    m_day = 1;
    m_night = false;
    m_nightsSurvived = 0;
    m_spawnTimer = 3.0f;  // un respiro al despertar
    m_zombieKills = 0;
    m_itemsCrafted = 0;
    m_food = z.startFood;
    m_rest = z.startRest;
    m_hungerStage = m_tiredStage = 0;
    m_needsTimer = 0.0f;
    m_sleeping = 0;
    m_sleepBed = -1;
    m_fumbleTimer = m_rng.range(20.0f, 35.0f);
    m_stumbleTimer = 0.0f;
    m_sleepRested = 0.0f;
    for (int i = 0; i < world::zsites::kSiteCount; ++i) {
        m_siteFound[i] = false;
        m_siteGuardDay[i] = 0;
    }
    m_objectivesDone = 0;
    m_lastObjective = ObjectiveType::Count;
    m_rescuedTotal = 0;
    m_allyBuildTimer = 2.0f;
    for (int i = 0; i < kActiveObjectives; ++i) newObjective(i);
    for (int i = 0; i < kBlueprintCount; ++i) {
        m_bpFound[i] = m_bpDone[i] = false;
        for (short& w : m_bpWall[i]) w = -1;
    }
    m_alive = 1;
    const int n = static_cast<int>(m_bots.size());
    for (int i = 0; i < n; ++i) {
        Bot& b = m_bots[i];
        b = Bot{};
        const int self = i + 1;
        if (i < kZombieSlots) {
            // Zombis: huecos libres hasta que aparecen (eliminados hace mucho: no se dibujan).
            b.kind = BotKind::Zombie;
            b.state = BotState::Eliminated;
            b.eliminatedAt = -1000.0f;
            b.lootDropped = true;
            b.body.reset({0.0f, -50.0f, 0.0f}, 0.0f, *m_world);
            b.body.place({0.0f, -50.0f, 0.0f});
            snprintf(m_names[self], sizeof(m_names[0]), "%s", i18n::tr(zombieDef(ZombieType::Walker).name));
            continue;
        }
        // Supervivientes: uno en cada campamento, junto a la hoguera, esperando ayuda.
        const int k = i - kZombieSlots;
        b.kind = BotKind::Survivor;
        b.home = world::zsites::kCamps[k % kSurvivorCount];
        const Vec3 spot = b.home + Vec3{0.0f, 0.0f, 0.8f};
        b.body.reset(spot, m_rng.range(-math3d::kPi, math3d::kPi), *m_world);
        b.body.state().health = b.body.state().maxHealth = m_config.zombie.allyHealth;  // v0.16
        b.body.state().maxShield = m_config.zombie.allyShield;
        b.state = BotState::Idle;
        b.palette = k % 3;
        b.decisionTimer = 0.1f * static_cast<float>(k + 1);
        b.progressAnchor = spot;
        b.strafeSign = m_rng.chance(0.5f) ? 1.0f : -1.0f;
        b.weapon.reset(m_config.botWeapon);
        b.searchYaw = b.body.state().yaw;
        snprintf(m_names[self], sizeof(m_names[0]), "%s", kSurvivorNames[k % kSurvivorCount]);
        ++m_alive;
    }
}

void Match::applyMapState() {
    // Estado de los objetos del mapa al empezar: en el modo zombi, los suyos en pie y parte
    // del resto derruido (siempre los mismos: el mapa en ruinas es fijo); en los demas modos,
    // todo en pie salvo los objetos del modo zombi. Solo se tocan la colision y la navegacion
    // de lo que cambia (tambien lo que rompio la partida anterior).
    if (!m_harvestables) return;
    const bool zombies = zombieMode();
    for (size_t i = 0; i < m_harvestables->size(); ++i) {
        const world::Harvestable& h = (*m_harvestables)[i];
        bool gone = h.zombieOnly;
        if (zombies) {
            u32 hsh = static_cast<u32>(i) * 2654435761u;
            hsh = (hsh ^ (hsh >> 15)) * 2246822519u;
            gone = !h.zombieOnly && static_cast<int>((hsh >> 8) % 100u) < h.ruinChance;
        }
        bool disabled = false;
        for (int k = 0; k < h.colliderCount; ++k) disabled |= !m_world->staticEnabled(h.collider + k);
        m_objectHealth[i] = gone ? 0.0f : h.health;
        m_objectDestroyed[i] = gone ? 1 : 0;
        if (disabled == gone) continue;
        for (int k = 0; k < h.colliderCount; ++k) m_world->setStaticEnabled(h.collider + k, !gone);
        m_nav->updateRegion(*m_world, h.box);
    }
}

// --- Consultas ---------------------------------------------------------------------------------

float Match::timeToChange() const {
    const float length = m_night ? m_config.zombie.nightLength : m_config.zombie.dayLength;
    return fmaxf(0.0f, length - m_zClock);
}

float Match::darkness() const {
    if (!zombieMode()) return 0.0f;
    const ZombieConfig& z = m_config.zombie;
    const float fade = fmaxf(z.duskSeconds, 0.1f);
    if (m_night) {
        // Ya es de noche; al acercarse el amanecer, aclara.
        return math3d::clampf((z.nightLength - m_zClock) / fade, 0.0f, 1.0f);
    }
    // De dia; oscurece al final (anochecer).
    return math3d::clampf(1.0f - (z.dayLength - m_zClock) / fade, 0.0f, 1.0f);
}

int Match::zombiesAlive() const {
    int n = 0;
    for (const Bot& b : m_bots) n += b.kind == BotKind::Zombie && b.state != BotState::Eliminated ? 1 : 0;
    return n;
}

int Match::survivorsRescued() const {
    int n = 0;
    for (const Bot& b : m_bots) n += b.kind == BotKind::Survivor && b.rescued ? 1 : 0;
    return n;
}

int Match::survivorsAlive() const {
    int n = 0;
    for (const Bot& b : m_bots) n += b.kind == BotKind::Survivor && b.rescued && b.state != BotState::Eliminated ? 1 : 0;
    return n;
}

bool Match::hostile(int a, int b) const {
    if (a == b) return false;
    if (!zombieMode()) return !teamMode() || !sameTeam(a, b);
    auto zombie = [&](int id) { return isZombie(id); };
    // Los supervivientes que aun esperan en su campamento no estan en la pelea, ni los que
    // duermen en su cama (v0.15: fuera de guardia, no se meten en problemas).
    auto waiting = [&](int id) { return isSurvivor(id) && (!m_bots[id - 1].rescued || m_bots[id - 1].sleeping); };
    if (waiting(a) || waiting(b)) return false;
    return zombie(a) != zombie(b);
}

float Match::entityScale(int id) const {
    if (!isZombie(id)) return 1.0f;
    return zombieDef(m_bots[id - 1].zombieType).scale;
}

// --- Dia, noche y aparicion de zombis ------------------------------------------------------------

void Match::updateZombieMode(float dt) {
    const ZombieConfig& z = m_config.zombie;
    if (m_phase == MatchPhase::Finished) return;
    // Durmiendo, el reloj del dia corre sleepTimeScale veces mas deprisa (v0.15).
    m_zClock += dt * sleepScale();
    if (!m_night && m_zClock >= z.dayLength) {
        m_night = true;
        m_zClock -= z.dayLength;
        pushEvent(GameEventType::NightStarted, -1, m_day, body(0).state().position);
        spawnHorde();
    } else if (m_night && m_zClock >= z.nightLength) {
        m_night = false;
        m_zClock -= z.nightLength;
        ++m_day;
        ++m_nightsSurvived;
        pushEvent(GameEventType::DayStarted, -1, m_day, body(0).state().position);
        restockContainers();
    }
    updateNeeds(dt);
    updateSleep(dt);
    updateObjectives(dt);
    if (m_allyBuildTimer > 0.0f) m_allyBuildTimer -= dt;
    // Aliados caidos: al rato se levantan como zombis armados.
    for (int i = kZombieSlots; i < botCount(); ++i) {
        Bot& b = m_bots[i];
        if (b.kind != BotKind::Survivor || b.state != BotState::Eliminated || b.riseTimer <= 0.0f) continue;
        b.riseTimer -= dt;
        if (b.riseTimer <= 0.0f) turnFallen(i);
    }

    // Poblacion: mas zombis de noche y cada dia que pasa.
    int target = (m_night ? z.nightZombies : z.dayZombies) + z.zombiesPerDay * (m_day - 1);
    if (target > kZombieSlots) target = kZombieSlots;
    m_spawnTimer -= dt;
    if (m_spawnTimer <= 0.0f && alive(0)) {
        m_spawnTimer = m_night ? z.nightSpawnInterval : z.daySpawnInterval;
        if (zombiesAlive() < target) spawnZombie(rollZombieType(), nullptr);
    }

    // Lo que el jugador descubre al pasar cerca (aparece en el minimapa).
    if (!alive(0)) return;
    updateSites();
    const Vec3 p = body(0).state().position;
    for (int i = 0; i < botCount(); ++i) {
        Bot& b = m_bots[i];
        if (b.kind != BotKind::Survivor || b.discovered || b.state == BotState::Eliminated) continue;
        if (distXZ(b.body.state().position, p) < kDiscoverRange) {
            b.discovered = true;
            pushEvent(GameEventType::SurvivorFound, i + 1, -1, b.body.state().position);
        }
    }
    for (size_t i = 0; i < m_containers.size(); ++i) {
        Container& c = m_containers[i];
        if (c.kind != ContainerKind::Workbench || c.discovered || distXZ(c.position, p) > kDiscoverRange) continue;
        c.discovered = true;
        pushEvent(GameEventType::WorkbenchFound, -1, static_cast<int>(i), c.position);
    }
}

ZombieType Match::rollZombieType() {
    const ZombieConfig& z = m_config.zombie;
    const float r = m_rng.uniform();
    if (m_night) {
        if (m_day >= z.brutesFromNight && r < 0.10f) return ZombieType::Brute;
        if (r < 0.40f) return ZombieType::Runner;
        return ZombieType::Walker;
    }
    if (m_day >= z.brutesFromNight + 1 && r < 0.05f) return ZombieType::Brute;
    if (m_day >= z.runnersFromDay && r < 0.22f) return ZombieType::Runner;
    return ZombieType::Walker;
}

int Match::spawnZombie(ZombieType type, const Vec3* near, float nearRadius) {
    // Un hueco libre (un zombi caido hace rato: su cuerpo ya no se ve).
    int slot = -1;
    for (int i = 0; i < kZombieSlots && i < botCount(); ++i) {
        const Bot& b = m_bots[i];
        if (b.kind == BotKind::Zombie && b.state == BotState::Eliminated && m_time - b.eliminatedAt > kCorpseReuse) {
            slot = i;
            break;
        }
    }
    if (slot < 0) return -1;
    const ZombieConfig& z = m_config.zombie;
    const Vec3 player = body(0).state().position;
    const Vec3 eye = eyeOf(0);
    Vec3 spot;
    bool found = false;
    for (int attempt = 0; attempt < 24 && !found; ++attempt) {
        Vec3 p;
        if (near) {
            if (!m_nav->randomPointNear(*near, nearRadius, m_rng, &p)) continue;
        } else {
            if (!m_nav->randomPointNear(player, z.spawnMaxDistance, m_rng, &p)) continue;
            const float d = distXZ(p, player);
            if (d < z.spawnMinDistance || d > z.spawnMaxDistance) continue;
            // Mejor donde no le vea aparecer (detras de algo), si se puede.
            if (attempt < 14 && lineOfSight(eye, p + Vec3{0.0f, 1.2f, 0.0f})) continue;
        }
        if (world::zsites::insideBase(p.x, p.z, 3.0f) || world::inWater(p.x, p.z)) continue;
        if (world::stormDepth(p.x, p.z) > -4.0f) continue;
        p.y = m_world->supportHeight(p.x, p.z, kFootRadius, p.y + 0.5f);
        if (m_world->overlapsAny(bodyBoxAt(p), world::kBlocksMovement)) continue;
        spot = p;
        found = true;
    }
    if (!found) return -1;
    Bot& b = m_bots[slot];
    b = Bot{};
    b.kind = BotKind::Zombie;
    b.zombieType = type;
    const ZombieDef& def = zombieDef(type);
    b.body.reset(spot, m_rng.range(-math3d::kPi, math3d::kPi), *m_world);
    CharacterState& s = b.body.state();
    s.health = s.maxHealth = def.health;
    s.shield = s.maxShield = 0.0f;
    b.state = BotState::Explore;
    b.armed = false;
    b.lootDropped = false;
    b.palette = m_rng.index(2);
    b.decisionTimer = m_rng.range(0.05f, kZombieDecision);
    b.progressAnchor = spot;
    b.sidestepSign = m_rng.chance(0.5f) ? 1.0f : -1.0f;
    b.swingTimer = m_rng.range(2.0f, 8.0f);  // primer gemido
    snprintf(m_names[slot + 1], sizeof(m_names[0]), "%s", i18n::tr(def.name));
    m_air[slot + 1] = AirState::Ground;
    m_inStorm[slot + 1] = false;
    ++m_alive;
    ++m_stats.zombiesSpawned;
    if (near) {
        // De la horda: van a por el jugador desde el principio.
        b.investigate = true;
        b.destination = player;
        b.target = 0;
    }
    return slot;
}

void Match::spawnHorde() {
    // Cada noche, una horda desde una direccion al azar, a ~45 m del jugador, que va a por el.
    const ZombieConfig& z = m_config.zombie;
    const Vec3 player = body(0).state().position;
    Vec3 from = player;
    bool found = false;
    for (int attempt = 0; attempt < 12 && !found; ++attempt) {
        const float a = m_rng.range(-math3d::kPi, math3d::kPi);
        Vec3 p;
        if (!m_nav->randomPointNear(player + Vec3{cosf(a) * 45.0f, 0.0f, sinf(a) * 45.0f}, 8.0f, m_rng, &p)) continue;
        if (world::stormDepth(p.x, p.z) > -6.0f || world::inWater(p.x, p.z)) continue;
        from = p;
        found = true;
    }
    if (!found) return;
    const int count = z.hordeBase + z.hordePerDay * (m_day - 1);
    int spawned = 0;
    for (int i = 0; i < count; ++i) {
        const ZombieType type = i == 0 && m_day >= z.brutesFromNight ? ZombieType::Brute
                                : (m_rng.chance(0.35f) ? ZombieType::Runner : ZombieType::Walker);
        if (spawnZombie(type, &from) >= 0) ++spawned;
    }
    pushEvent(GameEventType::HordeIncoming, -1, -1, from);
    m_events[(m_eventCount - 1) % kMaxEvents].amount = spawned;
}

// --- IA de los zombis --------------------------------------------------------------------------

void Match::decideZombie(int index) {
    Bot& b = m_bots[index];
    const int self = index + 1;
    const ZombieConfig& z = m_config.zombie;
    const Vec3 pos = b.body.state().position;
    // Los aliados caidos aun ven como personas (y disparan).
    const float sight = b.zombieType == ZombieType::Fallen ? 30.0f : (m_night ? z.nightSight : z.daySight);

    // Objetivo: el jugador o un superviviente rescatado. Lo notan si esta muy cerca (aunque
    // no lo vean) o si lo ven a su distancia de vision; al que ya persiguen lo siguen mas lejos.
    int best = -1;
    float bestD = 1e9f;
    for (int id = 0; id < entityCount(); ++id) {
        if (!alive(id) || !hostile(self, id)) continue;
        if (id == 0 && m_config.cheats.invisible) continue;
        const float d = distXZ(body(id).state().position, pos);
        const float range = id == b.target ? sight * 1.6f : sight;
        if (d > range) continue;
        if (d > z.smellRange && !lineOfSight(eyeOf(self), chestOf(id))) continue;
        const float score = id == b.target ? d * 0.7f : d;
        if (score < bestD) {
            bestD = score;
            best = id;
        }
    }
    b.breakingWall = false;
    if (best < 0 && b.state == BotState::Attack && b.target >= 0 && alive(b.target) && hostile(self, b.target) &&
        b.lostTime < 5.0f) {
        best = b.target;  // se ha escondido: va a donde lo vio por ultima vez
        b.lostTime += kZombieDecision;
    }
    if (best >= 0) {
        if (b.target != best || b.state != BotState::Attack) {
            b.pathLength = 0;
            b.pathRefresh = 0.0f;
            b.lostTime = 0.0f;
            if (m_rng.chance(0.5f)) pushEvent(GameEventType::ZombieGroan, self, -1, pos);
        }
        b.state = BotState::Attack;
        b.target = best;
        b.investigate = false;
        const Vec3 targetPos = body(best).state().position;
        b.lastSeen = targetPos;
        const float d = distXZ(pos, targetPos);
        // Una pieza construida entre los dos (el jugador encerrado en la base): a golpes.
        world::RayHit hit;
        const Vec3 from = chestOf(self), to = chestOf(best);
        if (m_world->raycast(from, to, world::kBlocksShots, &hit) && m_structures.wallFromCollider(hit.dynamicHandle) >= 0 &&
            hit.t * math3d::length(to - from) < zombieDef(b.zombieType).reach + 1.2f) {
            b.breakingWall = true;
            b.wallPoint = hit.point;
        }
        const bool direct = d < 3.5f || NavGrid::clearWalk(*m_world, pos, targetPos, kBodyRadius);
        if (direct) {
            b.pathLength = 0;
        } else if (b.pathRefresh <= 0.0f || b.pathIndex >= b.pathLength) {
            if (requestPath(b, targetPos)) b.pathRefresh = 1.2f;
        }
        return;
    }

    // Sin objetivo: un disparo que ha oido, de noche hacia el jugador (las hordas encuentran a
    // quien se esconde), y si no, deambular cerca.
    b.target = -1;
    if (b.state == BotState::Attack) {
        b.state = BotState::Search;
        b.stateTime = 0.0f;
        b.searchDuration = m_rng.range(1.0f, 3.0f);
    }
    const bool arrived = b.pathIndex >= b.pathLength;
    if (b.investigate) {
        b.investigate = false;
        if (requestPath(b, b.destination)) {
            b.state = BotState::Travel;
            b.stateTime = 0.0f;
        }
        return;
    }
    if (b.state == BotState::Search && b.stateTime < b.searchDuration) return;
    if (b.state == BotState::Travel && !arrived && b.stateTime < 20.0f) return;
    Vec3 dest;
    bool chosen = false;
    if (b.site >= 0) {
        // Guardian de un lugar de interes: no se aleja de el (v0.15).
        const world::zsites::Site& site = world::zsites::kSites[b.site];
        chosen = m_nav->randomPointNear({site.x, 0.0f, site.z}, site.radius * 0.7f, m_rng, &dest);
    } else if (m_night && alive(0) && m_rng.chance(0.55f)) {
        chosen = m_nav->randomPointNear(body(0).state().position, 12.0f, m_rng, &dest);
    }
    if (!chosen) chosen = m_nav->randomPointNear(pos, 12.0f, m_rng, &dest);
    if (chosen && !world::inWater(dest.x, dest.z) && requestPath(b, dest)) {
        b.destination = dest;
        b.state = BotState::Travel;
        b.stateTime = 0.0f;
    } else {
        b.state = BotState::Search;
        b.stateTime = 0.0f;
        b.searchDuration = m_rng.range(1.5f, 4.0f);
    }
}

void Match::updateZombie(int index, float dt) {
    Bot& b = m_bots[index];
    if (b.state == BotState::Eliminated) return;
    const int self = index + 1;
    if (m_config.cheats.freezeBots || m_config.cheats.partyBots) {
        b.aiming = false;
        b.body.step(MoveIntent{}, dt, *m_world);
        return;
    }
    const u64 t0 = core::nowMicros();
    const ZombieDef& def = zombieDef(b.zombieType);
    if (b.dormant) {
        // Tumbado entre las ruinas (v0.15): se levanta si el jugador (o un aliado) se acerca
        // o si le hacen dano.
        bool wake = b.body.state().health < b.body.state().maxHealth;
        for (int id = 0; id < entityCount() && !wake; ++id) {
            if (id == self || !alive(id) || isZombie(id)) continue;
            if (isSurvivor(id) && (!m_bots[id - 1].rescued || m_bots[id - 1].sleeping)) continue;
            wake = distXZ(body(id).state().position, b.body.state().position) < m_config.zombie.lurkerWakeRange;
        }
        if (!wake) {
            b.body.step(MoveIntent{}, dt, *m_world);
            return;
        }
        b.dormant = false;
        b.state = BotState::Search;
        b.stateTime = 0.0f;
        b.searchDuration = 0.3f;
        b.decisionTimer = 0.05f;
        b.attackTimer = 0.7f;  // un momento para levantarse
        pushEvent(GameEventType::LurkerRise, self, -1, b.body.state().position);
    }
    if (b.zombieType == ZombieType::Fallen) {
        b.weapon.update(dt);
        b.reaction -= dt;
    }
    b.stateTime += dt;
    b.attackTimer -= dt;
    b.pathRefresh -= dt;
    b.sidestepTimer -= dt;
    b.swingTimer -= dt;
    b.decisionTimer -= dt;
    if (b.decisionTimer <= 0.0f) {
        decideZombie(index);
        ++m_stats.decisions;
        b.decisionTimer += kZombieDecision;
        if (b.decisionTimer <= 0.0f) b.decisionTimer = kZombieDecision;
    }
    if (b.swingTimer <= 0.0f) {
        // Gemido de vez en cuando (mas a menudo persiguiendo a alguien).
        b.swingTimer = m_rng.range(4.0f, 11.0f) * (b.state == BotState::Attack ? 0.5f : 1.0f);
        pushEvent(GameEventType::ZombieGroan, self, -1, b.body.state().position);
    }

    const CharacterState& s = b.body.state();
    const Vec3 pos = s.position;
    MoveIntent intent;
    Vec3 desired{0.0f, 0.0f, 0.0f};
    float speed = m_night ? def.nightSpeed : def.speed;
    auto followPath = [&]() {
        while (b.pathIndex < b.pathLength && distXZ(pos, b.path[b.pathIndex]) < kArriveDistance) ++b.pathIndex;
        if (b.pathIndex >= b.pathLength) return false;
        desired = math3d::normalize(flatten(b.path[b.pathIndex] - pos));
        return true;
    };
    float targetDistance = 1e9f;
    if (b.state == BotState::Attack && b.target >= 0 && alive(b.target)) {
        const Vec3 targetPos = body(b.target).state().position;
        targetDistance = distXZ(pos, targetPos);
        const Vec3 face = b.breakingWall ? b.wallPoint : targetPos;
        if (b.breakingWall) {
            const Vec3 to = flatten(b.wallPoint - pos);
            const float d = math3d::length(to);
            if (d > def.reach * 0.7f && d > 0.01f) desired = to * (1.0f / d);
        } else if (targetDistance > def.reach * 0.75f) {
            if (!followPath()) {
                const Vec3 to = flatten(targetPos - pos);
                if (targetDistance > 0.01f) desired = to * (1.0f / targetDistance);
            }
        }
        if (targetDistance < 4.0f || b.breakingWall) {
            intent.faceYawOverride = true;
            intent.faceYaw = yawTowards(pos, face);
        }
        if (b.zombieType == ZombieType::Fallen && b.armed) {
            // Aliado caido con arma: de cerca se para y dispara de frente.
            b.targetVisible = lineOfSight(eyeOf(self), chestOf(b.target));
            if (b.targetVisible && targetDistance < 14.0f && !b.breakingWall) desired = Vec3{0.0f, 0.0f, 0.0f};
            if (b.targetVisible) {
                intent.faceYawOverride = true;
                intent.faceYaw = yawTowards(pos, targetPos);
            }
        }
    } else if (b.state == BotState::Travel) {
        followPath();
        speed *= 0.6f;  // deambulando, despacio
    }
    intent = avoidObstacles(index, desired, speed, intent);
    const u64 t1 = core::nowMicros();
    b.body.step(intent, dt, *m_world);
    const u64 t2 = core::nowMicros();
    m_stats.physicsMicros += t2 - t1;

    if (b.zombieType == ZombieType::Fallen && b.armed && b.state == BotState::Attack) updateFallen(index, dt, targetDistance);
    // Golpe: al objetivo si esta al alcance y de frente; si hay una pieza en medio, a ella.
    if (b.state == BotState::Attack && b.attackTimer <= 0.0f && m_phase != MatchPhase::Finished) {
        const float night = m_night ? 1.2f : 1.0f;
        if (b.breakingWall) {
            world::RayHit hit;
            const Vec3 chest = chestOf(self);
            const Vec3 dir = flatten(b.wallPoint - chest);
            const float len = math3d::length(dir);
            const Vec3 end = len > 0.01f ? chest + dir * ((def.reach + 0.3f) / len) : chest;
            const int wall = m_world->raycast(chest, end, world::kBlocksShots, &hit)
                                 ? m_structures.wallFromCollider(hit.dynamicHandle)
                                 : -1;
            if (wall >= 0) {
                b.attackTimer = def.attackInterval;
                ++m_stats.zombieHits;
                pushEvent(GameEventType::ZombieAttack, self, -1, hit.point);
                damageWall(wall, def.structureDamage * night, self, hit.point);
            }
        } else if (b.target >= 0 && alive(b.target) && hostile(self, b.target) && targetDistance <= def.reach &&
                   fabsf(body(b.target).state().position.y - pos.y) < 1.7f &&
                   fabsf(wrapAngle(s.yaw - yawTowards(pos, body(b.target).state().position))) < 0.9f) {
            // No a traves de una pared: si hay una pieza construida en medio, a por ella.
            world::RayHit hit;
            if (m_world->raycast(chestOf(self), chestOf(b.target), world::kBlocksShots, &hit)) {
                if (m_structures.wallFromCollider(hit.dynamicHandle) >= 0) {
                    b.breakingWall = true;
                    b.wallPoint = hit.point;
                }
                b.attackTimer = 0.2f;
                b.aiming = b.state == BotState::Attack;
                m_stats.aiMicros += (t1 - t0) + (core::nowMicros() - t2);
                return;
            }
            b.attackTimer = def.attackInterval;
            ++m_stats.zombieHits;
            pushEvent(GameEventType::ZombieAttack, self, b.target, chestOf(b.target));
            m_damageSource = -1;
            applyDamage(b.target, def.damage * night, self);
        }
    }
    b.aiming = b.state == BotState::Attack && targetDistance < 6.0f;
    m_stats.aiMicros += (t1 - t0) + (core::nowMicros() - t2);
}

void Match::zombieDrops(int index) {
    // Al caer, a veces materiales (chatarra), municion o una venda: el sustento del modo.
    Bot& b = m_bots[index];
    if (b.lootDropped) return;
    b.lootDropped = true;
    const Vec3 pos = b.body.state().position;
    // v0.15: los brutos y los aliados caidos sueltan una mochila (+3 ranuras); los caidos,
    // ademas, todo lo que llevaban.
    if (b.zombieType == ZombieType::Brute || b.zombieType == ZombieType::Fallen) {
        Pickup pack;
        pack.active = true;
        pack.kind = PickupKind::Backpack;
        pack.count = Inventory::kBackpackSlots;
        dropAround(pos + Vec3{0.6f, 0.0f, 0.0f}, pack, 0, 1);
        if (b.zombieType == ZombieType::Fallen) {
            dropBotWeapon(index);
            return;
        }
    }
    const float r = m_rng.uniform();
    const float brute = b.zombieType == ZombieType::Brute ? 2.0f : 1.0f;
    Pickup p;
    p.active = true;
    if (r < 0.40f) {
        p.kind = PickupKind::Materials;
        const float m = m_rng.uniform();
        p.resource = m < 0.45f ? world::Resource::Metal : (m < 0.8f ? world::Resource::Wood : world::Resource::Stone);
        p.count = static_cast<int>((8 + m_rng.index(10)) * brute);
    } else if (r < 0.60f) {
        p.kind = PickupKind::Ammo;
        p.ammo = m_rng.chance(0.5f) ? AmmoType::Shells : AmmoType::Light;
        p.count = ammoPerPickup(p.ammo) / 2 + 1;
    } else if (r < 0.66f) {
        p.kind = PickupKind::Item;
        p.item = ItemType::Bandage;
        p.rarity = itemDef(ItemType::Bandage).rarity;
        p.count = 2;
    } else if (r < 0.71f) {
        p.kind = PickupKind::Item;  // v0.15: una lata que llevaba encima
        p.item = ItemType::CannedFood;
        p.rarity = itemDef(ItemType::CannedFood).rarity;
        p.count = 1;
    } else {
        return;
    }
    dropAround(pos, p, 0, 1);
}

// --- Supervivientes ----------------------------------------------------------------------------

int Match::survivorNeeds(int botIndex) const {
    if (botIndex < 0 || botIndex >= botCount()) return 0;
    const Bot& b = m_bots[botIndex];
    if (b.kind != BotKind::Survivor || b.rescued) return 0;
    bool weapon = false, heal = false;
    for (int i = 0; i < b.bag.slotCount(); ++i) {
        weapon |= isWeapon(b.bag.slot(i).type);
        heal |= healItem(b.bag.slot(i).type);
    }
    return (weapon ? 0 : 1) | (heal ? 0 : 2);
}

bool Match::giveToSurvivor(int botIndex) {
    if (botIndex < 0 || botIndex >= botCount() || !alive(0)) return false;
    Bot& b = m_bots[botIndex];
    const int self = botIndex + 1;
    if (b.kind != BotKind::Survivor || b.rescued || b.state == BotState::Eliminated) return false;
    const int needs = survivorNeeds(botIndex);
    int given = -1;
    if (!m_inventory.pickaxe()) {
        const int slot = m_inventory.selected();
        const InventorySlot& held = m_inventory.slot(slot);
        if ((needs & 1) && isWeapon(held.type)) {
            cancelUse();
            const InventorySlot taken = m_inventory.take(slot);
            b.bag.add(taken.type, taken.rarity, 1, taken.weapon.ammo());
            given = static_cast<int>(taken.type);
        } else if ((needs & 2) && healItem(held.type)) {
            // Hasta tres de un monton (vendas, minipociones...).
            cancelUse();
            const ItemType type = held.type;
            const Rarity rarity = held.rarity;
            const int n = held.count < 3 ? held.count : 3;
            for (int k = 0; k < n; ++k) m_inventory.consumeOne(slot);
            b.bag.add(type, rarity, n);
            given = static_cast<int>(type);
        }
    }
    const int left = survivorNeeds(botIndex);
    pushEvent(GameEventType::SurvivorGift, self, given, b.body.state().position);
    m_events[(m_eventCount - 1) % kMaxEvents].amount = left;
    if (left == 0) {
        // Rescatado: saca el arma y sigue al jugador.
        b.rescued = true;
        b.discovered = true;
        ++m_rescuedTotal;
        b.materials.add(world::Resource::Wood, m_config.zombie.allyStartWood);  // para cubrirse
        botEquip(b, 20.0f);
        b.state = BotState::Search;
        b.stateTime = 0.0f;
        b.searchDuration = 0.5f;
        pushEvent(GameEventType::SurvivorRescued, self, -1, b.body.state().position);
    }
    return given >= 0;
}

// --- Centros de crafteo ------------------------------------------------------------------------

int Match::nearbyWorkbench() const {
    if (!alive(0) || m_air[0] != AirState::Ground) return -1;
    const Vec3 p = body(0).state().position;
    for (size_t i = 0; i < m_containers.size(); ++i) {
        const Container& c = m_containers[i];
        if (c.kind == ContainerKind::Workbench && distXZ(c.position, p) <= kWorkbenchReach && fabsf(c.position.y - p.y) < 1.6f) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

bool Match::canCraft(int r) const {
    if (r < 0 || r >= recipeCount()) return false;
    if (m_config.cheats.infiniteMaterials) return true;
    const Recipe& rc = recipe(r);
    for (int m = 0; m < static_cast<int>(world::Resource::Count); ++m) {
        if (m_materials.amount[m] < rc.cost[m]) return false;
    }
    return true;
}

bool Match::craft(int r) {
    const int bench = nearbyWorkbench();
    if (bench < 0 || r < 0 || r >= recipeCount()) return false;
    if (!canCraft(r)) {
        pushEvent(GameEventType::Crafted, 0, r, body(0).state().position);
        return false;
    }
    const Recipe& rc = recipe(r);
    if (!m_config.cheats.infiniteMaterials) {
        for (int m = 0; m < static_cast<int>(world::Resource::Count); ++m) {
            m_materials.spend(static_cast<world::Resource>(m), rc.cost[m]);
        }
    }
    const Vec3 front = m_containers[bench].position + forwardFromYaw(m_containers[bench].yaw) * 1.2f;
    if (rc.item == ItemType::None) {
        // Municion del arma en la mano (o de la primera; si no hay ninguna, media).
        const Weapon* w = m_inventory.activeWeapon() ? m_inventory.activeWeapon() : m_inventory.firstWeapon();
        const AmmoType ammo = w ? static_cast<AmmoType>(w->config().ammoType) : AmmoType::Medium;
        m_inventory.addAmmo(ammo, ammoPerPickup(ammo) * rc.count);
    } else if (itemDef(rc.item).kind == ItemKind::Trap) {
        giveTraps(rc.count);
    } else {
        const Rarity rarity = isWeapon(rc.item) ? existingRarity(rc.item, rc.rarity) : rc.rarity;
        const bool handsFree = m_inventory.activeType() == ItemType::None;
        const int left = m_inventory.add(rc.item, rarity, rc.count);
        if (left > 0) {
            // Inventario lleno: lo fabricado queda en el suelo, delante del banco.
            Pickup p;
            p.kind = PickupKind::Item;
            p.item = rc.item;
            p.rarity = rarity;
            p.count = left;
            dropAround(front, p, 0, 1);
        } else if (handsFree) {
            for (int i = 0; i < m_inventory.slotCount(); ++i) {
                if (m_inventory.slot(i).type == rc.item) m_inventory.select(i);
            }
        }
        if (isWeapon(rc.item)) {
            // Con su primer cargador de reserva.
            const AmmoType ammo = static_cast<AmmoType>(weaponConfig(rc.item, rarity).ammoType);
            m_inventory.addAmmo(ammo, ammoPerPickup(ammo));
        }
    }
    ++m_itemsCrafted;
    objectiveEvent(ObjectiveType::Craft);
    pushEvent(GameEventType::Crafted, 0, r, front);
    m_events[(m_eventCount - 1) % kMaxEvents].amount = 1;
    return true;
}

// --- Planos ------------------------------------------------------------------------------------

int Match::blueprintsDone() const {
    int n = 0;
    for (bool d : m_bpDone) n += d ? 1 : 0;
    return n;
}

void Match::giveBlueprint(int i) {
    if (i < 0 || i >= kBlueprintCount || m_bpFound[i]) return;
    m_bpFound[i] = true;
    pushEvent(GameEventType::BlueprintFound, 0, i, body(0).state().position);
}

WallPlacement Match::blueprintPiece(int bp, int piece) const {
    return blueprintPlacement(blueprint(bp).pieces[piece], m_config.build);
}

bool Match::pieceBuilt(int bp, int piece) const {
    if (bp < 0 || bp >= kBlueprintCount || piece < 0 || piece >= blueprint(bp).pieceCount) return false;
    const int w = m_bpWall[bp][piece];
    if (w < 0) return false;
    const BuiltWall& wall = m_structures.wall(w);
    // El hueco de una pieza rota puede haberlo ocupado otra: tiene que ser la misma.
    const world::Aabb box = blueprintPiece(bp, piece).box;
    return wall.active && wall.owner == kBlueprintOwner && fabsf(wall.box.min.x - box.min.x) < 0.05f &&
           fabsf(wall.box.min.z - box.min.z) < 0.05f && fabsf(wall.box.max.y - box.max.y) < 0.05f;
}

bool Match::buildBlueprintPiece(int bp, int piece) {
    if (!zombieMode() || !blueprintFound(bp) || piece < 0 || piece >= blueprint(bp).pieceCount || !alive(0)) return false;
    if (pieceBuilt(bp, piece)) return false;
    WallPlacement p = blueprintPiece(bp, piece);
    const int cost = m_config.zombie.pieceCost;
    // El material elegido para construir o, si no hay bastante, el que haya.
    p.material = m_buildMaterial;
    if (m_materials.get(p.material) < cost) {
        for (int r = static_cast<int>(world::Resource::Count) - 1; r >= 0; --r) {
            if (m_materials.get(static_cast<world::Resource>(r)) >= cost) p.material = static_cast<world::Resource>(r);
        }
    }
    const bool free = m_config.cheats.infiniteMaterials;
    if (!free && m_materials.get(p.material) < cost) return false;
    // Nadie dentro de la pieza (se quedaria atrapado).
    const world::Aabb shrunk{p.box.min + Vec3{0.03f, 0.03f, 0.03f}, p.box.max - Vec3{0.03f, 0.03f, 0.03f}};
    world::Aabb boxes[kMaxPieceBoxes];
    const int n = pieceBoxes(p, m_config.build, boxes);
    for (int id = 0; id < entityCount(); ++id) {
        if (!alive(id)) continue;
        const world::Aabb body = bodyBounds(id);
        for (int k = 0; k < n; ++k) {
            const world::Aabb s{boxes[k].min + Vec3{0.03f, 0.03f, 0.03f}, boxes[k].max - Vec3{0.03f, 0.03f, 0.03f}};
            if (s.overlaps(body)) return false;
        }
    }
    (void)shrunk;
    const int wall = m_structures.placeForced(p, kBlueprintOwner, m_config.zombie.pieceHealthScale);
    if (wall < 0) return false;
    if (!free) m_materials.spend(p.material, cost);
    m_bpWall[bp][piece] = static_cast<short>(wall);
    ++m_stats.wallsBuilt;
    objectiveEvent(ObjectiveType::BuildPieces);
    pushEvent(GameEventType::WallBuilt, 0, -1, (p.box.min + p.box.max) * 0.5f);
    pushEvent(GameEventType::BlueprintPiece, 0, bp, (p.box.min + p.box.max) * 0.5f);
    m_events[(m_eventCount - 1) % kMaxEvents].amount = piece;
    if (!m_bpDone[bp]) {
        bool all = true;
        for (int k = 0; k < blueprint(bp).pieceCount && all; ++k) all = pieceBuilt(bp, k);
        if (all) {
            m_bpDone[bp] = true;
            pushEvent(GameEventType::BlueprintDone, 0, bp, (p.box.min + p.box.max) * 0.5f);
        }
    }
    return true;
}

// --- v0.15: hambre y sueno -----------------------------------------------------------------------

bool Match::zombiesNear(const Vec3& p, float range) const {
    for (int i = 0; i < botCount(); ++i) {
        const Bot& b = m_bots[i];
        if (b.kind != BotKind::Zombie || b.state == BotState::Eliminated) continue;
        if (distXZ(b.body.state().position, p) < range) return true;
    }
    return false;
}

void Match::updateNeeds(float dt) {
    // Hambre y descanso bajan con el reloj del dia (durmiendo, mas deprisa el hambre; el
    // descanso, al reves, sube en updateSleep). A cero, se pierde vida poco a poco.
    if (!alive(0)) return;
    const ZombieConfig& z = m_config.zombie;
    const float clock = dt * sleepScale();
    m_food = fmaxf(0.0f, m_food - z.foodPerSecond * clock);
    if (!m_sleeping) m_rest = fmaxf(0.0f, m_rest - z.restPerSecond * clock);
    const Vec3 at = chestOf(0);
    const int hunger = m_food <= 0.0f ? 2 : (m_food < 25.0f ? 1 : 0);
    if (hunger > m_hungerStage) {
        pushEvent(GameEventType::Hungry, 0, -1, at);
        m_events[(m_eventCount - 1) % kMaxEvents].amount = hunger;
    }
    m_hungerStage = hunger;
    const int tiredness = m_rest <= 0.0f ? 2 : (m_rest < z.tiredLevel ? 1 : 0);
    if (tiredness > m_tiredStage) {
        pushEvent(GameEventType::Tired, 0, -1, at);
        m_events[(m_eventCount - 1) % kMaxEvents].amount = tiredness;
    }
    m_tiredStage = tiredness;
    // Dano por hambre o agotamiento, un golpe por segundo (sin escudo que valga).
    if (hunger == 2 || tiredness == 2) {
        m_needsTimer += clock;
        if (m_needsTimer >= 1.0f) {
            const float damage = (hunger == 2 ? z.starveDamage : 0.0f) + (tiredness == 2 ? z.exhaustedDamage : 0.0f);
            m_needsTimer -= 1.0f;
            m_damageSource = -1;
            applyDamage(0, damage, -1, true);
        }
    } else {
        m_needsTimer = 0.0f;
    }
    // Cansado: de vez en cuando se equivoca (tropieza, se le escapa un tiro, se le caen balas).
    if (m_stumbleTimer > 0.0f) m_stumbleTimer -= dt;
    if (tiredness == 0 || m_sleeping) return;
    m_fumbleTimer -= dt * (tiredness == 2 ? 2.0f : 1.0f);
    if (m_fumbleTimer > 0.0f) return;
    m_fumbleTimer = m_rng.range(22.0f, 40.0f);
    Fumble kind = static_cast<Fumble>(m_rng.index(3));
    Weapon* w = m_inventory.activeWeapon();
    if (kind == Fumble::Misfire && (!w || w->ammo() <= 0 || m_air[0] != AirState::Ground)) kind = Fumble::Stumble;
    if (kind == Fumble::DropAmmo && (!w || m_inventory.ammo(static_cast<AmmoType>(w->config().ammoType)) < 4)) kind = Fumble::Stumble;
    switch (kind) {
        case Fumble::Stumble:
            m_stumbleTimer = 1.4f;
            if (Weapon* held = m_inventory.activeWeapon()) held->cancelReload();
            cancelUse();
            break;
        case Fumble::Misfire: {
            // Se le escapa un disparo hacia donde no queria.
            if (!w->tryFire()) {
                kind = Fumble::Stumble;
                m_stumbleTimer = 1.4f;
                break;
            }
            PlayerAim wild = m_aim;
            const float yaw = m_rng.range(-0.6f, 0.6f), pitch = m_rng.range(-0.35f, 0.1f);
            wild.direction = math3d::normalize(wild.direction + Vec3{sinf(yaw), pitch, cosf(yaw)} * 0.6f);
            const WeaponConfig& c = w->config();
            if (c.projectile == ProjectileKind::None) {
                playerShot(wild, c.damage / static_cast<float>(c.pellets > 0 ? c.pellets : 1), c.range, 8.0f, &c, true,
                           static_cast<int>(m_inventory.activeType()));
            }
            break;
        }
        case Fumble::DropAmmo: {
            // Se le cae parte de la municion del arma en la mano (queda a sus pies).
            const AmmoType ammo = static_cast<AmmoType>(w->config().ammoType);
            const int lost = m_inventory.ammo(ammo) / 4;
            m_inventory.setAmmo(ammo, m_inventory.ammo(ammo) - lost);
            w->setReserve(m_inventory.ammo(ammo));
            Pickup p;
            p.active = true;
            p.kind = PickupKind::Ammo;
            p.ammo = ammo;
            p.count = lost;
            dropAround(body(0).state().position + forwardFromYaw(m_input.cameraYaw) * 0.8f, p, 0, 1);
            break;
        }
    }
    pushEvent(GameEventType::Fumble, 0, -1, at);
    m_events[(m_eventCount - 1) % kMaxEvents].amount = static_cast<int>(kind);
}

bool Match::trySleep(int bed) {
    if (!zombieMode() || !alive(0) || m_sleeping || m_air[0] != AirState::Ground) return false;
    const ZombieConfig& z = m_config.zombie;
    const Vec3 p = body(0).state().position;
    if (m_rest >= 99.0f) {
        pushEvent(GameEventType::SleepBlocked, 0, 1, p);
        return false;
    }
    if (zombiesNear(p, z.sleepBlockRange) && !m_config.cheats.invisible) {
        pushEvent(GameEventType::SleepBlocked, 0, 0, p);
        return false;
    }
    if (bed >= 0) {
        if (bed >= static_cast<int>(m_containers.size()) || m_containers[bed].kind != ContainerKind::Bed) return false;
        // v0.16: una cama asignada es de ese aliado (se le quita con L sobre la cama).
        if (bedOwner(bed) >= 0) {
            pushEvent(GameEventType::SleepBlocked, 0, 2, p);
            return false;
        }
        const Container& c = m_containers[bed];
        m_player.place(c.position + Vec3{0.0f, 0.0f, 0.0f});
        m_player.state().yaw = c.yaw;
    }
    cancelUse();
    if (Weapon* w = m_inventory.activeWeapon()) w->cancelReload();
    m_buildMode = false;
    m_playerCrouched = false;
    m_sleeping = bed >= 0 ? 1 : 2;
    m_sleepBed = bed;
    m_sleepRested = 0.0f;
    pushEvent(GameEventType::SleepStarted, 0, bed, p);
    return true;
}

void Match::wakeUp(SleepEnd why) {
    if (!m_sleeping) return;
    m_sleeping = 0;
    m_sleepBed = -1;
    if (why == SleepEnd::Rested || m_sleepRested >= 60.0f) objectiveEvent(ObjectiveType::SleepWell);
    pushEvent(GameEventType::SleepEnded, 0, -1, body(0).state().position);
    m_events[(m_eventCount - 1) % kMaxEvents].amount = static_cast<int>(why);
}

void Match::updateSleep(float dt) {
    if (!m_sleeping) return;
    if (!alive(0)) {
        m_sleeping = 0;
        return;
    }
    const ZombieConfig& z = m_config.zombie;
    // En la cama el descanso se llena en media noche; en el saco, en una noche entera
    // (tiempo del reloj del dia, que corre sleepTimeScale veces mas deprisa).
    const float fill = m_sleeping == 1 ? z.nightLength * 0.5f : z.nightLength;
    const float gained = 100.0f / fmaxf(fill, 1.0f) * dt * sleepScale();
    m_rest = fminf(100.0f, m_rest + gained);
    m_sleepRested += gained;
    if (m_sincePlayerDamaged < 0.05f) {
        wakeUp(SleepEnd::Hurt);
    } else if (zombiesNear(body(0).state().position, z.sleepWakeRange) && !m_config.cheats.invisible) {
        wakeUp(SleepEnd::Zombie);
    } else if (m_rest >= 100.0f) {
        wakeUp(SleepEnd::Rested);
    }
}

int Match::bedCount() const {
    int n = 0;
    for (const Container& c : m_containers) n += c.kind == ContainerKind::Bed ? 1 : 0;
    return n;
}

int Match::placeBed() {
    // R con una cama en la mano: delante, en el suelo, si cabe (ni paredes ni otra cama).
    if (!zombieMode() || !alive(0) || m_air[0] != AirState::Ground || m_inventory.pickaxe()) return -1;
    const int slot = m_inventory.selected();
    if (m_inventory.slot(slot).type != ItemType::Bed || bedCount() >= kMaxBeds) return -1;
    const float yaw = m_input.cameraYaw;
    const Vec3 feet = body(0).state().position;
    Vec3 p = feet + forwardFromYaw(yaw) * 1.6f;
    p.y = m_world->supportHeight(p.x, p.z, 0.3f, feet.y + 0.6f);
    if (fabsf(p.y - feet.y) > 0.6f) return -1;
    const bool alongX = fabsf(sinf(yaw)) > fabsf(cosf(yaw));
    const float ax = alongX ? 1.0f : 0.5f, az = alongX ? 0.5f : 1.0f;
    const world::Aabb box{{p.x - ax, p.y + 0.05f, p.z - az}, {p.x + ax, p.y + 0.6f, p.z + az}};
    if (m_world->overlapsAny(box, world::kBlocksMovement)) return -1;
    for (const Container& c : m_containers) {
        if (c.kind == ContainerKind::Bed && distXZ(c.position, p) < 1.3f) return -1;
    }
    m_inventory.consumeOne(slot);
    const int index = addContainer(ContainerKind::Bed, p, yaw);
    pushEvent(GameEventType::BedPlaced, 0, index, p);
    return index;
}

// --- v0.15: lugares de interes, guardianes y reabastecimiento -------------------------------------

void Match::updateSites() {
    using namespace world::zsites;
    const Vec3 p = body(0).state().position;
    for (int i = 0; i < kSiteCount; ++i) {
        const Site& site = kSites[i];
        const float d = sqrtf((p.x - site.x) * (p.x - site.x) + (p.z - site.z) * (p.z - site.z));
        if (d < site.radius) {
            if (!m_siteFound[i]) {
                m_siteFound[i] = true;
                pushEvent(GameEventType::SiteDiscovered, -1, i, {site.x, 0.0f, site.z});
            }
            for (Objective& o : m_objectives) {
                if (o.type == ObjectiveType::ExploreSite && o.site == i && o.doneTime < 0.0f) {
                    o.progress = o.target - 1;
                    objectiveEvent(ObjectiveType::ExploreSite);
                }
            }
        }
        // Sus guardianes salen al acercarse, una vez por dia.
        if (d < site.radius + m_config.zombie.siteGuardRange && m_siteGuardDay[i] != m_day) spawnSiteGuards(i);
    }
}

void Match::spawnSiteGuards(int index) {
    using namespace world::zsites;
    const Site& site = kSites[index];
    m_siteGuardDay[index] = m_day;
    const Vec3 center{site.x, 0.0f, site.z};
    const bool big = site.kind == SiteKind::BigHouse || site.kind == SiteKind::Storehouse;
    const int count = site.guards + (m_day - 1) / 2;
    for (int k = 0; k < count; ++k) {
        ZombieType type = m_rng.chance(m_night ? 0.35f : 0.15f) ? ZombieType::Runner : ZombieType::Walker;
        if (k == 0 && big && m_day >= 2) type = ZombieType::Brute;
        const int slot = spawnZombie(type, &center, site.radius * 0.6f);
        if (slot < 0) break;
        Bot& b = m_bots[slot];
        b.site = index;
        b.investigate = false;
        b.target = -1;
        // Algunos, tumbados entre las ruinas como si fueran cadaveres (de dia, mas).
        if (type != ZombieType::Brute && m_rng.chance(m_config.zombie.lurkerChance * (m_night ? 0.6f : 1.0f))) {
            b.dormant = true;
            b.state = BotState::Idle;
        }
        ++m_stats.guardsSpawned;
    }
}

int Match::spawnZombieAt(ZombieType type, const Vec3& near) { return spawnZombie(type, &near, 4.0f); }

void Match::restockContainers() {
    // Al amanecer, parte de lo saqueado vuelve a tener algo (la partida es larga).
    int restocked = 0;
    for (Container& c : m_containers) {
        const bool kind = c.kind == ContainerKind::Chest || c.kind == ContainerKind::AmmoBox ||
                          c.kind == ContainerKind::Fridge || c.kind == ContainerKind::Pantry;
        if (!kind || !c.opened || !m_rng.chance(m_config.zombie.restockChance)) continue;
        c.opened = false;
        c.claimedBy = -1;
        ++restocked;
    }
    if (restocked > 0) {
        pushEvent(GameEventType::Restocked, -1, -1, body(0).state().position);
        m_events[(m_eventCount - 1) % kMaxEvents].amount = restocked;
    }
}

// --- v0.15: aliados (inventario, camas, obras y caidos) ----------------------------------------

bool Match::allyGive(int botIndex, int slot) {
    if (botIndex < 0 || botIndex >= botCount() || slot < 0 || slot >= m_inventory.slotCount() || !alive(0)) return false;
    Bot& b = m_bots[botIndex];
    if (b.kind != BotKind::Survivor || !b.rescued || b.state == BotState::Eliminated) return false;
    const InventorySlot& held = m_inventory.slot(slot);
    if (held.type == ItemType::None) return false;
    const ItemKind kind = itemDef(held.type).kind;
    if (kind != ItemKind::Weapon && kind != ItemKind::Consumable && kind != ItemKind::Throwable) return false;
    if (b.bag.room(held.type) <= 0) return false;
    if (m_useSlot == slot) cancelUse();
    if (Weapon* w = m_inventory.activeWeapon()) w->cancelReload();
    const InventorySlot taken = m_inventory.take(slot);
    const int left = b.bag.add(taken.type, taken.rarity, taken.count, isWeapon(taken.type) ? taken.weapon.ammo() : -1);
    if (left > 0) m_inventory.add(taken.type, taken.rarity, left);
    botEquip(b, b.target >= 0 && alive(b.target) ? distXZ(body(b.target).state().position, b.body.state().position) : 15.0f);
    pushEvent(GameEventType::SurvivorGift, botIndex + 1, static_cast<int>(taken.type), b.body.state().position);
    return true;
}

bool Match::allyTake(int botIndex, int botSlot) {
    if (botIndex < 0 || botIndex >= botCount() || !alive(0)) return false;
    Bot& b = m_bots[botIndex];
    if (b.kind != BotKind::Survivor || !b.rescued || b.state == BotState::Eliminated) return false;
    if (botSlot < 0 || botSlot >= b.bag.slotCount()) return false;
    const InventorySlot& s = b.bag.slot(botSlot);
    if (s.type == ItemType::None || m_inventory.room(s.type) <= 0) return false;
    if (b.healSlot == botSlot) {
        b.healTimer = 0.0f;
        b.healSlot = -1;
    }
    const InventorySlot taken = b.bag.take(botSlot);
    const int left = m_inventory.add(taken.type, taken.rarity, taken.count, isWeapon(taken.type) ? taken.weapon.ammo() : -1);
    if (left > 0) b.bag.add(taken.type, taken.rarity, left);
    if (isWeapon(taken.type)) {
        // Con su municion (los aliados la tienen infinita; al jugador le llega un cargador).
        m_inventory.addAmmo(static_cast<AmmoType>(weaponConfig(taken.type, taken.rarity).ammoType),
                            ammoPerPickup(static_cast<AmmoType>(weaponConfig(taken.type, taken.rarity).ammoType)));
    }
    bool weapon = false;
    for (int i = 0; i < b.bag.slotCount(); ++i) weapon |= isWeapon(b.bag.slot(i).type);
    if (!weapon) {
        b.armed = false;
        b.weaponType = ItemType::None;
    } else {
        b.armed = false;  // vuelve a elegir entre las que le quedan
        botEquip(b, 15.0f);
    }
    pushEvent(GameEventType::PickedUp, 0, static_cast<int>(taken.type), b.body.state().position);
    return true;
}

int Match::pieceKeyNear(const Vec3& p) const {
    // Pieza de la base por levantar mas cercana a `p` que no tenga ya otro aliado: las del
    // muro del perimetro siempre; las de los demas planos, si se han encontrado.
    int best = 0;
    float bestD = 1e9f;
    for (int bp = 0; bp < kBlueprintCount; ++bp) {
        if (bp >= kPalisadeBlueprints && !m_bpFound[bp]) continue;
        for (int k = 0; k < blueprint(bp).pieceCount; ++k) {
            if (pieceBuilt(bp, k)) continue;
            const int key = bp * kMaxBlueprintPieces + k + 1;
            bool taken = false;
            for (const Bot& o : m_bots) taken |= o.kind == BotKind::Survivor && o.allyPiece == key;
            if (taken) continue;
            // Sin material del jugador solo el muro del perimetro (con lo que rapinan).
            if (bp >= kPalisadeBlueprints && m_materials.get(world::Resource::Wood) < m_config.zombie.pieceCost &&
                m_materials.get(world::Resource::Stone) < m_config.zombie.pieceCost &&
                m_materials.get(world::Resource::Metal) < m_config.zombie.pieceCost && !m_config.cheats.infiniteMaterials) {
                continue;
            }
            const world::Aabb box = blueprintPiece(bp, k).box;
            const float d = distXZ((box.min + box.max) * 0.5f, p);
            if (d < bestD) {
                bestD = d;
                best = key;
            }
        }
    }
    return best;
}

void Match::turnFallen(int index) {
    // El aliado caido se levanta como zombi armado: con sus armas, contra todos.
    Bot& b = m_bots[index];
    const Vec3 pos = b.body.state().position;
    if (b.bed >= 0 && b.bed < static_cast<int>(m_containers.size())) m_containers[b.bed].claimedBy = -1;
    char name[16];
    snprintf(name, sizeof(name), "%s", m_names[index + 1]);
    Inventory bag = b.bag;
    const Materials materials = b.materials;
    const int palette = b.palette;
    b = Bot{};
    b.kind = BotKind::Zombie;
    b.zombieType = ZombieType::Fallen;
    b.turned = true;
    b.bag = bag;
    b.materials = materials;
    b.palette = palette;
    const ZombieDef& def = zombieDef(ZombieType::Fallen);
    b.body.reset(pos, m_rng.range(-math3d::kPi, math3d::kPi), *m_world);
    CharacterState& s = b.body.state();
    s.health = s.maxHealth = def.health;
    s.shield = s.maxShield = 0.0f;
    b.state = BotState::Search;
    b.searchDuration = 1.0f;
    b.decisionTimer = 0.2f;
    b.progressAnchor = pos;
    b.swingTimer = 1.0f;
    b.reaction = 0.8f;
    b.lootDropped = false;
    botEquip(b, 12.0f);
    snprintf(m_names[index + 1], sizeof(m_names[0]), "%.10s Z", name);
    m_air[index + 1] = AirState::Ground;
    ++m_alive;
    pushEvent(GameEventType::AllyTurned, index + 1, -1, pos);
}

void Match::updateFallen(int index, float /*dt*/, float targetDistance) {
    // Dispara (botShoot) si ve a su objetivo y esta a tiro; de cerca tambien golpea.
    Bot& b = m_bots[index];
    if (!b.targetVisible || targetDistance > 26.0f) return;
    if (b.reaction > 0.0f) return;
    botShoot(index);
}

// --- v0.15: objetivos ------------------------------------------------------------------------------

void Match::newObjective(int slot) {
    Objective& o = m_objectives[slot];
    o = Objective{};
    // Uno distinto del otro activo y del ultimo cumplido.
    ObjectiveType other = ObjectiveType::Count;
    for (int i = 0; i < kActiveObjectives; ++i) {
        if (i != slot) other = m_objectives[i].type;
    }
    ObjectiveType type = ObjectiveType::KillZombies;
    for (int attempt = 0; attempt < 20; ++attempt) {
        type = static_cast<ObjectiveType>(m_rng.index(static_cast<int>(ObjectiveType::Count)));
        if (type == other || type == m_lastObjective) continue;
        if (type == ObjectiveType::KillBrute && m_day < 2) continue;
        if (type == ObjectiveType::SleepWell && m_rest > 70.0f) continue;
        break;
    }
    o.type = type;
    const int day = m_day;
    switch (type) {
        case ObjectiveType::KillZombies: o.target = 8 + 2 * day; break;
        case ObjectiveType::KillRunners: o.target = 3 + day / 2; break;
        case ObjectiveType::KillBrute: o.target = 1; break;
        case ObjectiveType::OpenFridges: o.target = 3; break;
        case ObjectiveType::EatFood: o.target = 2; break;
        case ObjectiveType::Craft: o.target = 2; break;
        case ObjectiveType::BuildPieces: o.target = 8; break;
        case ObjectiveType::Harvest: o.target = 150; break;
        case ObjectiveType::SleepWell: o.target = 1; break;
        case ObjectiveType::Headshots: o.target = 5 + day; break;
        case ObjectiveType::ExploreSite: {
            // Un lugar sin descubrir (o, si ya estan todos, uno lejos del jugador).
            o.target = 1;
            const Vec3 p = body(0).state().position;
            int best = -1;
            float bestScore = -1.0f;
            for (int i = 0; i < world::zsites::kSiteCount; ++i) {
                const world::zsites::Site& site = world::zsites::kSites[i];
                const float d = distXZ({site.x, 0.0f, site.z}, p);
                if (d < site.radius + 5.0f) continue;
                const float score = (m_siteFound[i] ? 0.0f : 1000.0f) + m_rng.range(0.0f, 60.0f) - d * 0.2f;
                if (score > bestScore) {
                    bestScore = score;
                    best = i;
                }
            }
            o.site = best >= 0 ? best : 0;
            break;
        }
        case ObjectiveType::Count: break;
    }
    o.reward = static_cast<RewardType>(m_rng.index(static_cast<int>(RewardType::Count)));
}

void Match::setObjective(int slot, ObjectiveType type, int target) {
    if (slot < 0 || slot >= kActiveObjectives) return;
    m_objectives[slot] = Objective{};
    m_objectives[slot].type = type;
    m_objectives[slot].target = target;
    m_objectives[slot].site = 0;
}

void Match::objectiveEvent(ObjectiveType type, int amount) {
    if (!zombieMode()) return;
    for (int i = 0; i < kActiveObjectives; ++i) {
        Objective& o = m_objectives[i];
        if (o.type != type || o.doneTime >= 0.0f) continue;
        o.progress += amount;
        if (o.progress < o.target) continue;
        o.progress = o.target;
        o.doneTime = m_time;
        ++m_objectivesDone;
        m_lastObjective = type;
        grantReward(o.reward);
        pushEvent(GameEventType::ObjectiveDone, 0, i, body(0).state().position);
        m_events[(m_eventCount - 1) % kMaxEvents].amount = kObjectiveVbucks;
    }
}

void Match::updateObjectives(float /*dt*/) {
    for (int i = 0; i < kActiveObjectives; ++i) {
        Objective& o = m_objectives[i];
        if (o.doneTime >= 0.0f && m_time - o.doneTime > 4.0f) {
            newObjective(i);
            pushEvent(GameEventType::ObjectiveNew, 0, i, body(0).state().position);
        }
    }
}

void Match::grantReward(RewardType reward) {
    // Directo al inventario (lo que no quepa, a los pies del jugador).
    const Vec3 feet = body(0).state().position;
    auto item = [&](ItemType type, int count) {
        const int left = m_inventory.add(type, itemDef(type).rarity, count);
        if (left <= 0) return;
        Pickup p;
        p.active = true;
        p.kind = PickupKind::Item;
        p.item = type;
        p.rarity = itemDef(type).rarity;
        p.count = left;
        dropAround(feet, p, 0, 1);
    };
    switch (reward) {
        case RewardType::Materials:
            m_materials.add(world::Resource::Wood, 60);
            m_materials.add(world::Resource::Stone, 40);
            m_materials.add(world::Resource::Metal, 40);
            break;
        case RewardType::Ammo:
            for (int a = 0; a < static_cast<int>(AmmoType::Count); ++a) {
                m_inventory.addAmmo(static_cast<AmmoType>(a), ammoPerPickup(static_cast<AmmoType>(a)) * 2);
            }
            break;
        case RewardType::Food:
            item(ItemType::CannedFood, 2);
            item(ItemType::Ration, 1);
            break;
        case RewardType::Heals:
            item(ItemType::Medkit, 1);
            item(ItemType::Bandage, 5);
            break;
        case RewardType::Count: break;
    }
}

}  // namespace game
