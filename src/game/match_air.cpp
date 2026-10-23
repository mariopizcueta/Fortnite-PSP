// Autobus de batalla y caida (game::Match). El autobus esta quieto en el cielo
// (MatchConfig::busPosition); todos empiezan dentro. Se salta con X (o al acabar
// busSeconds), se cae en caida libre dirigiendo con el stick, el ala delta se abre con X
// (o sola a glideAutoHeight metros del suelo) y al tocar el suelo empieza la partida a
// pie. Los bots saltan en momentos distintos hacia sitios distintos (cofres, sobre todo).
#include "game/match.h"

#include "game/match_internal.h"
#include "world/storm.h"

namespace game {

using math3d::Vec3;
using namespace tuning;
using namespace detail;

const char* airStateName(AirState state) {
    switch (state) {
        case AirState::Bus: return "Autobus";
        case AirState::Freefall: return "Caida libre";
        case AirState::Glide: return "Ala delta";
        case AirState::Ground: return "Suelo";
    }
    return "?";
}

float Match::heightAboveGround(int id) const {
    const Vec3& p = body(id).state().position;
    return p.y - m_world->supportHeight(p.x, p.z, kFootRadius, p.y + 0.01f);
}

void Match::startBus() {
    for (int id = 0; id < entityCount(); ++id) {
        m_air[id] = AirState::Bus;
        mutableBody(id).place(m_config.busPosition);
        CharacterState& s = mutableBody(id).state();
        s.velocity = {0.0f, 0.0f, 0.0f};
        s.jumpState = JumpState::Falling;
    }
    for (Bot& b : m_bots) {
        b.jumpTime = m_rng.range(1.5f, m_config.busSeconds * 0.6f);
        b.glideHeight = m_rng.range(m_config.glideAutoHeight, m_config.glideAutoHeight + 14.0f);
        chooseLanding(b);
    }
}

void Match::chooseLanding(Bot& b) {
    // Como la gente en Fortnite: casi siempre a un sitio con botin (un cofre) que se
    // alcance desde el autobus; si no, a un punto cualquiera de la zona segura.
    const Vec3 bus = flatten(m_config.busPosition);
    for (int attempt = 0; attempt < 8; ++attempt) {
        Vec3 p;
        if (!m_containers.empty() && m_rng.chance(0.7f)) {
            p = m_containers[m_rng.index(static_cast<int>(m_containers.size()))].position;
            p = p + Vec3{m_rng.range(-3.0f, 3.0f), 0.0f, m_rng.range(-3.0f, 3.0f)};
        } else if (!m_nav->randomPointNear(bus, 70.0f, m_rng, &p)) {
            continue;
        }
        if (distXZ(p, bus) > 75.0f || world::stormDepth(p.x, p.z) > -6.0f) continue;
        // A cielo abierto: los cofres de dentro de las naves se buscan ya a pie (si no,
        // se aterriza en el tejado).
        if (m_world->supportHeight(p.x, p.z, 1.0f, 200.0f) > 1.0f) continue;
        b.landingTarget = p;
        return;
    }
    b.landingTarget = bus + Vec3{m_rng.range(-20.0f, 20.0f), 0.0f, m_rng.range(-20.0f, 20.0f)};
}

void Match::updateAirState(int id, bool jump) {
    CharacterState& s = mutableBody(id).state();
    switch (m_air[id]) {
        case AirState::Bus:
            mutableBody(id).place(m_config.busPosition);
            s.velocity = {0.0f, 0.0f, 0.0f};
            if (jump || m_time >= m_config.busSeconds) {
                // Sale por la puerta trasera, algo por debajo del autobus.
                m_air[id] = AirState::Freefall;
                mutableBody(id).place(m_config.busPosition + Vec3{m_rng.range(-0.8f, 0.8f), -3.5f, 3.0f});
                s.velocity = {0.0f, -4.0f, 0.0f};
                s.jumpState = JumpState::Falling;
                pushEvent(GameEventType::BusJump, id, -1, s.position);
            }
            break;
        case AirState::Freefall: {
            const float height = heightAboveGround(id);
            const float autoHeight = id == 0 ? m_config.glideAutoHeight : m_bots[id - 1].glideHeight;
            if (s.grounded()) {
                m_air[id] = AirState::Ground;
                pushEvent(GameEventType::Landed, id, -1, s.position);
            } else if ((jump && height > 4.0f) || height < autoHeight) {
                m_air[id] = AirState::Glide;
                pushEvent(GameEventType::GliderOpen, id, -1, s.position);
            }
            break;
        }
        case AirState::Glide:
            if (s.grounded()) {
                m_air[id] = AirState::Ground;
                pushEvent(GameEventType::Landed, id, -1, s.position);
            }
            break;
        case AirState::Ground:
            break;
    }
}

MoveIntent Match::airIntent(int id, const Vec3& wish, bool /*openGlider*/) {
    MoveIntent intent;
    intent.wish = wish;
    const bool glide = m_air[id] == AirState::Glide;
    intent.airSpeed = glide ? m_config.glideSpeed : m_config.freefallSpeed;
    intent.fallSpeed = glide ? m_config.glideFallSpeed : m_config.freefallFallSpeed;
    return intent;
}

}  // namespace game
