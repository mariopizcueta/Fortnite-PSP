// IA de los aliados del modo zombi (v0.16, docs/ZOMBIS.md). Antes de nada: de noche, el que
// tiene cama asignada se acuesta pase lo que pase. Despues, por orden: con poca vida huye y se
// cubre con lo que construye; lejos del jugador lo deja todo y va con el (disparando de
// camino); si ve un zombi, pelea; y, si no, trabaja: fabrica junto a un centro de crafteo,
// recoge botin y materiales, fortifica su cama de dia y levanta la base. Si el jugador sube
// por una rampa construida, sube detras (la navegacion solo llega al suelo).
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

constexpr float kBedReach = 2.7f;
constexpr float kBenchReach = 3.0f;
constexpr int kFortPieces = 4;     // tres paredes (la puerta queda libre) y un techo
constexpr float kElevated = 1.2f;  // m: por encima, la navegacion ya no sirve
constexpr float kAllyCornered = 2.5f;  // m: con poca vida, solo pelea con el zombi encima
constexpr float kRampTop = 3.7f;   // m recorridos sobre la rampa para darla por subida

// Pie y cabeza de una rampa construida (en XZ), segun hacia donde sube.
void rampEnds(const BuiltWall& w, Vec3* foot, Vec3* top) {
    const Vec3 c = (w.box.min + w.box.max) * 0.5f;
    Vec3 f = c, t = c;
    switch (w.dir) {
        case 0: f.x = w.box.min.x - 0.6f; t.x = w.box.max.x; break;
        case 1: f.x = w.box.max.x + 0.6f; t.x = w.box.min.x; break;
        case 2: f.z = w.box.min.z - 0.6f; t.z = w.box.max.z; break;
        default: f.z = w.box.max.z + 0.6f; t.z = w.box.min.z; break;
    }
    *foot = f;
    *top = t;
}

}  // namespace

// --- Decision -------------------------------------------------------------------------------

