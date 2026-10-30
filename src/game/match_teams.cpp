// Equipos del battle royale (v0.16, docs/EQUIPOS.md): parejas y escuadrones de bots. Quien se
// queda sin vida con un companero en pie cae derribado (a cuatro patas, perdiendo vida poco a
// poco) y solo un companero puede levantarle; si cae el ultimo en pie del equipo, los
// derribados caen con el. Los bots del mismo equipo saltan juntos, van juntos y se reaniman.
#include "game/match.h"

#include "core/timer.h"
#include "game/match_internal.h"
#include "world/storm.h"

namespace game {

using math3d::Vec3;
using namespace tuning;
using namespace detail;

// --- Reparto ---------------------------------------------------------------------------------

void Match::splitIntoTeams() {
    // Las entidades van en orden: el jugador y sus companeros son las primeras.
    for (int id = 0; id <= kMaxBots; ++id) {
        m_team[id] = static_cast<u8>(id / m_config.teamSize);
        m_downed[id] = false;
        m_bleedRate[id] = 0.0f;
        m_downedBy[id] = -1;
        m_downCount[id] = 0;
        m_reviver[id] = -1;
        m_reviveTime[id] = 0.0f;
        m_reviveSeen[id] = false;
    }
    m_playerReviving = -1;
}

int Match::teamCount() const { return (entityCount() + m_config.teamSize - 1) / m_config.teamSize; }

int Match::teamsLeft() const {
    if (!teamMode()) return m_alive;
    int n = 0, last = -1;
    for (int id = 0; id < entityCount(); ++id) {
        if (!alive(id) || m_team[id] == last) continue;
        last = m_team[id];
        ++n;
    }
    return n;
}

bool Match::playerTeamAlive() const {
    for (int id = 0; id < m_config.teamSize && id < entityCount(); ++id) {
        if (alive(id)) return true;
    }
    return false;
}

bool Match::teamStanding(int id) const {
    const int first = m_team[id] * m_config.teamSize;
    for (int o = first; o < first + m_config.teamSize && o < entityCount(); ++o) {
        if (o != id && alive(o) && !m_downed[o]) return true;
    }
    return false;
}

int Match::teamLeader(int id) const {
    // El primero en pie: en el equipo del jugador, el jugador.
    const int first = m_team[id] * m_config.teamSize;
    for (int o = first; o < first + m_config.teamSize && o < entityCount(); ++o) {
        if (alive(o) && !m_downed[o] && m_air[o] == AirState::Ground) return o;
    }
    return -1;
}

// --- Derribados --------------------------------------------------------------------------------

void Match::releaseLootClaims(Bot& b) {
    if (b.lootPickup >= 0 && b.lootPickup < static_cast<int>(m_pickups.size())) m_pickups[b.lootPickup].claimedBy = -1;
    if (b.lootContainer >= 0 && b.lootContainer < static_cast<int>(m_containers.size())) m_containers[b.lootContainer].claimedBy = -1;
    b.lootPickup = b.lootContainer = -1;
}

void Match::goDown(int victim, int attacker) {
    CharacterState& s = mutableBody(victim).state();
    ++m_stats.downs;
    m_downed[victim] = true;
    m_downedBy[victim] = static_cast<short>(attacker);
    if (m_downCount[victim] < 200) ++m_downCount[victim];
    // Cada vez que vuelve a caer aguanta menos (como en Fortnite).
    const float seconds = m_config.downSeconds / (1.0f + 0.5f * static_cast<float>(m_downCount[victim] - 1));
    s.health = s.maxHealth;
    s.shield = 0.0f;
    m_bleedRate[victim] = s.health / fmaxf(seconds, 1.0f);
    m_reviver[victim] = -1;
    m_reviveTime[victim] = 0.0f;
    m_reviveSeen[victim] = false;
    if (victim == 0) {
        cancelUse();
        if (Weapon* w = m_inventory.activeWeapon()) w->cancelReload();
        m_playerCrouched = false;
        m_buildMode = false;
    } else {
        Bot& b = m_bots[victim - 1];
        releaseLootClaims(b);
        b.state = BotState::Downed;
        b.stateTime = 0.0f;
        b.target = -1;
        b.targetVisible = false;
        b.breakingWall = false;
        b.healTimer = 0.0f;
        b.healSlot = -1;
        b.buildStep = 0;
        b.climbTimer = 0.0f;
        b.coverFrom = -1;
        b.harvestObject = -1;
        b.campContainer = -1;
        b.reviveMate = -1;
        b.pathLength = 0;
        b.aiming = false;
        b.holding = false;
        b.investigate = false;
    }
    pushEvent(GameEventType::Downed, victim, attacker, body(victim).state().position);
}

void Match::updateDowned(float dt) {
    if (m_phase == MatchPhase::Finished) return;
    for (int id = 0; id < entityCount(); ++id) {
        if (!m_downed[id]) continue;
        const int rescuer = m_reviver[id];
        if (m_reviveSeen[id] && rescuer >= 0 && alive(rescuer) && !m_downed[rescuer]) {
            // Mientras le atienden no pierde vida.
            m_reviveSeen[id] = false;
            m_reviveTime[id] += dt;
            if (m_reviveTime[id] >= m_config.reviveSeconds) revive(id, rescuer);
            continue;
        }
        m_reviver[id] = -1;
        m_reviveTime[id] = 0.0f;
        m_reviveSeen[id] = false;
        CharacterState& s = mutableBody(id).state();
        s.health -= m_bleedRate[id] * dt;
        if (s.health <= 0.0f) {
            m_damageSource = -1;
            eliminate(id, m_downedBy[id]);
        }
    }
}

bool Match::markRevive(int rescuer, int mate) {
    if (m_reviver[mate] >= 0 && m_reviver[mate] != rescuer) return false;
    m_reviver[mate] = static_cast<short>(rescuer);
    m_reviveSeen[mate] = true;
    return true;
}

void Match::revive(int id, int rescuer) {
    CharacterState& s = mutableBody(id).state();
    ++m_stats.revives;
    m_downed[id] = false;
    m_bleedRate[id] = 0.0f;
    m_reviver[id] = -1;
    m_reviveTime[id] = 0.0f;
    s.health = fminf(m_config.reviveHealth, s.maxHealth);
    s.shield = 0.0f;
    if (id > 0) {
        Bot& b = m_bots[id - 1];
        b.state = BotState::Search;
        b.stateTime = 0.0f;
        b.searchDuration = 0.8f;
        b.searchYaw = s.yaw;
        b.decisionTimer = 0.1f;
    }
    if (rescuer > 0) {
        Bot& r = m_bots[rescuer - 1];
        r.reviveMate = -1;
        r.state = BotState::Explore;
    }
    pushEvent(GameEventType::Revived, id, rescuer, s.position);
}

int Match::reviveTarget() const {
    if (!teamMode() || !alive(0) || m_downed[0] || m_air[0] != AirState::Ground) return -1;
    const Vec3 pos = body(0).state().position;
    int best = -1;
    float bestD = m_config.reviveReach;
    for (int id = 1; id < m_config.teamSize && id < entityCount(); ++id) {
        if (!m_downed[id] || !alive(id) || (m_reviver[id] >= 0 && m_reviver[id] != 0)) continue;
        const Vec3 p = body(id).state().position;
        const float d = distXZ(p, pos);
        if (d <= bestD && fabsf(p.y - pos.y) < 1.6f) {
            bestD = d;
            best = id;
        }
    }
    return best;
}

// --- IA de equipo -------------------------------------------------------------------------------

void Match::updateDownedBot(int index, float dt) {
    // A cuatro patas: se arrastra hacia el companero en pie mas cercano (quieto si le atienden).
    Bot& b = m_bots[index];
    const int self = index + 1;
    const Vec3 pos = b.body.state().position;
    Vec3 desired{0.0f, 0.0f, 0.0f};
    if (m_reviver[self] < 0) {
        float best = 1e9f;
        const int first = m_team[self] * m_config.teamSize;
        for (int o = first; o < first + m_config.teamSize && o < entityCount(); ++o) {
            if (o == self || !alive(o) || m_downed[o]) continue;
            const float d = distXZ(body(o).state().position, pos);
            if (d < best && d > 1.5f) {
                best = d;
                desired = math3d::normalize(flatten(body(o).state().position - pos));
            }
        }
    }
    b.wantsJump = false;
    b.aiming = false;
    const MoveIntent intent = avoidObstacles(index, desired, m_config.downCrawlSpeed, MoveIntent{});
    const u64 t0 = core::nowMicros();
    b.body.step(intent, dt, *m_world);
    m_stats.physicsMicros += core::nowMicros() - t0;
}

int Match::downedMate(int index, float range) const {
    // El companero derribado mas cercano que no tenga ya a otro yendo a por el.
    const int self = index + 1;
    const Vec3 pos = m_bots[index].body.state().position;
    int best = -1;
    float bestD = range;
    const int first = m_team[self] * m_config.teamSize;
    for (int o = first; o < first + m_config.teamSize && o < entityCount(); ++o) {
        if (o == self || !m_downed[o] || !alive(o) || m_air[o] != AirState::Ground) continue;
        if (m_reviver[o] >= 0 && m_reviver[o] != self) continue;
        bool taken = false;
        for (int r = first; r < first + m_config.teamSize && r < entityCount() && !taken; ++r) {
            taken = r > 0 && r != self && m_bots[r - 1].reviveMate == o;
        }
        if (taken) continue;
        const float d = distXZ(body(o).state().position, pos);
        if (d < bestD) {
            bestD = d;
            best = o;
        }
    }
    return best;
}

void Match::startBotRevive(int index, int mate) {
    Bot& b = m_bots[index];
    releaseLootClaims(b);
    b.harvestObject = -1;
    b.campContainer = -1;
    b.investigate = false;
    b.holding = false;
    b.state = BotState::Revive;
    b.reviveMate = mate;
    b.stateTime = 0.0f;
    b.pathLength = 0;
    b.pathRefresh = 0.0f;
    b.target = -1;
    b.targetVisible = false;
}

void Match::updateBotRevive(int index) {
    // Cada paso, ya junto al companero, sigue reanimandole (updateDowned lleva la cuenta).
    Bot& b = m_bots[index];
    if (b.state != BotState::Revive) return;
    const int mate = b.reviveMate;
    if (mate < 0 || !alive(mate) || !m_downed[mate]) {
        b.reviveMate = -1;
        b.state = BotState::Explore;
        return;
    }
    const Vec3 pos = b.body.state().position, at = body(mate).state().position;
    if (distXZ(pos, at) <= m_config.reviveReach - 0.3f && fabsf(pos.y - at.y) < 1.6f) markRevive(index + 1, mate);
}

void Match::teamLanding() {
    // Cada equipo salta casi a la vez y al mismo sitio. Los companeros del jugador esperan a
    // que salte el (followPlayerJump).
    if (!teamMode()) return;
    for (int i = 0; i < botCount(); ++i) {
        Bot& b = m_bots[i];
        const int self = i + 1;
        const int lead = m_team[self] * m_config.teamSize;
        if (lead == self) continue;
        if (lead == 0) {
            b.jumpTime = m_config.busSeconds * 2.0f;
            continue;
        }
        const Bot& l = m_bots[lead - 1];
        const float a = static_cast<float>(self - lead) * 2.1f;
        b.jumpTime = l.jumpTime + 0.3f * static_cast<float>(self - lead);
        b.glideHeight = l.glideHeight;
        b.landingTarget = l.landingTarget + Vec3{cosf(a) * 3.0f, 0.0f, sinf(a) * 3.0f};
    }
}

void Match::followPlayerJump(int index) {
    if (m_air[0] == AirState::Bus) return;
    Bot& b = m_bots[index];
    const int self = index + 1;
    if (b.jumpTime > m_time + 2.0f) b.jumpTime = m_time + 0.35f * static_cast<float>(self);
    const float a = static_cast<float>(self) * 2.1f;
    b.landingTarget = body(0).state().position + Vec3{cosf(a) * 3.0f, 0.0f, sinf(a) * 3.0f};
}

}  // namespace game
