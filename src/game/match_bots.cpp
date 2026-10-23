// IA de los bots de game::Match: percepcion, decisiones (cada aiDecisionInterval), ejecucion
// del plan en cada paso y disparo. Fase 7: botin, caza de rivales, disparos que se oyen y
// paredes para cubrirse (ver docs/BOTS.md).
#include "game/match.h"

#include "core/timer.h"
#include "game/match_internal.h"
#include "world/storm.h"

namespace game {

using math3d::Vec3;
using namespace tuning;
using namespace detail;

void Match::updateBot(int index, float dt) {
    Bot& b = m_bots[index];
    if (b.state == BotState::Eliminated) return;
    const int self = index + 1;

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

    const u64 t0 = core::nowMicros();
    b.stateTime += dt;
    b.wallCooldown -= dt;
    b.meleeTimer -= dt;
    if (b.coverFrom >= 0) {
        if (alive(b.coverFrom)) botBuildCover(index, b.coverFrom);
        b.coverFrom = -1;
    }
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
        if (id == self || !alive(id) || m_air[id] != AirState::Ground) continue;
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
    // Caza: a menudo va a por el rival mas cercano (sin verlo: como quien sigue el ruido de
    // la partida), en vez de pasear sin rumbo.
    if (b.armed && m_rng.chance(0.45f)) {
        const Vec3 pos = b.body.state().position;
        float best = m_config.huntRange;
        for (int id = 0; id < entityCount(); ++id) {
            if (&body(id) == &b.body || !alive(id) || m_air[id] != AirState::Ground) continue;
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
    b.healTimer = fmaxf(itemDef(b.bag.slot(best).type).useSeconds, 0.05f);
}

bool Match::botNeedsSafety(const Vec3& pos, int self) const {
    // Fuera de la zona siguiente cuando la tormenta ya se cierra o falta poco (o ya dentro
    // de ella): hay que entrar. Con margen, para no quedarse justo en el borde.
    if (!m_storm.closes()) return false;
    if (self >= 0 && m_inStorm[self]) return true;
    const bool soon = m_storm.shrinking() || (m_storm.timeLeft() >= 0.0f && m_storm.timeLeft() < 25.0f);
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
    if (seen >= 0) {
        if (b.state == BotState::Loot) {
            if (b.lootPickup >= 0) m_pickups[b.lootPickup].claimedBy = -1;
            if (b.lootContainer >= 0) m_containers[b.lootContainer].claimedBy = -1;
            b.lootPickup = b.lootContainer = -1;
        }
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
        if (b.target >= 0 && alive(b.target) && distXZ(pos, body(b.target).state().position) < botAttackRange(b)) {
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

    // Tormenta: si hay que entrar en la zona, se deja el botin o la busqueda y se va.
    if ((b.state == BotState::Loot || b.state == BotState::Search ||
         (b.state == BotState::Attack && m_inStorm[self])) &&
        botNeedsSafety(pos, self)) {
        if (b.lootPickup >= 0) m_pickups[b.lootPickup].claimedBy = -1;
        if (b.lootContainer >= 0) m_containers[b.lootContainer].claimedBy = -1;
        b.lootPickup = b.lootContainer = -1;
        b.state = BotState::Explore;
    }
    // Botin: sin arma es lo primero; con arma, de vez en cuando una mejor o un airdrop.
    if (b.state != BotState::Attack && b.state != BotState::Loot && !botNeedsSafety(pos, self)) {
        const bool wantsLoot = !b.armed || m_rng.chance(0.12f);
        if (wantsLoot && findLoot(index, b.armed ? 22.0f : 70.0f)) {
            b.state = BotState::Loot;
            b.stateTime = 0.0f;
            b.pathRefresh = 0.0f;
            b.pathLength = 0;
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
            } else if (distXZ(b.destination, m_storm.nextCenter()) > m_storm.nextRadius() + 6.0f) {
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
                             (b.state == BotState::Travel || b.state == BotState::Attack || b.state == BotState::Loot);
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
        case BotState::Explore:
        case BotState::Eliminated:
            break;
    }

    // Curandose: quieto hasta terminar (o hasta ver a alguien).
    if (b.healTimer > 0.0f) desired = {0.0f, 0.0f, 0.0f};

    // Encima de algo (tejado de una nave, contenedor, altillo) tras caer del autobus: la
    // navegacion solo llega al suelo, asi que primero hay que bajar.
    if (s.grounded() && pos.y > 1.2f) {
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
        applyDamage(b.target, m_config.pickaxeDamage * m_config.botDamageScale, self);
        return;
    }

    const WeaponConfig& weapon = b.weapon.config();
    const int type = b.weaponType == ItemType::None ? -1 : static_cast<int>(b.weaponType);
    const float scale = m_config.botDamageScale;
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
    if (s.horizontalSpeed() > 1.0f) spreadDeg += 1.0f;
    fireWeapon(self, eye, math3d::normalize(aim - eye), weapon, spreadDeg, scale, type);
    b.weapon.addCooldown(weapon.fireInterval * m_rng.range(-0.15f, 0.25f));  // cadencia algo irregular
}

// --- IA de la fase 7: botin, oido, cobertura -------------------------------------------------

float Match::botAttackRange(const Bot& b) const {
    if (!b.armed) return 1.8f;
    switch (b.weaponType) {
        case ItemType::Shotgun: return 7.0f;
        case ItemType::Smg: return 15.0f;
        case ItemType::Sniper: return 38.0f;
        default: return m_config.attackRange;
    }
}

bool Match::findLoot(int index, float maxDistance) {
    // Sin arma: el cofre o el arma mas cercana. Con arma: solo un arma mejor que la suya o
    // un airdrop ya en el suelo (a mas distancia: merece la pena).
    Bot& b = m_bots[index];
    const int self = index + 1;
    const Vec3 pos = b.body.state().position;
    float best = 1e9f;
    int pickup = -1, container = -1;
    for (size_t i = 0; i < m_containers.size(); ++i) {
        const Container& c = m_containers[i];
        if (c.opened || !c.landed || c.kind == ContainerKind::AmmoBox) continue;
        if (c.claimedBy >= 0 && c.claimedBy != self) continue;
        if (b.armed && c.kind != ContainerKind::Airdrop && b.bag.full()) continue;
        const float d = distXZ(c.position, pos);
        const float limit = c.kind == ContainerKind::Airdrop ? maxDistance * 3.0f : maxDistance;
        if (d > limit || stormDepthAt(c.position.x, c.position.z) > -4.0f) continue;
        const float score = c.kind == ContainerKind::Airdrop ? d * 0.4f : d;
        if (score < best) {
            best = score;
            container = static_cast<int>(i);
            pickup = -1;
        }
    }
    for (size_t i = 0; i < m_pickups.size(); ++i) {
        const Pickup& p = m_pickups[i];
        if (!p.active || p.kind != PickupKind::Item) continue;
        if (p.claimedBy >= 0 && p.claimedBy != self) continue;
        // Sin arma, primero un arma; despues, lo que le sirva (otra arma, una mejor o curas).
        if (!b.armed && !isWeapon(p.item)) continue;
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

void Match::botBuildCover(int index, int attacker) {
    // Como un jugador: al recibir disparos desde lejos, a veces se cubre con una pared hacia
    // el atacante (sin gastar materiales; hay un limite de paredes por bot y en total).
    if (!m_config.botsBuild || index < 0) return;
    Bot& b = m_bots[index];
    const int self = index + 1;
    if (b.wallCooldown > 0.0f || m_air[self] != AirState::Ground || !b.body.state().grounded()) return;
    const Vec3 pos = b.body.state().position;
    const Vec3 from = body(attacker).state().position;
    if (distXZ(pos, from) < 5.0f) return;
    if (!m_rng.chance(0.6f)) {
        b.wallCooldown = 2.0f;
        return;
    }
    world::Aabb others[kMaxBots + 1];
    int n = 0;
    for (int id = 0; id < entityCount(); ++id) {
        if (id != self && alive(id) && m_air[id] == AirState::Ground) others[n++] = bodyBounds(id);
    }
    const WallPlacement plan = m_structures.plan(pos, yawTowards(pos, from), bodyBounds(self), others, n, self);
    b.wallCooldown = m_config.botWallCooldown + m_rng.range(0.0f, 3.0f);
    if (!plan.valid) return;
    const int wall = m_structures.place(plan, self);
    if (wall >= 0) {
        ++m_stats.wallsBuilt;
        const world::Aabb& box = m_structures.wall(wall).box;
        pushEvent(GameEventType::WallBuilt, self, -1, (box.min + box.max) * 0.5f);
    }
}

void Match::hearShot(int shooter, const Vec3& origin) {
    // Los bots armados que no estan peleando ni buscando botin van a mirar de donde vienen
    // los disparos que oyen.
    for (int i = 0; i < static_cast<int>(m_bots.size()); ++i) {
        Bot& b = m_bots[i];
        const int self = i + 1;
        if (self == shooter || b.state == BotState::Eliminated || b.state == BotState::Attack ||
            b.state == BotState::Loot || !b.armed || m_air[self] != AirState::Ground) {
            continue;
        }
        if (distXZ(b.body.state().position, origin) > m_config.hearingRange) continue;
        b.investigate = true;
        b.destination = origin + Vec3{m_rng.range(-5.0f, 5.0f), 0.0f, m_rng.range(-5.0f, 5.0f)};
        b.state = BotState::Explore;
        b.pathLength = 0;
        if (b.decisionTimer > 0.15f) b.decisionTimer = 0.15f;
    }
}

}  // namespace game
