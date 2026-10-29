// IA de los bots de game::Match: percepcion, decisiones (cada aiDecisionInterval), ejecucion
// del plan en cada paso y disparo. Fase 7: botin, caza de rivales, disparos que se oyen y
// paredes para cubrirse (ver docs/BOTS.md). v0.13: materiales propios (pican cuando les
// faltan), mejor botin y huida con poca vida para cubrirse y curarse.
#include "game/match.h"

#include "core/timer.h"
#include "game/match_internal.h"
#include "world/storm.h"
#include "world/water.h"

namespace game {

using math3d::Vec3;
using namespace tuning;
using namespace detail;

void Match::updateBot(int index, float dt) {
    Bot& b = m_bots[index];
    if (b.state == BotState::Eliminated) return;
    const int self = index + 1;
    // Modo zombi (v0.14): zombis y supervivientes tienen su propia IA (match_zombies.cpp).
    if (b.kind == BotKind::Zombie) {
        updateZombie(index, dt);
        return;
    }
    if (b.kind == BotKind::Survivor) {
        updateSurvivor(index, dt);
        return;
    }

    // Autobus y caida: salta en su momento y planea hacia donde quiere aterrizar.
    if (m_air[self] != AirState::Ground) {
        updateAirState(self, m_air[self] == AirState::Bus && m_time >= b.jumpTime);
        if (m_air[self] == AirState::Bus) return;
        const Vec3 pos = b.body.state().position;
        const Vec3 to = flatten(b.landingTarget - pos);
        const float d = math3d::length(to);
        const Vec3 wish = d > 0.5f ? to * (fminf(1.0f, d / 10.0f) / d) : Vec3{0.0f, 0.0f, 0.0f};
        const u64 t1 = core::nowMicros();
        b.body.step(airIntent(self, wish, false), dt, *m_world);
        m_stats.physicsMicros += core::nowMicros() - t1;
        updateAirState(self, false);
        if (m_air[self] == AirState::Ground) {
            b.state = BotState::Explore;
            b.decisionTimer = m_rng.range(0.05f, 0.3f);
            b.progressAnchor = b.body.state().position;
            b.progressTimer = 0.0f;
        }
        return;
    }

    // Trucos del menu de administrador: bots congelados (o de fiesta, bailando): quietos y
    // sin disparar; solo les afecta la gravedad.
    if (m_config.cheats.freezeBots || m_config.cheats.partyBots) {
        b.aiming = false;
        b.body.step(MoveIntent{}, dt, *m_world);
        return;
    }

    const u64 t0 = core::nowMicros();
    b.stateTime += dt;
    b.wallCooldown -= dt;
    b.meleeTimer -= dt;
    b.swingTimer -= dt;
    b.sinceHurt += dt;
    b.perchTimer -= dt;
    b.climbTimer -= dt;
    if (b.coverFrom >= 0) {
        // Huyendo, la pared es lo primero (siempre que tenga materiales). v0.14: si tiene
        // bastantes y el que dispara no esta encima, a veces una caja de cuatro paredes.
        if (alive(b.coverFrom)) {
            const Vec3 pos = b.body.state().position;
            const Vec3 from = body(b.coverFrom).state().position;
            int mats = 0;
            for (int r = 0; r < static_cast<int>(world::Resource::Count); ++r) mats += b.materials.amount[r];
            const bool urgent = b.state == BotState::Flee;
            const bool box = m_config.botsSmallBuilds && b.buildStep == 0 && b.boxTimer <= 0.0f &&
                             (mats >= 40 || m_config.botsFreeBuild) &&
                             (urgent || (distXZ(pos, from) > 8.0f && m_rng.chance(lateGame() ? 0.5f : 0.3f)));
            if (box) startSmallBuild(index, 1, yawTowards(pos, from));
            else botBuildCover(index, b.coverFrom, urgent);
        }
        b.coverFrom = -1;
    }
    updateSmallBuild(index, dt);
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
    botUpdateHealing(index, dt);
    botHarvest(index, dt);
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
        if (id == self || !alive(id) || m_air[id] != AirState::Ground || !hostile(self, id)) continue;
        if (id == 0 && m_config.cheats.invisible) continue;  // truco: el jugador no se ve
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
    // Un disparo que ha oido: ese destino ya esta puesto (hearShot).
    if (b.investigate) {
        b.investigate = false;
        return;
    }
    // A donde ir: dentro de la zona siguiente (la tormenta se cierra hacia ella).
    const Vec3 zone = m_storm.nextCenter();
    const float radius = fmaxf(fminf(m_storm.nextRadius(), 80.0f) * (m_storm.closes() ? 0.8f : 1.0f), 2.0f);
    Vec3 dest = zone;
    bool chosen = false;
    if (botNeedsSafety(b.body.state().position, -1)) {
        safePoint(&b.destination);
        return;
    }
    // Final de la partida (v0.14): mejor esperar a cubierto dentro de la zona siguiente
    // (junto a algo que tape) que correr a campo abierto.
    if (b.armed && lateGame() && m_rng.chance(0.7f)) {
        const float r = fmaxf(m_storm.nextRadius() * 0.5f, 2.0f);
        float bestScore = -1e9f;
        for (int attempt = 0; attempt < 6; ++attempt) {
            Vec3 p;
            if (!m_nav->randomPointNear(zone, r, m_rng, &p) || world::inWater(p.x, p.z)) continue;
            float score = -distXZ(p, zone) * 0.3f;
            if (m_harvestables) {
                for (size_t i = 0; i < m_harvestables->size(); ++i) {
                    if (m_objectDestroyed[i]) continue;
                    const world::Aabb& box = (*m_harvestables)[i].box;
                    if (box.max.y - box.min.y > 0.9f && distanceToBox(p + Vec3{0.0f, 1.0f, 0.0f}, box) < 2.5f) {
                        score += 6.0f;  // cobertura al lado
                        break;
                    }
                }
            }
            if (score > bestScore) {
                bestScore = score;
                dest = p;
                chosen = true;
            }
        }
        if (chosen) {
            b.destination = dest;
            b.holding = true;
            ++m_stats.botHolds;
            return;
        }
    }
    // Caza: a menudo va a por el rival mas cercano (sin verlo: como quien sigue el ruido de
    // la partida), en vez de pasear sin rumbo. Al final de la partida, menos.
    if (b.armed && m_rng.chance(lateGame() ? 0.2f : 0.45f)) {
        const Vec3 pos = b.body.state().position;
        float best = m_config.huntRange;
        for (int id = 0; id < entityCount(); ++id) {
            if (&body(id) == &b.body || !alive(id) || m_air[id] != AirState::Ground) continue;
            if (id == 0 && m_config.cheats.invisible) continue;
            const float d = distXZ(body(id).state().position, pos);
            if (d < best) {
                best = d;
                dest = body(id).state().position;
                chosen = true;
            }
        }
        if (chosen) {
            Vec3 near;
            if (m_nav->randomPointNear(dest, 6.0f, m_rng, &near)) dest = near;
            b.destination = dest;
            return;
        }
    }
    if (!m_pois.empty() && m_rng.chance(0.55f)) {
        for (int attempt = 0; attempt < 6 && !chosen; ++attempt) {
            const Vec3& poi = m_pois[m_rng.index(static_cast<int>(m_pois.size()))];
            if (distXZ(poi, zone) > radius + 5.0f) continue;
            chosen = m_nav->randomPointNear(poi, 5.0f, m_rng, &dest);
        }
    }
    if (!chosen) m_nav->randomPointNear(zone, radius, m_rng, &dest);
    b.destination = dest;
}

Vec3 Match::descentDirection(const Vec3& pos) const {
    // Se prueban 8 direcciones a distancias crecientes; la primera distancia a la que en
    // alguna el apoyo baja al menos 0.3 m decide (la del apoyo mas bajo). En un tejado
    // inclinado sale cuesta abajo; en uno plano, hacia el borde mas cercano.
    for (float d : {1.5f, 3.0f, 5.0f, 8.0f}) {
        float lowest = pos.y - 0.3f;
        Vec3 best{0.0f, 0.0f, 0.0f};
        for (int k = 0; k < 8; ++k) {
            const float a = k * (math3d::kPi * 0.25f);
            const Vec3 dir{cosf(a), 0.0f, sinf(a)};
            const Vec3 p = pos + dir * d;
            const float support = m_world->supportHeight(p.x, p.z, kFootRadius, pos.y + kStepHeight);
            if (support < lowest) {
                lowest = support;
                best = dir;
            }
        }
        if (math3d::length(best) > 0.5f) return best;
    }
    return {0.0f, 0.0f, 0.0f};
}

void Match::botUpdateHealing(int index, float dt) {
    // Consumibles de su inventario cuando no hay nadie a la vista: botiquin o vendas si le
    // falta vida, pociones de escudo si le falta escudo. Tardan lo mismo que al jugador y
    // ver a alguien los interrumpe (sin gastarlos).
    Bot& b = m_bots[index];
    CharacterState& s = b.body.state();
    const bool threatened = b.state == BotState::Attack || (b.target >= 0 && b.targetVisible);
    if (b.healTimer > 0.0f) {
        if (threatened || b.healSlot < 0) {
            b.healTimer = 0.0f;
            b.healSlot = -1;
            return;
        }
        b.healTimer -= dt;
        if (b.healTimer > 0.0f) return;
        const ItemType type = b.bag.slot(b.healSlot).type;
        const ItemDef& def = itemDef(type);
        if (def.health > 0.0f && s.health < def.healthCap) s.health = fminf(def.healthCap, s.health + def.health);
        if (def.shield > 0.0f && s.shield < def.shieldCap) s.shield = fminf(def.shieldCap, s.shield + def.shield);
        b.bag.consumeOne(b.healSlot);
        b.healSlot = -1;
        b.healTimer = 0.0f;
        return;
    }
    if (threatened || b.state == BotState::Loot || m_air[index + 1] != AirState::Ground) return;
    int best = -1;
    float bestGain = 0.0f;
    for (int i = 0; i < Inventory::kSlots; ++i) {
        const ItemType type = b.bag.slot(i).type;
        if (type == ItemType::None || isWeapon(type)) continue;
        const ItemDef& def = itemDef(type);
        const float health = def.health > 0.0f ? fminf(def.health, fmaxf(0.0f, def.healthCap - s.health)) : 0.0f;
        const float shield = def.shield > 0.0f ? fminf(def.shield, fmaxf(0.0f, def.shieldCap - s.shield)) : 0.0f;
        // Solo si se aprovecha bien (no una venda para curar 2 puntos); la vida, antes.
        const float gain = health * 1.5f + shield;
        if (gain >= fminf(def.health * 1.5f + def.shield, 20.0f) * 0.6f && gain > bestGain) {
            bestGain = gain;
            best = i;
        }
    }
    if (best < 0) return;
    b.healSlot = best;
    b.healTimer = useSecondsFor(b.bag.slot(best).type);
}

bool Match::botNeedsSafety(const Vec3& pos, int self) const {
    // Fuera de la zona siguiente cuando la tormenta ya se cierra o falta poco (o ya dentro
    // de ella): hay que entrar. Con margen, para no quedarse justo en el borde.
    if (!m_storm.closes()) return false;
    if (self >= 0 && m_inStorm[self]) return true;
    // Al final de la partida se rota antes (v0.14): se entra con tiempo y se busca sitio.
    const float early = lateGame() ? 45.0f : 25.0f;
    const bool soon = m_storm.shrinking() || (m_storm.timeLeft() >= 0.0f && m_storm.timeLeft() < early);
    return soon && distXZ(pos, m_storm.nextCenter()) > m_storm.nextRadius() - 2.0f;
}

bool Match::safePoint(Vec3* out) {
    const float r = fmaxf(m_storm.nextRadius() * 0.6f, 1.5f);
    if (m_nav->randomPointNear(m_storm.nextCenter(), r, m_rng, out)) return true;
    *out = m_storm.nextCenter();
    return false;
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
    const int self = index + 1;

    if (b.target >= 0 && (!alive(b.target) || m_air[b.target] != AirState::Ground)) {
        b.target = -1;
        b.targetVisible = false;
        if (b.state == BotState::Attack) {
            b.state = BotState::Search;
            b.stateTime = 0.0f;
            b.searchDuration = m_rng.range(1.5f, 3.0f);
            b.searchYaw = b.body.state().yaw;
        }
    }

    // Percepcion: rango + campo de vision + linea de vision (raycast). Sin arma solo se
    // pelea si el rival esta encima (pico); si no, sigue buscando botin.
    int seen = perceive(index);
    // Con inventario: el arma que mejor va a la distancia del rival (o la mejor).
    botEquip(b, seen >= 0 ? distXZ(pos, body(seen).state().position)
                          : (b.target >= 0 ? distXZ(pos, b.lastSeen) : 20.0f));
    if (seen >= 0 && !b.armed && distXZ(pos, body(seen).state().position) > 4.0f) seen = -1;

    // Materiales del suelo (de los caidos o de la llama) que pisa: se los queda.
    if (m_config.loot) {
        for (Pickup& p : m_pickups) {
            if (!p.active || p.kind != PickupKind::Materials || distXZ(p.position, pos) > 1.4f ||
                fabsf(p.position.y - pos.y) > 1.5f) {
                continue;
            }
            p.count -= b.materials.add(p.resource, p.count);
            if (p.count <= 0) p.active = false;
        }
    }

    // Con poca vida evita el combate: huye a donde no le vean, se cubre y se cura. Solo
    // pelea si el rival esta encima (acorralado).
    const float seenDistance = seen >= 0 ? distXZ(pos, body(seen).state().position) : 1e9f;
    const bool cornered = seen >= 0 && b.armed && seenDistance < m_config.botCorneredRange;
    if (m_config.botsFlee && b.state != BotState::Flee && botWeak(b) && !cornered) {
        const int threat = seen >= 0 ? seen : (b.sinceHurt < 3.0f ? b.threat : -1);
        if (threat >= 0 && alive(threat)) startFlee(index, threat);
    }
    if (b.state == BotState::Flee) {
        if (cornered) {
            b.state = BotState::Attack;  // no tiene escapatoria: pelea (abajo)
            b.target = -1;
        } else {
            // Huyendo no dispara: solo recuerda por donde anda el rival.
            if (seen >= 0) {
                b.threat = seen;
                b.lastSeen = body(seen).state().position;
            }
            b.target = b.threat;
            b.targetVisible = seen >= 0;
            seen = -1;
        }
    }
    if (seen >= 0) {
        if (b.state == BotState::Loot) {
            if (b.lootPickup >= 0) m_pickups[b.lootPickup].claimedBy = -1;
            if (b.lootContainer >= 0) m_containers[b.lootContainer].claimedBy = -1;
            b.lootPickup = b.lootContainer = -1;
        }
        b.harvestObject = -1;
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
        // Altura (v0.14): si el rival esta mas alto (tejado, rampa), o al final de la partida a
        // media distancia, una rampa hacia el para ganarla.
        if (m_config.botsSmallBuilds && b.armed && b.buildStep == 0 && b.perchTimer <= 0.0f &&
            b.body.state().grounded() && pos.y < 1.0f) {
            const float d = distXZ(pos, b.lastSeen);
            const float dy = b.lastSeen.y - pos.y;
            if ((dy > 1.6f && d > 5.0f && d < 28.0f && m_rng.chance(0.6f)) ||
                (lateGame() && d > 10.0f && d < 30.0f && m_rng.chance(0.1f))) {
                startSmallBuild(index, 2, yawTowards(pos, b.lastSeen));
            }
        }
    } else if (b.state == BotState::Attack) {
        b.targetVisible = false;
        b.lostTime += interval;
        // Objetivo escondido tras una pared construida cercana: disparar a la pared.
        b.breakingWall = false;
        if (b.target >= 0 && alive(b.target) && distXZ(pos, body(b.target).state().position) < botAttackRange(b)) {
            world::RayHit hit;
            const int wall = m_world->raycast(eyeOf(index + 1), chestOf(b.target), world::kBlocksShots, &hit)
                                 ? m_structures.wallFromCollider(hit.dynamicHandle)
                                 : -1;
            if (wall >= 0 && m_structures.wall(wall).owner != index + 1) {
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

    // Tormenta: si hay que entrar en la zona, se deja el botin o la busqueda y se va.
    if ((b.state == BotState::Loot || b.state == BotState::Search || b.state == BotState::Harvest || b.state == BotState::Camp ||
         ((b.state == BotState::Attack || b.state == BotState::Flee) && m_inStorm[self])) &&
        botNeedsSafety(pos, self)) {
        if (b.lootPickup >= 0) m_pickups[b.lootPickup].claimedBy = -1;
        if (b.lootContainer >= 0) m_containers[b.lootContainer].claimedBy = -1;
        b.lootPickup = b.lootContainer = -1;
        b.harvestObject = -1;
        b.campContainer = -1;
        b.state = BotState::Explore;
    }
    // Airdrops (v0.14): armado y con vida, a veces se aposta cerca y espera a quien vaya a
    // por el (hay un limite de bots por airdrop).
    if (m_config.botsCampAirdrops && b.armed && b.state != BotState::Attack && b.state != BotState::Loot &&
        b.state != BotState::Flee && b.state != BotState::Camp && !botNeedsSafety(pos, self) && m_rng.chance(0.35f) &&
        findCamp(index)) {
        b.state = BotState::Camp;
        b.stateTime = 0.0f;
    }
    // Botin: sin arma es lo primero; con arma, hasta tener un inventario completo (dos armas
    // y curas) lo busca a menudo (cofres, armas mejores); despues, de vez en cuando. Si no
    // hay botin y le faltan materiales, va a picar.
    if (b.state != BotState::Attack && b.state != BotState::Loot && b.state != BotState::Flee &&
        b.state != BotState::Harvest && b.state != BotState::Camp && !botNeedsSafety(pos, self)) {
        int weapons = 0;
        for (int i = 0; i < Inventory::kSlots; ++i) weapons += isWeapon(b.bag.slot(i).type) ? 1 : 0;
        const bool incomplete = weapons < 2 || !botHasHeals(b);
        const bool wantsLoot = !b.armed || m_rng.chance(incomplete ? 0.35f : 0.12f);
        int materials = 0;
        for (int r = 0; r < static_cast<int>(world::Resource::Count); ++r) materials += b.materials.amount[r];
        if (wantsLoot && findLoot(index, !b.armed ? 70.0f : (incomplete ? 32.0f : 22.0f))) {
            b.state = BotState::Loot;
            b.stateTime = 0.0f;
            b.pathRefresh = 0.0f;
            b.pathLength = 0;
        } else if (m_config.botsHarvest && b.armed && materials < m_config.botHarvestBelow && m_rng.chance(0.3f) &&
                   findHarvest(index)) {
            b.state = BotState::Harvest;
            b.stateTime = 0.0f;
        }
    }

    switch (b.state) {
        case BotState::Loot: {
            Vec3 goal = b.destination;
            bool valid = false;
            if (b.lootContainer >= 0) {
                const Container& c = m_containers[b.lootContainer];
                valid = !c.opened && c.landed;
                goal = c.position;
            } else if (b.lootPickup >= 0) {
                const Pickup& p = m_pickups[b.lootPickup];
                valid = p.active;
                goal = p.position;
            }
            if (!valid || b.stateTime > 25.0f) {
                if (b.lootPickup >= 0) m_pickups[b.lootPickup].claimedBy = -1;
                if (b.lootContainer >= 0) m_containers[b.lootContainer].claimedBy = -1;
                b.lootPickup = b.lootContainer = -1;
                b.state = BotState::Explore;
                break;
            }
            b.destination = goal;
            if (distXZ(pos, goal) < 1.8f && fabsf(goal.y - pos.y) < 1.6f) {
                botTakeLoot(index);
            } else if (b.pathRefresh <= 0.0f || b.pathIndex >= b.pathLength) {
                if (requestPath(b, goal)) b.pathRefresh = 3.0f;
            }
            break;
        }
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
                if (b.holding) {
                    // Ha llegado a su sitio del final de la partida: lo protege y espera.
                    b.holding = false;
                    b.searchDuration = m_rng.range(8.0f, 14.0f);
                    int mats = 0;
                    for (int r = 0; r < static_cast<int>(world::Resource::Count); ++r) mats += b.materials.amount[r];
                    if (m_config.botsSmallBuilds && mats >= 40 && b.buildStep == 0) {
                        const Vec3 toward = b.threat >= 0 && alive(b.threat) ? body(b.threat).state().position : m_storm.nextCenter();
                        startSmallBuild(index, 1, yawTowards(pos, toward));
                    }
                }
            } else if (distXZ(b.destination, m_storm.nextCenter()) > m_storm.nextRadius() + 6.0f) {
                b.state = BotState::Explore;  // la zona se ha cerrado: otro destino
            }
            break;
        case BotState::Search:
            if (b.stateTime > b.searchDuration) b.state = BotState::Explore;
            break;
        case BotState::Camp: {
            // Apostado junto a un airdrop: espera hasta que lo abran (y entonces sigue) o, si
            // nadie viene, va a por el (v0.14).
            const bool valid = b.campContainer >= 0 && b.campContainer < static_cast<int>(m_containers.size()) &&
                               !m_containers[b.campContainer].opened;
            if (!valid) {
                b.campContainer = -1;
                b.state = BotState::Explore;
                break;
            }
            const Container& c = m_containers[b.campContainer];
            if (c.landed && b.stateTime > 30.0f) {
                b.campContainer = -1;
                b.state = BotState::Explore;
                if (findLoot(index, 200.0f)) {
                    b.state = BotState::Loot;
                    b.stateTime = 0.0f;
                    b.pathRefresh = 0.0f;
                    b.pathLength = 0;
                }
                break;
            }
            if (b.pathIndex >= b.pathLength && distXZ(pos, b.destination) > 2.0f && b.pathRefresh <= 0.0f) {
                if (requestPath(b, b.destination)) b.pathRefresh = 3.0f;
            }
            break;
        }
        case BotState::Idle:
            break;
        case BotState::Harvest: {
            int total = 0;
            for (int r = 0; r < static_cast<int>(world::Resource::Count); ++r) total += b.materials.amount[r];
            if (total >= m_config.botHarvestGoal || b.stateTime > 30.0f || !m_harvestables) {
                b.harvestObject = -1;
                b.state = BotState::Explore;
                break;
            }
            if (b.harvestObject < 0 || m_objectDestroyed[b.harvestObject]) {
                // Se ha acabado (o lo ha roto otro): el siguiente objeto cercano.
                b.harvestObject = -1;
                if (!findHarvest(index)) b.state = BotState::Explore;
                break;
            }
            const world::Aabb& box = (*m_harvestables)[b.harvestObject].box;
            if (distanceToBox(chestOf(self), box) <= m_config.pickaxeReach - 0.2f) {
                b.pathLength = 0;  // ya llega: se queda picando (botHarvest)
            } else if (b.pathRefresh <= 0.0f || b.pathIndex >= b.pathLength) {
                if (requestPath(b, b.destination)) b.pathRefresh = 3.0f;
            }
            break;
        }
        case BotState::Flee: {
            // Recuperado (curado), o lleva mucho huyendo sin que nadie le vea: sigue con lo suyo.
            const float hp = b.body.state().health + b.body.state().shield;
            if (hp >= m_config.botRecoveredHealth || (b.stateTime > 25.0f && !b.targetVisible && b.healTimer <= 0.0f)) {
                b.state = BotState::Explore;
                b.threat = -1;
                b.target = -1;
                break;
            }
            // Sin curas y a salvo: a por ellas (cofres o curativos del suelo).
            if (!b.targetVisible && b.healTimer <= 0.0f && !botHasHeals(b) && findLoot(index, 35.0f, true)) {
                b.state = BotState::Loot;
                b.stateTime = 0.0f;
                b.pathRefresh = 0.0f;
                b.pathLength = 0;
                break;
            }
            // Le siguen viendo (o ya ha llegado y aun le ven): otro escondite.
            const bool arrived = b.pathIndex >= b.pathLength;
            if (b.pathRefresh <= 0.0f && (b.targetVisible || (arrived && b.healTimer <= 0.0f && b.stateTime < 2.0f))) {
                Vec3 dest;
                const Vec3 from = b.threat >= 0 && alive(b.threat) ? body(b.threat).state().position : b.lastSeen;
                if (fleePoint(b, from, &dest)) {
                    b.destination = dest;
                    if (requestPath(b, dest)) b.pathRefresh = 2.5f;
                }
            }
            break;
        }
        case BotState::Attack:
            if (b.strafeTimer <= 0.0f) {
                b.strafeSign = m_rng.chance(0.5f) ? 1.0f : -1.0f;
                b.strafeTimer = m_rng.range(0.8f, 1.8f);
            }
            if (b.targetVisible) {
                const float d = distXZ(pos, b.lastSeen);
                const float range = botAttackRange(b);
                if (d > range && b.pathRefresh <= 0.0f && !NavGrid::clearWalk(*m_world, pos, b.lastSeen, kBodyRadius)) {
                    requestPath(b, b.lastSeen);
                    b.pathRefresh = 2.0f;
                } else if (d <= range) {
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
                             (b.state == BotState::Travel || b.state == BotState::Attack || b.state == BotState::Loot ||
                              b.state == BotState::Flee || b.state == BotState::Harvest || b.state == BotState::Camp);
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
                if (b.state == BotState::Harvest || b.state == BotState::Camp) {
                    b.harvestObject = -1;  // no llega: a otra cosa
                    b.campContainer = -1;
                    b.state = BotState::Explore;
                }
            } else if (b.stuckCount == 2) {
                requestPath(b, b.state == BotState::Attack ? b.lastSeen : b.destination);
                if (b.state == BotState::Loot && b.stuckCount == 2) b.stateTime += 8.0f;  // se rinde antes
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
        case BotState::Loot:
            if (!followPath()) {
                const Vec3 to = flatten(b.destination - pos);
                const float d = math3d::length(to);
                if (d > 0.4f) desired = to * (1.0f / d);
            }
            speed = 0.95f;
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
            } else if (!b.armed) {
                // Sin arma: a por el, pico en mano.
                if (d > 1.2f) desired = to * (1.0f / d);
                speed = 1.0f;
            } else if (!b.targetVisible || d > botAttackRange(b)) {
                if (!followPath() && d > 1.5f) desired = to * (1.0f / d);
                speed = 0.9f;
            } else if (d > 0.01f) {
                // A distancia de combate: moverse de lado (y alejarse si esta muy cerca para
                // su arma; la escopeta busca la distancia corta).
                desired = Vec3{-to.z, 0.0f, to.x} * (b.strafeSign / d);
                if (d < botAttackRange(b) * 0.3f) desired += to * (-0.7f / d);
                speed = 0.55f;
            }
            break;
        }
        case BotState::Search:
            b.searchYaw = wrapAngle(b.searchYaw + 1.4f * dt);
            intent.faceYawOverride = true;
            intent.faceYaw = b.searchYaw;
            break;
        case BotState::Harvest:
            if (b.harvestObject >= 0 && m_harvestables) {
                const world::Aabb& box = (*m_harvestables)[b.harvestObject].box;
                if (distanceToBox(chestOf(self), box) <= m_config.pickaxeReach - 0.2f) {
                    // Al alcance: quieto y de cara al objeto.
                    intent.faceYawOverride = true;
                    intent.faceYaw = yawTowards(pos, (box.min + box.max) * 0.5f);
                } else if (!followPath()) {
                    const Vec3 to = flatten(b.destination - pos);
                    const float d = math3d::length(to);
                    if (d > 0.3f) desired = to * (1.0f / d);
                }
            }
            speed = 0.9f;
            break;
        case BotState::Flee:
            // Corre por el camino al escondite; alli espera (y se cura).
            followPath();
            speed = 1.0f;
            break;
        case BotState::Camp:
            // Va a su sitio; alli, quieto y con el arma lista, vigila el airdrop.
            if (!followPath() && b.campContainer >= 0 && b.campContainer < static_cast<int>(m_containers.size())) {
                const Vec3 drop = m_containers[b.campContainer].position;
                intent.faceYawOverride = true;
                intent.faceYaw = yawTowards(pos, drop) + 0.45f * sinf(b.stateTime * 0.7f);
                b.aiming = true;
            }
            speed = 0.85f;
            break;
        case BotState::Idle:
        case BotState::Explore:
        case BotState::Eliminated:
            break;
    }

    // Curandose: quieto hasta terminar (o hasta ver a alguien).
    if (b.healTimer > 0.0f) desired = {0.0f, 0.0f, 0.0f};
    // Subiendo por su rampa (v0.14): recto hacia arriba, sin esquivar los escalones.
    if (b.climbTimer > 0.0f) {
        intent.wish = forwardFromYaw(b.buildYaw);
        intent.speedScale = 1.0f;
        intent.faceYawOverride = false;
        return intent;
    }

    // Encima de algo (tejado de una nave, contenedor, altillo) tras caer del autobus: la
    // navegacion solo llega al suelo, asi que primero hay que bajar. No si se ha subido a
    // proposito (su rampa para ganar altura, o siguiendo al jugador en lo alto).
    if (s.grounded() && pos.y > 1.2f && b.perchTimer <= 0.0f) {
        b.descentTimer -= dt;
        if (b.descentTimer <= 0.0f) {
            b.descent = descentDirection(pos);
            b.descentTimer = 0.5f;
        }
        if (math3d::length(b.descent) > 0.5f) {
            desired = b.descent;
            speed = 0.9f;
        }
    } else {
        b.descentTimer = 0.0f;
    }

    return avoidObstacles(index, desired, speed, intent);
}

MoveIntent Match::avoidObstacles(int index, Vec3 desired, float speed, MoveIntent intent) {
    // Comun a todas las IA: apartarse de lado si esta atascado, no amontonarse con otros
    // personajes y esquivar el obstaculo que tenga justo delante.
    Bot& b = m_bots[index];
    const int self = index + 1;
    const CharacterState& s = b.body.state();
    const Vec3 pos = s.position;
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
    const Vec3 eye = eyeOf(self);

    // Sin arma: golpes de pico si el objetivo esta al alcance.
    if (!b.armed) {
        const Vec3 targetPos = body(b.target).state().position;
        if (b.meleeTimer > 0.0f || distXZ(s.position, targetPos) > m_config.pickaxeReach + 0.4f) return;
        if (fabsf(wrapAngle(s.yaw - yawTowards(s.position, targetPos))) > 0.6f) return;
        b.meleeTimer = m_config.pickaxeInterval + 0.15f;
        pushEvent(GameEventType::HitEntity, b.target, self, chestOf(b.target));
        m_damageSource = -1;
        applyDamage(b.target, m_config.pickaxeDamage * m_config.botDamageScale, self);
        return;
    }

    const WeaponConfig& weapon = b.weapon.config();
    const int type = b.weaponType == ItemType::None ? -1 : static_cast<int>(b.weaponType);
    // Los supervivientes del modo zombi pegan algo mas que los bots del battle royale.
    const float scale = b.kind == BotKind::Survivor ? m_config.zombie.survivorDamageScale : m_config.botDamageScale;
    if (!b.targetVisible) {
        if (!b.breakingWall) return;
        // Disparo a la pared que tapa al objetivo.
        if (fabsf(wrapAngle(s.yaw - yawTowards(s.position, b.wallPoint))) > 0.45f) return;
        if (!b.weapon.tryFire()) return;
        fireWeapon(self, eye, math3d::normalize(b.wallPoint - eye), weapon, m_config.botSpreadDegrees, scale, type);
        b.weapon.addCooldown(weapon.fireInterval * m_rng.range(-0.15f, 0.25f));
        return;
    }
    const Vec3 targetPos = body(b.target).state().position;
    // Solo dispara si ya esta encarado (no dispara de espaldas).
    if (fabsf(wrapAngle(s.yaw - yawTowards(s.position, targetPos))) > 0.45f) return;

    const Vec3 aim = chestOf(b.target);
    const float d = math3d::length(aim - eye);
    if (d > weapon.range) return;
    // El arma decide si puede disparar (cadencia, municion, recarga).
    if (!b.weapon.tryFire()) return;
    float spreadDeg = m_config.botSpreadDegrees + m_config.botSpreadPerMeter * d + weapon.hipSpread * 0.4f;
    if (body(b.target).state().horizontalSpeed() > 2.0f) spreadDeg += 1.5f;
    if (b.target == 0 && m_playerCrouched) spreadDeg += m_config.crouchedSpreadPenalty;  // agachado: mas dificil
    if (s.horizontalSpeed() > 1.0f) spreadDeg += 1.0f;
    fireWeapon(self, eye, math3d::normalize(aim - eye), weapon, spreadDeg, scale, type);
    b.weapon.addCooldown(weapon.fireInterval * m_rng.range(-0.15f, 0.25f));  // cadencia algo irregular
}

// --- IA de la fase 7: botin, oido, cobertura -------------------------------------------------

float Match::botAttackRange(const Bot& b) const {
    if (!b.armed) return 1.8f;
    switch (weaponClass(b.weaponType)) {
        case WeaponClass::Shotgun: return 7.0f;
        case WeaponClass::Smg: return 15.0f;
        case WeaponClass::Pistol: return 14.0f;
        case WeaponClass::Sniper: return 38.0f;
        case WeaponClass::Launcher: return 24.0f;  // y nunca a menos de 8 m (botEquip)
        default: return m_config.attackRange;
    }
}

bool Match::findLoot(int index, float maxDistance, bool healsOnly) {
    // Sin arma: el cofre o el arma mas cercana. Con arma: solo un arma mejor que la suya o
    // un airdrop ya en el suelo (a mas distancia: merece la pena). `healsOnly`: solo curas
    // (curativos del suelo, cofres y la llama), para un bot herido que no lleva.
    Bot& b = m_bots[index];
    const int self = index + 1;
    const Vec3 pos = b.body.state().position;
    float best = 1e9f;
    int pickup = -1, container = -1;
    for (size_t i = 0; i < m_containers.size(); ++i) {
        const Container& c = m_containers[i];
        if (c.opened || !c.landed || c.kind == ContainerKind::AmmoBox || c.kind == ContainerKind::Workbench) continue;
        if (c.claimedBy >= 0 && c.claimedBy != self) continue;
        if (healsOnly && c.kind == ContainerKind::Airdrop) continue;
        // El airdrop y la llama merecen la pena aunque ya vaya armado.
        const bool special = c.kind == ContainerKind::Airdrop || c.kind == ContainerKind::Llama;
        if (b.armed && !special && b.bag.full()) continue;
        const float d = distXZ(c.position, pos);
        const float limit = special ? maxDistance * 3.0f : maxDistance;
        if (d > limit || stormDepthAt(c.position.x, c.position.z) > -4.0f) continue;
        const float score = special ? d * 0.4f : d;
        if (score < best) {
            best = score;
            container = static_cast<int>(i);
            pickup = -1;
        }
    }
    for (size_t i = 0; i < m_pickups.size(); ++i) {
        const Pickup& p = m_pickups[i];
        if (!p.active || p.kind == PickupKind::Ammo) continue;
        if (p.claimedBy >= 0 && p.claimedBy != self) continue;
        if (healsOnly && (p.kind != PickupKind::Item || isWeapon(p.item) || itemDef(p.item).kind != ItemKind::Consumable)) continue;
        // Sin arma, primero un arma; despues, lo que le sirva (otra arma, una mejor, curas o
        // materiales si le faltan).
        if (!b.armed && (p.kind != PickupKind::Item || !isWeapon(p.item))) continue;
        if (!botWants(b, p)) continue;
        const float d = distXZ(p.position, pos);
        if (d > maxDistance || d >= best || fabsf(p.position.y - pos.y) > 3.0f) continue;
        best = d;
        pickup = static_cast<int>(i);
        container = -1;
    }
    if (pickup < 0 && container < 0) return false;
    if (b.lootPickup >= 0) m_pickups[b.lootPickup].claimedBy = -1;
    if (b.lootContainer >= 0) m_containers[b.lootContainer].claimedBy = -1;
    b.lootPickup = pickup;
    b.lootContainer = container;
    if (pickup >= 0) {
        m_pickups[pickup].claimedBy = self;
        b.destination = m_pickups[pickup].position;
    } else {
        m_containers[container].claimedBy = self;
        b.destination = m_containers[container].position;
    }
    return true;
}

void Match::botTakeLoot(int index) {
    Bot& b = m_bots[index];
    const int self = index + 1;
    if (b.lootContainer >= 0) {
        Interactable it;
        it.kind = Interactable::Kind::Container;
        it.index = b.lootContainer;
        interact(self, it);
    } else if (b.lootPickup >= 0) {
        pickUp(self, b.lootPickup);
        if (b.lootPickup >= 0) m_pickups[b.lootPickup].claimedBy = -1;
    }
    b.lootPickup = b.lootContainer = -1;
    // Tras abrir un cofre, lo que ha soltado esta justo al lado: a por ello.
    b.state = b.armed ? BotState::Explore : BotState::Search;
    b.stateTime = 0.0f;
    b.searchDuration = 0.3f;
    b.searchYaw = b.body.state().yaw;
    if (findLoot(index, 6.0f)) b.state = BotState::Loot;
}

void Match::botBuildCover(int index, int attacker, bool urgent) {
    // Como un jugador: al recibir disparos desde lejos, a veces se cubre con una pared hacia
    // el atacante, gastando sus materiales (v0.13; hay un limite de paredes por bot y en
    // total). Huyendo (`urgent`) siempre que pueda, aunque el rival este cerca.
    if (!m_config.botsBuild || index < 0) return;
    Bot& b = m_bots[index];
    const int self = index + 1;
    if (m_air[self] != AirState::Ground || !b.body.state().grounded()) return;
    if (urgent ? b.wallCooldown > m_config.botWallCooldown - 1.5f : b.wallCooldown > 0.0f) return;
    world::Resource material = world::Resource::Wood;
    if (!m_config.botsFreeBuild && !botPickMaterial(b, &material)) return;  // sin materiales
    const Vec3 pos = b.body.state().position;
    const Vec3 from = body(attacker).state().position;
    if (!urgent && distXZ(pos, from) < 5.0f) return;
    if (!urgent && !m_rng.chance(0.6f)) {
        b.wallCooldown = 2.0f;
        return;
    }
    world::Aabb others[kMaxBots + 1];
    int n = 0;
    for (int id = 0; id < entityCount(); ++id) {
        if (id != self && alive(id) && m_air[id] == AirState::Ground) others[n++] = bodyBounds(id);
    }
    WallPlacement plan = m_structures.plan(pos, yawTowards(pos, from), bodyBounds(self), others, n, self);
    plan.material = material;
    b.wallCooldown = m_config.botWallCooldown + m_rng.range(0.0f, 3.0f);
    if (!plan.valid) return;
    const int wall = m_structures.place(plan, self);
    if (wall >= 0) {
        if (!m_config.botsFreeBuild) b.materials.spend(material, m_config.build.wallCost);
        ++m_stats.wallsBuilt;
        const world::Aabb& box = m_structures.wall(wall).box;
        pushEvent(GameEventType::WallBuilt, self, -1, (box.min + box.max) * 0.5f);
    }
}

void Match::hearShot(int shooter, const Vec3& origin) {
    // Los bots armados que no estan peleando ni buscando botin van a mirar de donde vienen
    // los disparos que oyen. Los zombis (v0.14) tambien acuden al ruido.
    for (int i = 0; i < static_cast<int>(m_bots.size()); ++i) {
        Bot& b = m_bots[i];
        const int self = i + 1;
        if (b.kind == BotKind::Zombie) {
            if (b.state == BotState::Eliminated || b.state == BotState::Attack || isZombie(shooter)) continue;
            if (shooter == 0 && m_config.cheats.invisible) continue;
            const float range = m_config.zombie.hearingRange * (m_night ? 1.4f : 1.0f);
            if (distXZ(b.body.state().position, origin) > range) continue;
            b.investigate = true;
            b.destination = origin + Vec3{m_rng.range(-4.0f, 4.0f), 0.0f, m_rng.range(-4.0f, 4.0f)};
            if (b.decisionTimer > 0.15f) b.decisionTimer = 0.15f;
            continue;
        }
        if (b.kind == BotKind::Survivor) continue;
        if (self == shooter || b.state == BotState::Eliminated || b.state == BotState::Attack ||
            b.state == BotState::Loot || b.state == BotState::Flee || b.state == BotState::Camp || !b.armed ||
            m_air[self] != AirState::Ground) {
            continue;
        }
        if (shooter == 0 && m_config.cheats.invisible) continue;  // truco: no se le oye
        if (distXZ(b.body.state().position, origin) > m_config.hearingRange) continue;
        b.investigate = true;
        b.destination = origin + Vec3{m_rng.range(-5.0f, 5.0f), 0.0f, m_rng.range(-5.0f, 5.0f)};
        b.state = BotState::Explore;
        b.pathLength = 0;
        if (b.decisionTimer > 0.15f) b.decisionTimer = 0.15f;
    }
}

// --- IA v0.13: materiales, picar y huir con poca vida ----------------------------------------

bool Match::botWeak(const Bot& b) const {
    // Al final de la partida (v0.14) se arriesga menos: huye antes.
    const CharacterState& s = b.body.state();
    return s.health + s.shield < m_config.botFleeHealth + (lateGame() ? 15.0f : 0.0f);
}

bool Match::botHasHeals(const Bot& b) const {
    for (int i = 0; i < Inventory::kSlots; ++i) {
        const ItemDef& def = itemDef(b.bag.slot(i).type);
        if (def.kind == ItemKind::Consumable && (def.health > 0.0f || def.shield > 0.0f)) return true;
    }
    return false;
}

bool Match::botPickMaterial(const Bot& b, world::Resource* out) const {
    int best = -1, most = m_config.build.wallCost - 1;
    for (int r = 0; r < static_cast<int>(world::Resource::Count); ++r) {
        if (b.materials.amount[r] > most) {
            most = b.materials.amount[r];
            best = r;
        }
    }
    if (best < 0) return false;
    *out = static_cast<world::Resource>(best);
    return true;
}

bool Match::findHarvest(int index) {
    // El objeto del mapa mas cercano que se pueda picar desde el suelo: arboles, rocas, cajas,
    // contenedores, vallas... (no tejados ni los paneles altos de las naves), lejos de la
    // tormenta.
    if (!m_harvestables || !m_config.botsHarvest) return false;
    Bot& b = m_bots[index];
    const Vec3 pos = b.body.state().position;
    float best = m_config.botHarvestRange;
    int chosen = -1;
    for (size_t i = 0; i < m_harvestables->size(); ++i) {
        if (m_objectDestroyed[i]) continue;
        const world::Aabb& box = (*m_harvestables)[i].box;
        if (box.min.y > pos.y + 0.6f || box.min.y < pos.y - 1.0f || box.max.y - box.min.y > 5.0f) continue;
        const Vec3 center = (box.min + box.max) * 0.5f;
        const float d = distXZ(center, pos);
        if (d >= best || stormDepthAt(center.x, center.z) > -4.0f) continue;
        best = d;
        chosen = static_cast<int>(i);
    }
    if (chosen < 0) return false;
    // Se pica desde el lado mas cercano: un punto transitable junto a la caja.
    const world::Aabb& box = (*m_harvestables)[chosen].box;
    Vec3 side = closestOnBox(pos, box);
    const Vec3 out = flatten(pos - side);
    const float len = math3d::length(out);
    side = side + (len > 0.01f ? out * (0.8f / len) : Vec3{0.8f, 0.0f, 0.0f});
    side.y = pos.y;
    Vec3 dest = side;
    if (!NavGrid::clearWalk(*m_world, pos, side, kBodyRadius)) m_nav->randomPointNear(side, 1.5f, m_rng, &dest);
    b.harvestObject = chosen;
    b.destination = dest;
    b.pathLength = 0;
    b.pathRefresh = 0.0f;
    b.stateTime = 0.0f;
    return true;
}

void Match::botHarvest(int index, float dt) {
    // Picando: un golpe cada pickaxeInterval (como el jugador) mientras el objeto este al
    // alcance. Cada golpe le da material y quita vida al objeto.
    (void)dt;
    Bot& b = m_bots[index];
    const int self = index + 1;
    if (b.state != BotState::Harvest || b.harvestObject < 0 || !m_harvestables || b.swingTimer > 0.0f) return;
    if (m_objectDestroyed[b.harvestObject] || b.healTimer > 0.0f) return;
    const world::Harvestable& h = (*m_harvestables)[b.harvestObject];
    const Vec3 chest = chestOf(self);
    if (distanceToBox(chest, h.box) > m_config.pickaxeReach) return;
    const Vec3 center = (h.box.min + h.box.max) * 0.5f;
    if (fabsf(wrapAngle(b.body.state().yaw - yawTowards(b.body.state().position, center))) > 0.6f) return;
    b.swingTimer = m_config.pickaxeInterval + 0.1f;
    b.materials.add(h.resource, h.perHit);
    ++m_stats.botHarvestHits;
    const Vec3 point = closestOnBox(chest, h.box);
    pushEvent(GameEventType::PickaxeSwing, self, 1, point);
    damageObject(b.harvestObject, m_config.pickaxeObjectDamage, point, self);
}

void Match::startFlee(int index, int threat) {
    Bot& b = m_bots[index];
    if (b.lootPickup >= 0) m_pickups[b.lootPickup].claimedBy = -1;
    if (b.lootContainer >= 0) m_containers[b.lootContainer].claimedBy = -1;
    b.lootPickup = b.lootContainer = -1;
    b.harvestObject = -1;
    b.state = BotState::Flee;
    b.stateTime = 0.0f;
    ++m_stats.botFlees;
    b.threat = threat;
    b.target = threat;
    b.targetVisible = false;
    b.breakingWall = false;
    b.lastSeen = body(threat).state().position;
    b.pathLength = 0;
    b.pathRefresh = 0.0f;
    // Lo primero, una pared entre los dos (en su siguiente paso, si tiene materiales).
    b.coverFrom = threat;
    Vec3 dest;
    if (fleePoint(b, b.lastSeen, &dest)) {
        b.destination = dest;
        if (requestPath(b, dest)) b.pathRefresh = 2.5f;
    }
}

bool Match::fleePoint(const Bot& b, const Vec3& threat, Vec3* out) {
    // Un abanico de puntos a ~16 m alejandose del rival: gana el mas lejano, sobre todo si
    // el rival no lo ve (detras de una nave, un contenedor...), y nunca hacia la tormenta.
    const Vec3 pos = b.body.state().position;
    Vec3 away = flatten(pos - threat);
    const float len = math3d::length(away);
    away = len > 0.1f ? away * (1.0f / len) : forwardFromYaw(b.body.state().yaw + math3d::kPi);
    const Vec3 eye = threat + Vec3{0.0f, kEyeHeight, 0.0f};
    float bestScore = -1e9f;
    bool found = false;
    for (int k = 0; k < 6; ++k) {
        const Vec3 dir = rotateY(away, (static_cast<float>(k) - 2.5f) * 0.45f);
        Vec3 p;
        if (!m_nav->randomPointNear(pos + dir * 16.0f, 5.0f, m_rng, &p)) continue;
        float score = distXZ(p, threat);
        if (!lineOfSight(eye, p + Vec3{0.0f, kChestHeight, 0.0f})) score += 25.0f;  // a cubierto
        if (stormDepthAt(p.x, p.z) > -3.0f) score -= 80.0f;
        else if (botNeedsSafety(p, -1)) score -= 30.0f;
        if (score > bestScore) {
            bestScore = score;
            *out = p;
            found = true;
        }
    }
    return found;
}

// --- IA v0.14: airdrops, estructuras pequenas y final de partida ------------------------------

bool Match::lateGame() const {
    // Quedan pocos, o la tormenta ya va por sus ultimos cierres.
    // Solo en partidas que eran grandes (con un par de bots no hay "final").
    if (zombieMode() || (1 + botCount()) < 2 * m_config.lateGameAlive) return false;
    return m_alive <= m_config.lateGameAlive || (m_storm.closes() && m_storm.phase() >= 5);
}

bool Match::findCamp(int index) {
    Bot& b = m_bots[index];
    const int self = index + 1;
    const CharacterState& s = b.body.state();
    if (s.health + s.shield < 60.0f) return false;
    const Vec3 pos = s.position;
    // El airdrop sin abrir mas cercano (cayendo o ya en el suelo) que no tenga ya bastantes
    // bots apostados.
    int drop = -1;
    float best = m_config.botCampRange;
    for (size_t i = 0; i < m_containers.size(); ++i) {
        const Container& c = m_containers[i];
        if (c.kind != ContainerKind::Airdrop || c.opened) continue;
        const float d = distXZ(c.position, pos);
        if (d >= best || stormDepthAt(c.position.x, c.position.z) > -6.0f) continue;
        int campers = 0;
        for (const Bot& o : m_bots) campers += o.state == BotState::Camp && o.campContainer == static_cast<int>(i) ? 1 : 0;
        if (campers >= m_config.botCampers) continue;
        best = d;
        drop = static_cast<int>(i);
    }
    if (drop < 0) return false;
    const Vec3 c = m_containers[drop].position;
    const float ground = m_world->supportHeight(c.x, c.z, 0.6f, 200.0f);
    const Vec3 target{c.x, ground + 0.8f, c.z};
    // Un sitio a 10-14 m desde el que se vea el airdrop, por su lado y mejor junto a algo que
    // le cubra.
    Vec3 side = flatten(pos - c);
    const float len = math3d::length(side);
    side = len > 0.1f ? side * (1.0f / len) : forwardFromYaw(s.yaw);
    float bestScore = -1e9f;
    Vec3 spot;
    bool found = false;
    for (int k = 0; k < 6; ++k) {
        const Vec3 dir = rotateY(side, (static_cast<float>(k) - 2.5f) * 0.5f);
        Vec3 p;
        if (!m_nav->randomPointNear(c + dir * m_rng.range(10.0f, 14.0f), 3.0f, m_rng, &p)) continue;
        if (world::inWater(p.x, p.z) || stormDepthAt(p.x, p.z) > -4.0f) continue;
        if (!lineOfSight(p + Vec3{0.0f, kEyeHeight, 0.0f}, target)) continue;
        float score = -distXZ(p, pos) * 0.2f;
        if (m_harvestables) {
            for (size_t i = 0; i < m_harvestables->size(); ++i) {
                if (m_objectDestroyed[i]) continue;
                const world::Aabb& box = (*m_harvestables)[i].box;
                if (box.max.y - box.min.y > 0.9f && distanceToBox(p + Vec3{0.0f, 1.0f, 0.0f}, box) < 2.5f) {
                    score += 8.0f;
                    break;
                }
            }
        }
        if (score > bestScore) {
            bestScore = score;
            spot = p;
            found = true;
        }
    }
    if (!found) return false;
    b.campContainer = drop;
    b.destination = spot;
    b.pathLength = 0;
    b.pathRefresh = 0.0f;
    if (requestPath(b, spot)) b.pathRefresh = 3.0f;
    ++m_stats.botCamps;
    pushEvent(GameEventType::BotCamping, self, drop, spot);
    return true;
}

void Match::startSmallBuild(int index, int kind, float yaw) {
    Bot& b = m_bots[index];
    if (!m_config.botsSmallBuilds || !m_config.botsBuild) return;
    b.buildKind = kind;
    b.buildYaw = yaw;
    b.buildStep = kind == 1 ? 4 : 1;
    b.buildTimer = 0.0f;
    if (kind == 1) ++m_stats.botBoxes;
}

bool Match::botPlacePiece(int index, PieceType type, float yaw) {
    Bot& b = m_bots[index];
    const int self = index + 1;
    world::Resource material = world::Resource::Wood;
    if (!m_config.botsFreeBuild && !botPickMaterial(b, &material)) return false;
    world::Aabb others[kMaxBots + 1];
    int n = 0;
    for (int id = 0; id < entityCount(); ++id) {
        if (id != self && alive(id) && m_air[id] == AirState::Ground) others[n++] = bodyBounds(id);
    }
    WallPlacement plan = m_structures.plan(b.body.state().position, yaw, bodyBounds(self), others, n, self, type);
    plan.material = material;
    if (!plan.valid) return false;
    const int wall = m_structures.place(plan, self);
    if (wall < 0) return false;
    if (!m_config.botsFreeBuild) b.materials.spend(material, m_config.build.wallCost);
    ++m_stats.wallsBuilt;
    const world::Aabb& box = m_structures.wall(wall).box;
    pushEvent(GameEventType::WallBuilt, self, -1, (box.min + box.max) * 0.5f);
    return true;
}

void Match::updateSmallBuild(int index, float dt) {
    // Una pieza cada 0.2 s: la caja (hacia la amenaza, los lados y detras) o la rampa (y
    // sube por ella). Al cabo de un rato quita las paredes de su caja para salir.
    Bot& b = m_bots[index];
    const int self = index + 1;
    if (b.boxTimer > 0.0f) {
        b.boxTimer -= dt;
        const bool busy = b.healTimer > 0.0f || (b.state == BotState::Attack && b.targetVisible);
        if (b.boxTimer <= 0.0f && busy) b.boxTimer = 1.0f;
        if (b.boxTimer <= 0.0f) {
            const Vec3 pos = b.body.state().position;
            for (int w = 0; w < m_structures.capacity(); ++w) {
                const BuiltWall& wall = m_structures.wall(w);
                if (!wall.active || wall.owner != self || wall.type != PieceType::Wall) continue;
                if (distanceToBox(pos + Vec3{0.0f, 1.0f, 0.0f}, wall.box) < 3.5f) {
                    const Vec3 center = (wall.box.min + wall.box.max) * 0.5f;
                    if (m_structures.remove(w)) pushEvent(GameEventType::WallRemoved, self, -1, center);
                }
            }
        }
    }
    if (b.buildStep <= 0) return;
    b.buildTimer -= dt;
    if (b.buildTimer > 0.0f || !b.body.state().grounded() || m_air[self] != AirState::Ground) return;
    b.buildTimer = 0.2f;
    if (b.buildKind == 1) {
        const float offsets[4] = {0.0f, 0.5f * math3d::kPi, -0.5f * math3d::kPi, math3d::kPi};
        botPlacePiece(index, PieceType::Wall, b.buildYaw + offsets[4 - b.buildStep]);
        if (--b.buildStep == 0) b.boxTimer = m_rng.range(5.0f, 9.0f);
        return;
    }
    // Rampa: si se ha puesto, sube por ella y se queda arriba un rato.
    b.buildStep = 0;
    if (botPlacePiece(index, PieceType::Ramp, b.buildYaw)) {
        ++m_stats.botRamps;
        b.climbTimer = 1.3f;
        b.perchTimer = 7.0f;
        b.pathLength = 0;
    }
}

}  // namespace game
