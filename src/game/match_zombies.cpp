// Modo Supervivencia zombi de game::Match (v0.14, docs/ZOMBIS.md): dia y noche, zombis
// (aparicion, IA y golpes), supervivientes (rescate y seguimiento), centros de crafteo y
// planos de la base. Los datos del modo estan en game/zombies.h.
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

// La rareza existente mas cercana (como en el botin).
Rarity existingRarity(ItemType type, Rarity rarity) {
    int k = static_cast<int>(rarity);
    while (k < 4 && !weaponHasRarity(type, static_cast<Rarity>(k))) ++k;
    while (k > 0 && !weaponHasRarity(type, static_cast<Rarity>(k))) --k;
    return static_cast<Rarity>(k);
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
    if (!zombieMode()) return true;
    auto zombie = [&](int id) { return isZombie(id); };
    // Los supervivientes que aun esperan en su campamento no estan en la pelea.
    auto waiting = [&](int id) { return isSurvivor(id) && !m_bots[id - 1].rescued; };
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
    m_zClock += dt;
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

bool Match::spawnZombie(ZombieType type, const Vec3* near) {
    // Un hueco libre (un zombi caido hace rato: su cuerpo ya no se ve).
    int slot = -1;
    for (int i = 0; i < kZombieSlots && i < botCount(); ++i) {
        const Bot& b = m_bots[i];
        if (b.kind == BotKind::Zombie && b.state == BotState::Eliminated && m_time - b.eliminatedAt > kCorpseReuse) {
            slot = i;
            break;
        }
    }
    if (slot < 0) return false;
    const ZombieConfig& z = m_config.zombie;
    const Vec3 player = body(0).state().position;
    const Vec3 eye = eyeOf(0);
    Vec3 spot;
    bool found = false;
    for (int attempt = 0; attempt < 24 && !found; ++attempt) {
        Vec3 p;
        if (near) {
            if (!m_nav->randomPointNear(*near, 7.0f, m_rng, &p)) continue;
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
    if (!found) return false;
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
    return true;
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
        if (spawnZombie(type, &from)) ++spawned;
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
    const float sight = m_night ? z.nightSight : z.daySight;

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
    if (best < 0 && b.state == BotState::Attack && b.target >= 0 && alive(b.target) && b.lostTime < 5.0f) {
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
    if (m_night && alive(0) && m_rng.chance(0.55f)) {
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
    } else if (b.state == BotState::Travel) {
        followPath();
        speed *= 0.6f;  // deambulando, despacio
    }
    intent = avoidObstacles(index, desired, speed, intent);
    const u64 t1 = core::nowMicros();
    b.body.step(intent, dt, *m_world);
    const u64 t2 = core::nowMicros();
    m_stats.physicsMicros += t2 - t1;

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
        } else if (b.target >= 0 && alive(b.target) && targetDistance <= def.reach &&
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
    for (int i = 0; i < Inventory::kSlots; ++i) {
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
        botEquip(b, 20.0f);
        b.state = BotState::Search;
        b.stateTime = 0.0f;
        b.searchDuration = 0.5f;
        pushEvent(GameEventType::SurvivorRescued, self, -1, b.body.state().position);
    }
    return given >= 0;
}

void Match::decideSurvivor(int index) {
    Bot& b = m_bots[index];
    const Vec3 pos = b.body.state().position;
    if (!alive(0)) {
        b.state = BotState::Idle;
        b.pathLength = 0;
        return;
    }
    int seen = perceive(index);
    if (seen >= 0 && distXZ(body(seen).state().position, pos) > m_config.zombie.survivorSight) seen = -1;
    botEquip(b, seen >= 0 ? distXZ(pos, body(seen).state().position) : 15.0f);
    if (seen >= 0) {
        if (b.state != BotState::Attack || b.target != seen) {
            b.reaction = m_config.reactionTime;
            b.stateTime = 0.0f;
            b.pathLength = 0;
        }
        b.state = BotState::Attack;
        b.target = seen;
        b.targetVisible = true;
        b.breakingWall = false;
        b.lastSeen = body(seen).state().position;
        b.lostTime = 0.0f;
        // Mantiene la distancia de su arma (no persigue lejos del jugador).
        if (b.strafeTimer <= 0.0f) {
            b.strafeSign = m_rng.chance(0.5f) ? 1.0f : -1.0f;
            b.strafeTimer = m_rng.range(0.8f, 1.8f);
        }
        return;
    }
    b.targetVisible = false;
    b.target = -1;
    // Sigue al jugador: un hueco detras de el (cada uno el suyo), sin pegarse.
    const CharacterState& player = body(0).state();
    const float d = distXZ(pos, player.position);
    const int k = index - kZombieSlots;
    const float angle = player.yaw + math3d::kPi + (static_cast<float>(k) - 2.0f) * 0.55f;
    Vec3 spot = player.position + Vec3{-sinf(angle) * 3.2f, 0.0f, -cosf(angle) * 3.2f};
    // En lo alto (la torre, el tejado), se quedan arriba con el jugador.
    b.perchTimer = player.position.y > 1.2f ? 1.0f : 0.0f;
    if (d > 6.0f || (b.state == BotState::Travel && b.pathIndex < b.pathLength)) {
        if (b.pathRefresh <= 0.0f || distXZ(b.destination, spot) > 3.0f || b.state != BotState::Travel) {
            Vec3 dest = spot;
            if (!NavGrid::clearWalk(*m_world, pos, spot, kBodyRadius)) m_nav->randomPointNear(spot, 2.0f, m_rng, &dest);
            b.destination = dest;
            if (requestPath(b, dest)) {
                b.state = BotState::Travel;
                b.stateTime = 0.0f;
                b.pathRefresh = 1.5f;
            } else if (NavGrid::clearWalk(*m_world, pos, dest, kBodyRadius)) {
                b.path[0] = dest;
                b.pathLength = 1;
                b.pathIndex = 0;
                b.state = BotState::Travel;
            }
        }
        if (d < 3.5f) b.pathLength = 0;
    } else {
        b.state = BotState::Idle;
        b.pathLength = 0;
        b.searchYaw = player.yaw;
    }
}

void Match::updateSurvivor(int index, float dt) {
    Bot& b = m_bots[index];
    if (b.state == BotState::Eliminated) return;
    const int self = index + 1;
    if (!b.rescued) {
        // Esperando en su campamento: quieto, mirando al jugador si se acerca.
        MoveIntent intent;
        if (alive(0) && distXZ(body(0).state().position, b.body.state().position) < 10.0f) {
            intent.faceYawOverride = true;
            intent.faceYaw = yawTowards(b.body.state().position, body(0).state().position);
        }
        b.aiming = false;
        b.body.step(intent, dt, *m_world);
        return;
    }
    if (m_config.cheats.freezeBots || m_config.cheats.partyBots) {
        b.aiming = false;
        b.body.step(MoveIntent{}, dt, *m_world);
        return;
    }
    const u64 t0 = core::nowMicros();
    b.stateTime += dt;
    b.weapon.update(dt);
    b.reaction -= dt;
    b.sidestepTimer -= dt;
    b.strafeTimer -= dt;
    b.pathRefresh -= dt;
    b.perchTimer -= dt;
    b.decisionTimer -= dt;
    if (b.decisionTimer <= 0.0f) {
        decideSurvivor(index);
        ++m_stats.decisions;
        b.decisionTimer += m_config.aiDecisionInterval;
        if (b.decisionTimer <= 0.0f) b.decisionTimer = m_config.aiDecisionInterval;
    }
    botUpdateHealing(index, dt);
    MoveIntent intent = steer(index, dt);
    if (b.state == BotState::Idle) {
        intent.faceYawOverride = true;
        intent.faceYaw = b.searchYaw;
    }
    const u64 t1 = core::nowMicros();
    b.body.step(intent, dt, *m_world);
    const u64 t2 = core::nowMicros();
    botShoot(index);
    (void)self;
    m_stats.aiMicros += (t1 - t0) + (core::nowMicros() - t2);
    m_stats.physicsMicros += t2 - t1;
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
            for (int i = 0; i < Inventory::kSlots; ++i) {
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

}  // namespace game