void Match::decideSurvivor(int index) {
    Bot& b = m_bots[index];
    const ZombieConfig& z = m_config.zombie;
    const Vec3 pos = b.body.state().position;
    if (!alive(0)) {
        b.state = BotState::Idle;
        b.pathLength = 0;
        return;
    }
    b.fireOnMove = false;
    // 1) Con cama asignada, de noche se duerme si o si (sale con tiempo para llegar).
    if (allyBedtime(b)) {
        allyGoToBed(index);
        return;
    }
    b.bedWalk = 0.0f;
    if (b.sleeping) {
        b.sleeping = false;  // amanece: se levanta (la cama sigue siendo suya)
        b.state = BotState::Search;
        b.stateTime = 0.0f;
        b.searchDuration = 0.5f;
    }
    if (b.climbTimer > 0.0f) return;  // subiendo por una rampa: hasta arriba
    // Atascado (quiere andar y apenas avanza): se aparta y salta; despues, camino nuevo.
    b.progressTimer += m_config.aiDecisionInterval;
    if (b.progressTimer >= kStuckWindow) {
        const bool moving = b.pathIndex < b.pathLength && b.state != BotState::Attack && b.state != BotState::Idle;
        if (moving && distXZ(pos, b.progressAnchor) < kStuckMinProgress) {
            ++m_stats.stuckEvents;
            b.sidestepTimer = 0.7f;
            b.sidestepSign = m_rng.chance(0.5f) ? 1.0f : -1.0f;
            b.wantsJump = true;
            b.pathRefresh = 0.0f;
        }
        b.progressAnchor = pos;
        b.progressTimer = 0.0f;
    }
    const float toPlayer = distXZ(pos, body(0).state().position);
    int seen = perceive(index);
    float seenDistance = seen >= 0 ? distXZ(body(seen).state().position, pos) : 1e9f;
    if (seenDistance > z.survivorSight) {
        seen = -1;
        seenDistance = 1e9f;
    }
    botEquip(b, seen >= 0 ? seenDistance : 15.0f);
    // Los materiales que pisa, para el (con ellos construye y fabrica).
    for (Pickup& p : m_pickups) {
        if (!p.active || p.kind != PickupKind::Materials || distXZ(p.position, pos) > 1.4f || fabsf(p.position.y - pos.y) > 1.5f) continue;
        p.count -= b.materials.add(p.resource, p.count);
        if (p.count <= 0) p.active = false;
    }

    // 2) Poca vida: huye hacia el jugador, se cubre y se cura. Solo pelea si le acorralan.
    const bool cornered = seen >= 0 && b.armed && seenDistance < kAllyCornered;
    if (b.state != BotState::Flee && botWeak(b) && !cornered) {
        const int threat = seen >= 0 ? seen : (b.sinceHurt < 3.0f ? b.threat : -1);
        if (threat >= 0 && alive(threat)) {
            b.allyPiece = 0;
            b.craftBench = -1;
            b.climbRamp = -1;
            startFlee(index, threat);
        }
    }
    if (b.state == BotState::Flee) {
        const CharacterState& s = b.body.state();
        if (cornered) {
            b.state = BotState::Attack;
        } else if (s.health + s.shield >= z.allyRecovered || (b.stateTime > 15.0f && seen < 0 && b.healTimer <= 0.0f)) {
            b.state = BotState::Idle;
            b.threat = -1;
            b.target = -1;
            return;
        } else {
            if (seen >= 0) {
                b.threat = seen;
                b.lastSeen = body(seen).state().position;
            }
            b.target = b.threat;
            b.targetVisible = seen >= 0;
            const bool arrived = b.pathIndex >= b.pathLength;
            if (b.pathRefresh <= 0.0f && (seen >= 0 || (arrived && b.healTimer <= 0.0f && b.stateTime < 2.0f))) {
                Vec3 dest;
                const Vec3 from = b.threat >= 0 && alive(b.threat) ? body(b.threat).state().position : b.lastSeen;
                if (fleePoint(b, from, &dest)) {
                    b.destination = dest;
                    if (requestPath(b, dest)) b.pathRefresh = 2.5f;
                }
            }
            return;
        }
    }

    // 3) Antes que pelear, seguir al jugador: lejos de el lo deja todo (y dispara de camino).
    if (toPlayer > z.allyLeash) {
        releaseLootClaims(b);
        b.allyPiece = 0;
        b.craftBench = -1;
        b.harvestObject = -1;
        if (b.state == BotState::Loot || b.state == BotState::Harvest || b.state == BotState::Attack) b.state = BotState::Search;
        if (seen >= 0) {
            if (b.target != seen) b.reaction = m_config.reactionTime;
            b.target = seen;
            b.targetVisible = true;
            b.lastSeen = body(seen).state().position;
            b.fireOnMove = true;
        } else {
            b.target = -1;
            b.targetVisible = false;
        }
        allyFollow(index);
        return;
    }

    // 4) Un zombi a la vista: pelea (sin perseguirle: se mueve de lado en su sitio).
    if (seen >= 0) {
        releaseLootClaims(b);
        b.allyPiece = 0;
        b.craftBench = -1;
        b.harvestObject = -1;
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
        if (b.strafeTimer <= 0.0f) {
            b.strafeSign = m_rng.chance(0.5f) ? 1.0f : -1.0f;
            b.strafeTimer = m_rng.range(0.8f, 1.8f);
        }
        return;
    }
    b.targetVisible = false;
    b.target = -1;

    // 5) Si el jugador ha subido por una rampa, detras de el; en lo alto, nada de trabajar.
    const bool above = body(0).state().grounded() && body(0).state().position.y - pos.y > 1.0f;
    if ((above && allyStartClimb(index)) || pos.y > kElevated) {
        if (pos.y > kElevated && !above) allyFollow(index);
        return;
    }
    b.climbRamp = -1;

    // 6) Trabajo y, si no hay nada que hacer, con el jugador.
    if (allyTasks(index)) return;
    allyFollow(index);
}

