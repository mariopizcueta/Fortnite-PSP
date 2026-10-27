#include "game/character.h"

#include "core/i18n.h"

#include <math.h>

#include "world/storm.h"
#include "world/world_units.h"

namespace game {

using math3d::Vec3;
using namespace tuning;

namespace {

constexpr float kSkin = 0.002f;  // holgura al separar de una pared

float& axisRef(Vec3& v, int axis) { return axis == 0 ? v.x : (axis == 1 ? v.y : v.z); }
float axisOf(const Vec3& v, int axis) { return axis == 0 ? v.x : (axis == 1 ? v.y : v.z); }

}  // namespace

float CharacterState::horizontalSpeed() const {
    return sqrtf(velocity.x * velocity.x + velocity.z * velocity.z);
}

float wrapAngle(float a) {
    const float twoPi = 2.0f * math3d::kPi;
    while (a > math3d::kPi) a -= twoPi;
    while (a <= -math3d::kPi) a += twoPi;
    return a;
}

float approachAngle(float from, float to, float maxDelta) {
    const float diff = wrapAngle(to - from);
    if (diff > maxDelta) return wrapAngle(from + maxDelta);
    if (diff < -maxDelta) return wrapAngle(from - maxDelta);
    return wrapAngle(to);
}

float yawTowards(const Vec3& from, const Vec3& to) { return atan2f(-(to.x - from.x), -(to.z - from.z)); }

Vec3 forwardFromYaw(float yaw) { return {-sinf(yaw), 0.0f, -cosf(yaw)}; }

const char* jumpStateName(JumpState state) {
    switch (state) {
        case JumpState::Grounded: return i18n::tr(i18n::T::JumpGrounded);
        case JumpState::Rising: return i18n::tr(i18n::T::JumpRising);
        case JumpState::Falling: return i18n::tr(i18n::T::JumpFalling);
    }
    return "?";
}

const char* locomotionName(const CharacterState& s) {
    const float speed = s.horizontalSpeed();
    if (speed < 0.2f) return i18n::tr(i18n::T::MoveIdle);
    if (speed < kRunSpeed * 0.6f) return i18n::tr(i18n::T::MoveWalk);
    return i18n::tr(i18n::T::MoveRun);
}

void CharacterBody::reset(const Vec3& spawn, float yaw, const world::CollisionWorld& world) {
    m_state = CharacterState{};
    m_state.position = spawn;
    m_state.position.y = world.supportHeight(spawn.x, spawn.z, kFootRadius, spawn.y + kStepHeight);
    m_state.yaw = wrapAngle(yaw);
    depenetrate(world);
    m_previous = m_state;
    m_jumpBuffer = 0.0f;
    m_coyote = kCoyoteTime;
    m_jumpCut = false;
}

world::Aabb CharacterBody::bounds() const {
    const Vec3& p = m_state.position;
    return {{p.x - kBodyRadius, p.y, p.z - kBodyRadius},
            {p.x + kBodyRadius, p.y + kBodyHeight, p.z + kBodyRadius}};
}

void CharacterBody::moveAxis(int axis, float delta, const world::CollisionWorld& world) {
    if (delta == 0.0f) return;
    Vec3& p = m_state.position;
    axisRef(p, axis) += delta;
    // La parte baja (hasta kStepHeight) no bloquea en horizontal: es un escalon que se
    // sube en la fase vertical.
    const world::Aabb body{{p.x - kBodyRadius, p.y + kStepHeight, p.z - kBodyRadius},
                           {p.x + kBodyRadius, p.y + kBodyHeight, p.z + kBodyRadius}};
    // Moviendose en un solo eje, la caja mas restrictiva decide la posicion final.
    float limit = axisOf(p, axis);
    bool blocked = false;
    world.query(body, world::kBlocksMovement, [&](const world::Collider& c) {
        if (delta > 0.0f) {
            const float allowed = axisOf(c.box.min, axis) - kBodyRadius - kSkin;
            if (allowed < limit) limit = allowed;
        } else {
            const float allowed = axisOf(c.box.max, axis) + kBodyRadius + kSkin;
            if (allowed > limit) limit = allowed;
        }
        blocked = true;
    });
    if (blocked) {
        axisRef(p, axis) = limit;
        axisRef(m_state.velocity, axis) = 0.0f;
    }
}

bool CharacterBody::depenetrate(const world::CollisionWorld& world) {
    bool moved = false;
    for (int iteration = 0; iteration < 4; ++iteration) {
        Vec3& p = m_state.position;
        const world::Aabb body{{p.x - kBodyRadius, p.y + kStepHeight, p.z - kBodyRadius},
                               {p.x + kBodyRadius, p.y + kBodyHeight, p.z + kBodyRadius}};
        // Busca la salida mas corta (en X o Z) de la primera caja que solape.
        bool found = false;
        float bestPush = 1e9f;
        int bestAxis = 0;
        float bestValue = 0.0f;
        world.query(body, world::kBlocksMovement, [&](const world::Collider& c) {
            found = true;
            const float candidates[4][2] = {
                {0, c.box.min.x - kBodyRadius - kSkin}, {0, c.box.max.x + kBodyRadius + kSkin},
                {2, c.box.min.z - kBodyRadius - kSkin}, {2, c.box.max.z + kBodyRadius + kSkin}};
            for (const auto& cand : candidates) {
                const int axis = static_cast<int>(cand[0]);
                const float push = fabsf(cand[1] - axisOf(p, axis));
                if (push < bestPush) {
                    bestPush = push;
                    bestAxis = axis;
                    bestValue = cand[1];
                }
            }
        });
        if (!found) break;
        axisRef(p, bestAxis) = bestValue;
        moved = true;
    }
    return moved;
}

void CharacterBody::step(const MoveIntent& intent, float dt, const world::CollisionWorld& world) {
    m_previous = m_state;
    CharacterState& s = m_state;
    const bool wasGrounded = s.grounded();
    if (intent.jumpPressed) m_jumpBuffer = kJumpBuffer;

    // --- Velocidad horizontal ----------------------------------------------------------
    Vec3 wish{intent.wish.x, 0.0f, intent.wish.z};
    float amount = math3d::length(wish);
    if (amount > 1.0f) {
        wish = wish * (1.0f / amount);
        amount = 1.0f;
    }
    const bool flying = !wasGrounded && intent.airSpeed > 0.0f;
    const Vec3 target = wish * (flying ? intent.airSpeed : kRunSpeed * intent.speedScale);
    const float accel = wasGrounded || intent.fly ? (amount > 0.0f ? kGroundAccel : kGroundBrake) : (flying ? 16.0f : kAirAccel);
    const float dvx = target.x - s.velocity.x;
    const float dvz = target.z - s.velocity.z;
    const float dv = sqrtf(dvx * dvx + dvz * dvz);
    const float maxChange = accel * dt;
    if (dv <= maxChange) {
        s.velocity.x = target.x;
        s.velocity.z = target.z;
    } else {
        s.velocity.x += dvx / dv * maxChange;
        s.velocity.z += dvz / dv * maxChange;
    }
    const float hs = s.horizontalSpeed();
    const float maxSpeed = flying ? intent.airSpeed : kRunSpeed * intent.maxSpeedScale;
    if (hs > maxSpeed) {
        s.velocity.x *= maxSpeed / hs;
        s.velocity.z *= maxSpeed / hs;
    }

    // --- Orientacion -----------------------------------------------------------------------
    if (intent.faceYawOverride) {
        s.yaw = approachAngle(s.yaw, intent.faceYaw, kTurnRate * dt);
    } else if (amount > 0.1f) {
        s.yaw = approachAngle(s.yaw, atan2f(-wish.x, -wish.z), kTurnRate * dt);
    }

    // --- Salto ---------------------------------------------------------------------------
    m_coyote = wasGrounded ? kCoyoteTime : m_coyote - dt;
    if (m_jumpBuffer > 0.0f && m_coyote > 0.0f && !intent.fly) {
        s.velocity.y = kJumpSpeed * intent.jumpScale;
        s.jumpState = JumpState::Rising;
        m_jumpBuffer = 0.0f;
        m_coyote = 0.0f;
        m_jumpCut = false;
    }
    m_jumpBuffer = m_jumpBuffer > dt ? m_jumpBuffer - dt : 0.0f;
    if (!s.grounded() && s.velocity.y > 0.0f && !intent.jumpHeld && !m_jumpCut) {
        s.velocity.y *= kJumpCutFactor;
        m_jumpCut = true;
    }

    // --- Movimiento horizontal con colision, eje a eje ---------------------------------
    moveAxis(0, s.velocity.x * dt, world);
    moveAxis(2, s.velocity.z * dt, world);

    // Ya no hay pared invisible en el borde del mapa: alli empieza la tormenta (ver
    // world/storm.h). Este tope, al final del suelo, no se alcanza vivo.
    const float limit = world::kWorldExtent - kMapMargin;
    if (s.position.x > limit || s.position.x < -limit) {
        s.position.x = math3d::clampf(s.position.x, -limit, limit);
        s.velocity.x = 0.0f;
    }
    if (s.position.z > limit || s.position.z < -limit) {
        s.position.z = math3d::clampf(s.position.z, -limit, limit);
        s.velocity.z = 0.0f;
    }

    // --- Vertical: gravedad, apoyo y techo -------------------------------------------------
    const float feetBefore = s.position.y;
    if (intent.fly) {
        // Vuelo (truco): sin gravedad, sube o baja a voluntad sin atravesar suelos ni techos.
        constexpr float kFlySpeed = 9.0f;
        s.velocity.y = intent.flyVertical * kFlySpeed;
        float y = feetBefore + s.velocity.y * dt;
        const float support = world.supportHeight(s.position.x, s.position.z, kFootRadius, feetBefore + 0.01f);
        if (s.velocity.y > 0.0f) {
            const float ceiling = world.ceilingHeight(s.position.x, s.position.z, kFootRadius, feetBefore + kBodyHeight - 0.05f);
            if (y + kBodyHeight > ceiling) y = ceiling - kBodyHeight;
        }
        if (y <= support) {
            y = support;
            s.velocity.y = 0.0f;
            s.jumpState = JumpState::Grounded;
            s.airTime = 0.0f;
        } else {
            s.jumpState = s.velocity.y > 0.0f ? JumpState::Rising : JumpState::Falling;
            s.airTime += dt;
        }
        s.position.y = y;
        return;
    }
    if (s.grounded()) {
        // Pegado al suelo: sube escalones y baja escaleras; si debajo no hay nada a
        // menos de un escalon, empieza a caer.
        const float support =
            world.supportHeight(s.position.x, s.position.z, kFootRadius, feetBefore + kStepHeight);
        if (support >= feetBefore - kStepHeight) {
            s.position.y = support;
            s.velocity.y = 0.0f;
        } else {
            s.jumpState = JumpState::Falling;
        }
    }
    if (!s.grounded()) {
        s.velocity.y -= kGravity * intent.gravityScale * dt;
        if (intent.fallSpeed > 0.0f && s.velocity.y < -intent.fallSpeed) {
            // Ala delta (o caida libre): frena hasta su velocidad de caida en ~0.5 s.
            s.velocity.y += fminf(-intent.fallSpeed - s.velocity.y, 30.0f * dt + kGravity * dt);
        }
        if (s.velocity.y < -kMaxFallSpeed) s.velocity.y = -kMaxFallSpeed;
        s.position.y += s.velocity.y * dt;

        if (s.velocity.y > 0.0f) {
            // Cabeza contra un techo (p. ej. bajo el altillo de una nave).
            const float ceiling = world.ceilingHeight(s.position.x, s.position.z, kFootRadius,
                                                      feetBefore + kBodyHeight - 0.05f);
            if (s.position.y + kBodyHeight > ceiling) {
                s.position.y = ceiling - kBodyHeight;
                s.velocity.y = 0.0f;
            }
        } else {
            // Apoyo: cualquier techo de caja que estuviera por debajo de los pies antes del
            // paso. Aunque se caiga 0.67 m en un paso, la plataforma se detecta (sin tunelado).
            const float support =
                world.supportHeight(s.position.x, s.position.z, kFootRadius, feetBefore + 0.01f);
            if (s.position.y <= support) {
                s.position.y = support;
                s.velocity.y = 0.0f;
                s.jumpState = JumpState::Grounded;
            }
        }
    }

    if (!s.grounded()) {
        s.jumpState = s.velocity.y > 0.0f ? JumpState::Rising : JumpState::Falling;
        s.airTime += dt;
    } else {
        s.airTime = 0.0f;
    }
}

CharacterState CharacterBody::interpolated(float alpha) const {
    CharacterState r = m_state;
    r.position = math3d::lerp(m_previous.position, m_state.position, alpha);
    r.yaw = wrapAngle(m_previous.yaw + wrapAngle(m_state.yaw - m_previous.yaw) * alpha);
    return r;
}

}  // namespace game