void Match::updateSurvivor(int index, float dt) {
    Bot& b = m_bots[index];
    if (b.state == BotState::Eliminated) return;
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
    if (b.sleeping) {
        // Durmiendo en su cama: quieto (se levanta en su siguiente decision si toca).
        b.aiming = false;
        b.decisionTimer -= dt;
        if (b.decisionTimer <= 0.0f) {
            b.decisionTimer = 1.0f;
            decideSurvivor(index);
        }
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
    b.climbTimer -= dt;
    b.wallCooldown -= dt;
    b.meleeTimer -= dt;
    b.swingTimer -= dt;
    b.sinceHurt += dt;
    b.decisionTimer -= dt;
    // Huyendo, lo primero es cubrirse: una caja si tiene material y, si no, una pared.
    if (b.coverFrom >= 0) {
        if (b.state == BotState::Flee && alive(b.coverFrom)) {
            int mats = 0;
            for (int r = 0; r < static_cast<int>(world::Resource::Count); ++r) mats += b.materials.amount[r];
            const Vec3 from = body(b.coverFrom).state().position;
            if (mats >= 40 && b.buildStep == 0 && b.boxTimer <= 0.0f) startSmallBuild(index, 1, yawTowards(b.body.state().position, from));
            else botBuildCover(index, b.coverFrom, true);
        }
        b.coverFrom = -1;
    }
    updateSmallBuild(index, dt);
    if (b.decisionTimer <= 0.0f) {
        decideSurvivor(index);
        ++m_stats.decisions;
        b.decisionTimer += m_config.aiDecisionInterval;
        if (b.decisionTimer <= 0.0f) b.decisionTimer = m_config.aiDecisionInterval;
    }
    if (b.sleeping) return;  // se acaba de acostar
    botUpdateHealing(index, dt);
    botHarvest(index, dt);
    MoveIntent intent = steer(index, dt);
    if (b.state == BotState::Idle) {
        intent.faceYawOverride = true;
        intent.faceYaw = b.searchYaw;
    }
    if (b.fireOnMove && b.climbTimer <= 0.0f) {
        intent.faceYawOverride = true;
        intent.faceYaw = yawTowards(b.body.state().position, b.lastSeen);
        b.aiming = true;
    }
    // En lo alto de una rampa pelea quieto: moviendose de lado se caeria.
    if (b.state == BotState::Attack && b.body.state().position.y > kElevated) intent.wish = {0.0f, 0.0f, 0.0f};
    const u64 t1 = core::nowMicros();
    b.body.step(intent, dt, *m_world);
    const u64 t2 = core::nowMicros();
    if (b.climbRamp >= 0) allyClimbStep(index);
    // Llega a la obra (o a su cama): levanta la pieza o se acuesta.
    if (b.allyPiece != 0 || b.bed >= 0) allyBuildStep(index);
    botShoot(index);
    m_stats.aiMicros += (t1 - t0) + (core::nowMicros() - t2);
    m_stats.physicsMicros += t2 - t1;
}

// --- Camas -----------------------------------------------------------------------------------

int Match::nearbyBed() const {
    if (!zombieMode() || !alive(0) || m_air[0] != AirState::Ground) return -1;
    const Vec3 p = body(0).state().position;
    int best = -1;
    float bestD = kBedReach;
    for (size_t i = 0; i < m_containers.size(); ++i) {
        const Container& c = m_containers[i];
        if (c.kind != ContainerKind::Bed || fabsf(c.position.y - p.y) > 1.6f) continue;
        const float d = distXZ(c.position, p);
        if (d <= bestD) {
            bestD = d;
            best = static_cast<int>(i);
        }
    }
    return best;
}

int Match::bedOwner(int bed) const {
    if (bed < 0 || bed >= static_cast<int>(m_containers.size()) || m_containers[bed].kind != ContainerKind::Bed) return -1;
    const int owner = m_containers[bed].claimedBy;
    return owner >= 0 && owner < botCount() && m_bots[owner].bed == bed && m_bots[owner].state != BotState::Eliminated ? owner : -1;
}

bool Match::assignBed(int bed, int botIndex) {
    if (bed < 0 || bed >= static_cast<int>(m_containers.size()) || m_containers[bed].kind != ContainerKind::Bed) return false;
    if (botIndex >= botCount()) return false;
    if (botIndex >= 0) {
        const Bot& b = m_bots[botIndex];
        if (b.kind != BotKind::Survivor || !b.rescued || b.state == BotState::Eliminated) return false;
    }
    auto release = [&](Bot& b) {
        if (b.sleeping) {
            b.sleeping = false;
            b.state = BotState::Search;
            b.stateTime = 0.0f;
            b.searchDuration = 0.5f;
        }
        if (b.bed >= 0 && b.bed < static_cast<int>(m_containers.size())) m_containers[b.bed].claimedBy = -1;
        b.bed = -1;
    };
    const int owner = bedOwner(bed);
    if (owner >= 0) release(m_bots[owner]);
    m_containers[bed].claimedBy = -1;
    if (botIndex >= 0) {
        Bot& b = m_bots[botIndex];
        release(b);
        b.bed = bed;
        m_containers[bed].claimedBy = botIndex;
    }
    pushEvent(GameEventType::BedAssigned, botIndex >= 0 ? botIndex + 1 : -1, bed, m_containers[bed].position);
    return true;
}

bool Match::allyBedtime(const Bot& b) const {
    if (b.bed < 0 || b.bed >= static_cast<int>(m_containers.size()) || m_containers[b.bed].kind != ContainerKind::Bed) return false;
    if (m_night) return true;
    // Al atardecer: sale con el tiempo justo para estar acostado cuando caiga la noche.
    const float walk = distXZ(b.body.state().position, m_containers[b.bed].position) / (kRunSpeed * 0.8f);
    return timeToChange() < walk + 5.0f;
}

void Match::allyLieDown(int index) {
    Bot& b = m_bots[index];
    const Container& c = m_containers[b.bed];
    b.sleeping = true;
    b.pathLength = 0;
    b.state = BotState::Idle;
    b.target = -1;
    b.targetVisible = false;
    b.fireOnMove = false;
    b.healTimer = 0.0f;
    b.healSlot = -1;
    b.buildStep = 0;
    b.bedWalk = 0.0f;
    b.body.place(c.position);
    b.body.state().yaw = c.yaw;
    b.body.state().velocity = {0.0f, 0.0f, 0.0f};
    pushEvent(GameEventType::AllySleeps, index + 1, b.bed, c.position);
}

void Match::allyGoToBed(int index) {
    Bot& b = m_bots[index];
    if (b.sleeping) return;
    releaseLootClaims(b);
    b.allyPiece = 0;
    b.craftBench = -1;
    b.harvestObject = -1;
    b.climbRamp = -1;
    b.climbTimer = 0.0f;
    b.target = -1;
    b.targetVisible = false;
    // De noche, si algo le impide llegar, se acuesta igualmente: duerme si o si.
    if (m_night) b.bedWalk += m_config.aiDecisionInterval;
    if (b.bedWalk > m_config.zombie.allyBedWalkLimit) {
        allyLieDown(index);
        return;
    }
    const Vec3 bed = m_containers[b.bed].position;
    if (b.state != BotState::Travel || distXZ(b.destination, bed) > 0.5f || b.pathRefresh <= 0.0f) {
        b.destination = bed;
        if (allyGoTo(b, bed)) {
            b.state = BotState::Travel;
            b.stateTime = 0.0f;
            b.pathRefresh = 2.0f;
        }
    }
}

// --- Seguir al jugador (tambien por sus rampas) ----------------------------------------------------

bool Match::allyGoTo(Bot& b, const Vec3& spot) {
    // Recto si ya esta en lo alto o el sitio esta cerca y sin nada en medio (los nodos de la
    // navegacion van cada 4 m: se quedarian cortos). Si no, camino por la navegacion.
    const Vec3 pos = b.body.state().position;
    const bool clear = pos.y > kElevated || NavGrid::clearWalk(*m_world, pos, spot, kBodyRadius);
    if (!(clear && (pos.y > kElevated || distXZ(pos, spot) < 6.0f)) && requestPath(b, spot)) return true;
    if (!clear) return false;
    b.path[0] = spot;
    b.pathLength = 1;
    b.pathIndex = 0;
    return true;
}

void Match::allyFollow(int index) {
    Bot& b = m_bots[index];
    const Vec3 pos = b.body.state().position;
    const CharacterState& player = body(0).state();
    const float d = distXZ(pos, player.position);
    const int k = index - kZombieSlots;
    // Un hueco detras de el (cada uno el suyo), sin pegarse.
    const float angle = player.yaw + math3d::kPi + (static_cast<float>(k) - 2.0f) * 0.55f;
    const Vec3 spot = player.position + Vec3{-sinf(angle) * 3.2f, 0.0f, -cosf(angle) * 3.2f};
    const float dy = player.position.y - pos.y;
    // En lo alto se queda con el jugador; si el baja, baja (Match::steer busca por donde).
    b.perchTimer = pos.y > kElevated && dy > -1.0f ? 1.0f : 0.0f;
    // El jugador ha subido por una rampa: detras de el.
    if (dy > 1.0f && player.grounded() && allyStartClimb(index)) return;
    b.climbRamp = -1;
    if (pos.y > kElevated && fabsf(dy) < 1.0f) {
        // Los dos en lo alto (una rampa, un suelo construido): recto hacia el, sin caerse.
        const Vec3 to = flatten(player.position - pos);
        const float len = math3d::length(to);
        bool safe = d > 2.6f && len > 0.01f;
        if (safe) {
            const Vec3 ahead = pos + to * (1.0f / len);
            safe = pos.y - m_world->supportHeight(ahead.x, ahead.z, kFootRadius, pos.y + kStepHeight) < 1.0f;
        }
        if (safe) {
            b.path[0] = player.position;
            b.pathLength = 1;
            b.pathIndex = 0;
            b.state = BotState::Travel;
        } else {
            b.state = BotState::Idle;
            b.pathLength = 0;
            b.searchYaw = player.yaw;
        }
        return;
    }
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

bool Match::allyStartClimb(int index) {
    // La rampa construida cuyo pie esta a su altura y que mas le acerca al jugador. Con varias
    // en cadena se repite en cada piso.
    Bot& b = m_bots[index];
    const Vec3 pos = b.body.state().position;
    const Vec3 player = body(0).state().position;
    const float rise = m_config.build.wallHeight;
    int best = -1;
    float bestScore = 1e9f;
    Vec3 bestFoot;
    for (int i = 0; i < m_structures.capacity(); ++i) {
        const BuiltWall& w = m_structures.wall(i);
        if (!w.active || w.type != PieceType::Ramp || fabsf(w.level - pos.y) > 0.9f || w.level + rise > player.y + 1.6f) continue;
        Vec3 foot, top;
        rampEnds(w, &foot, &top);
        const float dFoot = distXZ(pos, foot);
        if (dFoot > 40.0f) continue;
        const float score = distXZ(top, player) + 2.0f * fabsf(player.y - (w.level + rise)) + 0.2f * dFoot;
        if (score < bestScore) {
            bestScore = score;
            best = i;
            bestFoot = foot;
        }
    }
    if (best < 0) return false;
    if (b.climbRamp != best || b.state != BotState::Travel || b.pathRefresh <= 0.0f) {
        b.climbRamp = best;
        b.destination = bestFoot;
        if (allyGoTo(b, bestFoot)) {
            b.state = BotState::Travel;
            b.pathRefresh = 1.5f;
        }
    }
    return true;
}

void Match::allyClimbStep(int index) {
    // Al pie de la rampa: sube recto (Match::steer con climbTimer) hasta lo alto y para.
    Bot& b = m_bots[index];
    const BuiltWall& w = m_structures.wall(b.climbRamp);
    if (!w.active || w.type != PieceType::Ramp) {
        b.climbRamp = -1;
        b.climbTimer = 0.0f;
        return;
    }
    const Vec3 pos = b.body.state().position;
    Vec3 foot, top;
    rampEnds(w, &foot, &top);
    if (b.climbTimer > 0.0f) {
        const float along = fabsf(w.dir < 2 ? pos.x - foot.x : pos.z - foot.z);
        if (along >= kRampTop + 0.6f || pos.y >= w.level + m_config.build.wallHeight - 0.25f) {
            b.climbTimer = 0.0f;
            b.climbRamp = -1;
            b.perchTimer = 3.0f;
            b.pathLength = 0;
            b.state = BotState::Idle;
            b.searchYaw = b.body.state().yaw;
        }
    } else if (distXZ(pos, foot) < 1.5f && fabsf(pos.y - w.level) < 0.7f) {
        b.buildYaw = yawTowards(foot, top);
        b.climbTimer = 2.5f;
        b.perchTimer = 4.0f;
        b.pathLength = 0;
    }
}

// --- Trabajo ----------------------------------------------------------------------------------------

bool Match::allyTasks(int index) {
    Bot& b = m_bots[index];
    if (b.state == BotState::Loot) return allyLootTask(index);
    if (b.state == BotState::Harvest) return allyHarvestTask(index);
    if (allyCraftTask(index)) return true;
    if (m_rng.chance(0.3f) && allyLootTask(index)) return true;
    if (allyFortify(index)) return true;
    if (updateAllyDuty(index)) return true;
    return allyHarvestTask(index);
}

bool Match::allyLootTask(int index) {
    // Armas (solo mejores que las suyas: botWants), curas, mochilas y materiales cerca.
    Bot& b = m_bots[index];
    const Vec3 pos = b.body.state().position;
    if (b.state != BotState::Loot) {
        if (!findLoot(index, m_config.zombie.allyLootRange)) return false;
        b.state = BotState::Loot;
        b.stateTime = 0.0f;
        b.pathRefresh = 0.0f;
        b.pathLength = 0;
    }
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
    if (!valid || b.stateTime > 12.0f) {
        if (b.lootPickup >= 0) b.skipPickup = b.lootPickup;
        releaseLootClaims(b);
        b.state = BotState::Idle;
        return false;
    }
    b.destination = goal;
    if (distXZ(pos, goal) < 1.8f && fabsf(goal.y - pos.y) < 1.6f) {
        botTakeLoot(index);
    } else if (b.pathRefresh <= 0.0f || b.pathIndex >= b.pathLength) {
        if (allyGoTo(b, goal)) b.pathRefresh = 2.0f;
    }
    return true;
}

bool Match::allyHarvestTask(int index) {
    // Sin material y de dia, pica lo que tenga cerca (nunca lo de la base ni los lugares).
    Bot& b = m_bots[index];
    const ZombieConfig& z = m_config.zombie;
    int total = 0;
    for (int r = 0; r < static_cast<int>(world::Resource::Count); ++r) total += b.materials.amount[r];
    if (b.state != BotState::Harvest) {
        if (total >= z.allyHarvestBelow || m_night || !m_rng.chance(0.2f) || !findHarvest(index)) return false;
        b.state = BotState::Harvest;
        b.stateTime = 0.0f;
        return true;
    }
    if (total >= z.allyMaterialGoal / 2 || b.stateTime > 20.0f || b.harvestObject < 0 || m_objectDestroyed[b.harvestObject]) {
        b.harvestObject = -1;
        b.state = BotState::Idle;
        return false;
    }
    const world::Aabb& box = (*m_harvestables)[b.harvestObject].box;
    if (distanceToBox(chestOf(index + 1), box) <= m_config.pickaxeReach - 0.2f) {
        b.pathLength = 0;
    } else if (b.pathRefresh <= 0.0f || b.pathIndex >= b.pathLength) {
        if (allyGoTo(b, b.destination)) b.pathRefresh = 2.5f;
    }
    return true;
}

int Match::allyRecipe(const Bot& b) const {
    // Un arma mejor que la mejor que lleva o, si no lleva curas, curas; lo que pueda pagar.
    float bestWeapon = 0.0f;
    for (int i = 0; i < b.bag.slotCount(); ++i) {
        const InventorySlot& s = b.bag.slot(i);
        if (isWeapon(s.type)) bestWeapon = fmaxf(bestWeapon, weaponScore(s.type, s.rarity));
    }
    const bool heals = botHasHeals(b);
    int best = -1;
    float bestScore = 0.0f;
    for (int r = 0; r < recipeCount(); ++r) {
        const Recipe& rc = recipe(r);
        bool paid = true;
        for (int m = 0; m < static_cast<int>(world::Resource::Count); ++m) paid &= b.materials.amount[m] >= rc.cost[m];
        if (!paid) continue;
        float score = 0.0f;
        if (isWeapon(rc.item)) {
            const float s = weaponScore(rc.item, existingRarity(rc.item, rc.rarity));
            if (s > bestWeapon + 2.0f) score = s;
        } else if (!heals && (rc.item == ItemType::Bandage || rc.item == ItemType::Medkit)) {
            score = 100.0f;  // lo primero, poder curarse
        }
        if (score > bestScore) {
            bestScore = score;
            best = r;
        }
    }
    return best;
}

void Match::allyCraft(int index, int recipeIndex) {
    Bot& b = m_bots[index];
    const Recipe& rc = recipe(recipeIndex);
    for (int m = 0; m < static_cast<int>(world::Resource::Count); ++m) b.materials.spend(static_cast<world::Resource>(m), rc.cost[m]);
    const Rarity rarity = isWeapon(rc.item) ? existingRarity(rc.item, rc.rarity) : rc.rarity;
    if (isWeapon(rc.item) && b.bag.full()) {
        Pickup wanted;
        wanted.item = rc.item;
        wanted.rarity = rarity;
        const int slot = botReplacedSlot(b, wanted);
        if (slot >= 0) dropSlot(b.bag.take(slot), b.body.state().position);
    }
    b.bag.add(rc.item, rarity, rc.count);
    botEquip(b, 15.0f);
    ++m_stats.allyCrafts;
    pushEvent(GameEventType::AllyCrafted, index + 1, recipeIndex, b.body.state().position);
}

bool Match::allyCraftTask(int index) {
    Bot& b = m_bots[index];
    const ZombieConfig& z = m_config.zombie;
    const Vec3 pos = b.body.state().position;
    if (b.craftBench < 0) {
        if (!m_rng.chance(0.25f) || allyRecipe(b) < 0) return false;
        float best = z.allyCraftRange;
        for (size_t i = 0; i < m_containers.size(); ++i) {
            const Container& c = m_containers[i];
            if (c.kind != ContainerKind::Workbench) continue;
            const float d = distXZ(c.position, pos);
            if (d < best) {
                best = d;
                b.craftBench = static_cast<int>(i);
            }
        }
        if (b.craftBench < 0) return false;
        b.craftTimer = 0.0f;
        b.state = BotState::Search;  // Travel solo cuando tenga camino
    }
    const Container& c = m_containers[b.craftBench];
    const Vec3 front = c.position + forwardFromYaw(c.yaw) * 1.2f;
    if (distXZ(pos, c.position) <= kBenchReach) {
        // Junto al banco: un momento y lo fabrica (si sigue pudiendo pagarlo).
        b.state = BotState::Idle;
        b.pathLength = 0;
        b.searchYaw = yawTowards(pos, c.position);
        b.craftTimer += m_config.aiDecisionInterval;
        if (b.craftTimer < z.allyCraftSeconds) return true;
        const int r = allyRecipe(b);
        if (r >= 0) allyCraft(index, r);
        b.craftBench = -1;
        return false;
    }
    if (b.state == BotState::Travel && b.stateTime > 15.0f) {
        b.craftBench = -1;  // no llega
        return false;
    }
    if (b.state != BotState::Travel || b.pathIndex >= b.pathLength) {
        b.destination = front;
        if (!allyGoTo(b, front)) {
            b.craftBench = -1;
            return false;
        }
        if (b.state != BotState::Travel) b.stateTime = 0.0f;
        b.state = BotState::Travel;
    }
    return true;
}

bool Match::fortPiece(int bed, int k, WallPlacement* out) const {
    // La celda de la rejilla de construccion en la que esta la cama: tres paredes (la cuarta,
    // por donde se llega a ella, queda libre: el lado desde el que se coloco) y un techo.
    if (bed < 0 || bed >= static_cast<int>(m_containers.size()) || k < 0 || k >= kFortPieces) return false;
    const Container& c = m_containers[bed];
    const BuildConfig& build = m_config.build;
    const float g = build.gridSize;
    const float x0 = floorf(c.position.x / g) * g, z0 = floorf(c.position.z / g) * g;
    const Vec3 f = forwardFromYaw(c.yaw);
    const int door = fabsf(f.x) > fabsf(f.z) ? (f.x > 0.0f ? 1 : 0) : (f.z > 0.0f ? 3 : 2);
    const BlueprintPiece sides[4] = {{PieceType::Wall, x0 + g, z0, 0, false, 0},
                                     {PieceType::Wall, x0, z0, 0, false, 0},
                                     {PieceType::Wall, x0, z0 + g, 0, true, 0},
                                     {PieceType::Wall, x0, z0, 0, true, 0}};
    BlueprintPiece piece{PieceType::Floor, x0, z0, 1, true, 0};
    if (k < 3) {
        int side = 0;
        for (int n = 0; n <= k; ++side) {
            if (side != door) ++n;
        }
        piece = sides[side - 1];
    }
    WallPlacement p = blueprintPlacement(piece, build);
    p.box.min.y += c.position.y;
    p.box.max.y += c.position.y;
    p.level += c.position.y;
    const world::Aabb shrunk{p.box.min + Vec3{0.03f, 0.03f, 0.03f}, p.box.max - Vec3{0.03f, 0.03f, 0.03f}};
    // No a traves de la cama (si esta sobre una linea de la rejilla) ni del escenario.
    const float ax = fabsf(sinf(c.yaw)) > fabsf(cosf(c.yaw)) ? 1.0f : 0.5f, az = 1.5f - ax;
    const world::Aabb bedBox{{c.position.x - ax, c.position.y, c.position.z - az}, {c.position.x + ax, c.position.y + 0.6f, c.position.z + az}};
    if (shrunk.overlaps(bedBox) || m_world->overlapsAnyStatic(shrunk, world::kBlocksMovement)) return false;
    *out = p;
    return true;
}

bool Match::allyFortify(int index) {
    // De dia y con el jugador por la zona, levanta su refugio junto a la cama.
    Bot& b = m_bots[index];
    const ZombieConfig& z = m_config.zombie;
    if (b.bed < 0 || m_night || b.bed >= static_cast<int>(m_containers.size())) return false;
    const Vec3 bed = m_containers[b.bed].position;
    if (distXZ(body(0).state().position, bed) > z.allyFortRange) return false;
    WallPlacement p;
    bool pending = false;
    for (int k = 0; k < kFortPieces && !pending; ++k) pending = fortPiece(b.bed, k, &p) && !m_structures.occupied(p);
    if (!pending) return false;
    const Vec3 pos = b.body.state().position;
    if (distXZ(pos, bed) > 2.8f) {
        if (b.state != BotState::Travel || distXZ(b.destination, bed) > 0.5f || b.pathIndex >= b.pathLength) {
            b.destination = bed;
            if (!allyGoTo(b, bed)) return false;
            b.state = BotState::Travel;
            b.stateTime = 0.0f;
        }
        return true;
    }
    b.state = BotState::Idle;
    b.pathLength = 0;
    if (m_allyBuildTimer <= 0.0f) allyPlacePiece(index, p, 1.0f, false, true, nullptr);
    return true;
}

bool Match::allyPlacePiece(int index, WallPlacement p, float healthScale, bool mustPay, bool ownFirst, int* wall) {
    // Paga con su material o con el del jugador (ownFirst: lo suyo antes); sin ninguno, con lo
    // que rapina (`mustPay`: entonces no se levanta), el triple de lento.
    Bot& b = m_bots[index];
    const ZombieConfig& z = m_config.zombie;
    const int cost = z.pieceCost;
    bool paid = m_config.cheats.infiniteMaterials, own = false;
    p.material = world::Resource::Wood;
    auto pay = [&](Materials& from, bool isOwn) {
        for (int r = static_cast<int>(world::Resource::Count) - 1; r >= 0 && !paid; --r) {
            if (from.get(static_cast<world::Resource>(r)) < cost) continue;
            p.material = static_cast<world::Resource>(r);
            from.spend(p.material, cost);
            paid = true;
            own = isOwn;
        }
    };
    if (ownFirst) pay(b.materials, true);
    pay(m_materials, false);
    if (!ownFirst) pay(b.materials, true);
    if (!paid && mustPay) return false;
    auto refund = [&]() {
        if (!paid || m_config.cheats.infiniteMaterials) return;
        (own ? b.materials : m_materials).add(p.material, cost);
    };
    // Nadie dentro de la pieza.
    world::Aabb boxes[kMaxPieceBoxes];
    const int n = pieceBoxes(p, m_config.build, boxes);
    for (int id = 0; id < entityCount(); ++id) {
        if (!alive(id)) continue;
        const world::Aabb body = bodyBounds(id);
        for (int i = 0; i < n; ++i) {
            const world::Aabb shrunk{boxes[i].min + Vec3{0.03f, 0.03f, 0.03f}, boxes[i].max - Vec3{0.03f, 0.03f, 0.03f}};
            if (shrunk.overlaps(body)) {
                refund();
                m_allyBuildTimer = 1.0f;
                return false;
            }
        }
    }
    const int placed = m_structures.placeForced(p, kBlueprintOwner, healthScale);
    if (placed < 0) {
        refund();
        return false;
    }
    if (wall) *wall = placed;
    m_allyBuildTimer = z.allyBuildSeconds * (paid ? 1.0f : 3.0f);
    ++m_stats.allyPieces;
    pushEvent(GameEventType::WallBuilt, index + 1, -1, (p.box.min + p.box.max) * 0.5f);
    return true;
}

bool Match::updateAllyDuty(int index) {
    // Con el jugador en la base y de dia, a levantar piezas. true: tiene obra.
    Bot& b = m_bots[index];
    const ZombieConfig& z = m_config.zombie;
    const Vec3 pos = b.body.state().position;
    const bool home = alive(0) && distXZ(body(0).state().position, world::zsites::kBaseCenter) < z.allyBaseRange;
    if (!home || m_night) {
        b.allyPiece = 0;
        return false;
    }
    if (b.allyPiece == 0) b.allyPiece = pieceKeyNear(pos);
    if (b.allyPiece == 0) return false;
    const int bp = (b.allyPiece - 1) / kMaxBlueprintPieces, k = (b.allyPiece - 1) % kMaxBlueprintPieces;
    if (pieceBuilt(bp, k)) {
        b.allyPiece = 0;
        return false;
    }
    // A pie de obra: el punto de la rejilla junto a la pieza (por fuera de la base no: dentro).
    const world::Aabb box = blueprintPiece(bp, k).box;
    Vec3 spot = (box.min + box.max) * 0.5f;
    spot = spot + math3d::normalize(flatten(world::zsites::kBaseCenter - spot)) * 1.8f;
    spot.y = 0.0f;
    if (b.state != BotState::Travel || distXZ(b.destination, spot) > 0.5f) {
        b.destination = spot;
        if (allyGoTo(b, spot)) {
            b.state = BotState::Travel;
            b.stateTime = 0.0f;
        } else {
            b.allyPiece = 0;  // no llega: otra
            return false;
        }
    }
    return true;
}

bool Match::allyBuildStep(int index) {
    // Cada paso: si el aliado ha llegado a su cama, se acuesta; si ha llegado a su obra y le
    // toca, levanta la pieza (reforzada si el plano se ha encontrado).
    Bot& b = m_bots[index];
    const Vec3 pos = b.body.state().position;
    if (b.bed >= 0 && b.allyPiece == 0) {
        if (b.bed >= static_cast<int>(m_containers.size()) || m_containers[b.bed].kind != ContainerKind::Bed) {
            b.bed = -1;
            return false;
        }
        if (!allyBedtime(b) || distXZ(pos, m_containers[b.bed].position) > 1.4f) return false;
        allyLieDown(index);
        return true;
    }
    if (b.allyPiece == 0 || m_allyBuildTimer > 0.0f) return false;
    const int bp = (b.allyPiece - 1) / kMaxBlueprintPieces, k = (b.allyPiece - 1) % kMaxBlueprintPieces;
    const WallPlacement p = blueprintPiece(bp, k);
    if (distXZ(pos, (p.box.min + p.box.max) * 0.5f) > 3.6f) return false;
    if (pieceBuilt(bp, k)) {
        b.allyPiece = 0;
        return false;
    }
    int wall = -1;
    const bool mustPay = bp >= kPalisadeBlueprints;
    if (!allyPlacePiece(index, p, m_bpFound[bp] ? m_config.zombie.pieceHealthScale : 1.0f, mustPay, false, &wall)) {
        if (mustPay || wall < 0) {
            if (m_allyBuildTimer <= 0.0f) b.allyPiece = 0;
        }
        return false;
    }
    m_bpWall[bp][k] = static_cast<short>(wall);
    b.allyPiece = 0;
    const Vec3 center = (p.box.min + p.box.max) * 0.5f;
    pushEvent(GameEventType::AllyBuilt, index + 1, bp, center);
    m_events[(m_eventCount - 1) % kMaxEvents].amount = k;
    if (m_bpFound[bp] && !m_bpDone[bp]) {
        bool all = true;
        for (int i = 0; i < blueprint(bp).pieceCount && all; ++i) all = pieceBuilt(bp, i);
        if (all) {
            m_bpDone[bp] = true;
            pushEvent(GameEventType::BlueprintDone, 0, bp, center);
        }
    }
    return true;
}

}  // namespace game
