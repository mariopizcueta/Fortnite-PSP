#include "camera/third_person_camera.h"

#include <math.h>

#include "game/character.h"
#include "world/terrain.h"

namespace camera {

using math3d::Vec3;

namespace {

constexpr float kLockTurnRate = 1.6f;   // rad/s: fijar objetivo gira la camara, no la clava
constexpr float kLockLookRate = 1.0f;
constexpr float kOrbitPitch = 0.24f;    // el ojo queda ~1.2 m por encima del pivote
constexpr float kPivotHeight = 1.7f;    // sobre los pies
constexpr float kDefaultLook = -0.065f; // ~ -4 grados: la mira cae a la altura del pecho
constexpr float kMinLook = -0.50f;
constexpr float kMaxLook = 0.45f;
constexpr float kYawSpeed = 2.4f;       // rad/s con el stick en el modo apuntado
constexpr float kLookSpeed = 1.0f;
constexpr float kFollowRate = 0.9f;     // 1/s: lo que tarda en colocarse detras al andar
constexpr float kLookReturnRate = 1.5f; // 1/s: la mira vuelve a la altura normal al andar
constexpr float kMinEyeHeight = 0.7f;   // >= 1.155 * plano cercano (0.5 m), ver MUNDO.md
constexpr float kWallMargin = 0.6f;     // separacion del ojo respecto a una pared

// Suavizado exponencial (1/s): independiente del frame rate.
constexpr float kFollowHorizontal = 12.0f;
constexpr float kFollowVertical = 7.0f;  // mas lento: el salto no sacude la camara
constexpr float kAngleRate = 14.0f;

float smoothFactor(float rate, float dt) { return 1.0f - expf(-rate * dt); }

}  // namespace

void ThirdPersonCamera::reset(const Vec3& playerPosition, float playerYaw) {
    m_pivot = playerPosition + Vec3{0.0f, kPivotHeight, 0.0f};
    m_yaw = m_targetYaw = playerYaw;
    m_look = m_targetLook = kDefaultLook;
    computeEye(nullptr);
}

void ThirdPersonCamera::update(const CameraInput& control, const Vec3& playerPosition, float playerYaw,
                               float dt, bool recenter, const Vec3* lockTarget,
                               const world::CollisionWorld* world) {
    if (lockTarget) {
        // Fijar objetivo: la camara gira hacia el con velocidad limitada (no se clava en el:
        // si el objetivo se mueve, hay que seguir apuntando).
        const float wantYaw = game::yawTowards(m_pivot, *lockTarget);
        m_targetYaw = game::approachAngle(m_targetYaw, wantYaw, kLockTurnRate * dt);
        const Vec3 d = *lockTarget - m_eye;
        const float wantLook = atan2f(d.y, sqrtf(d.x * d.x + d.z * d.z));
        const float dl = wantLook - m_targetLook, maxLook = kLockLookRate * dt;
        m_targetLook += dl > maxLook ? maxLook : (dl < -maxLook ? -maxLook : dl);
    } else {
        // Giro manual: a la derecha el yaw decrece (crece hacia el oeste).
        m_targetYaw -= control.turn * kYawSpeed * dt;
        m_targetLook += control.look * kLookSpeed * dt;
        // Raton: sin suavizado (el giro llega ya entero y con retraso se notaria flotante).
        if (control.yawDelta != 0.0f || control.lookDelta != 0.0f) {
            m_targetYaw -= control.yawDelta;
            m_yaw -= control.yawDelta;
            m_targetLook = math3d::clampf(m_targetLook + control.lookDelta, kMinLook, kMaxLook);
            m_look = math3d::clampf(m_look + control.lookDelta, kMinLook, kMaxLook);
        }
        // Seguimiento: al andar, la camara se va colocando detras del jugador y la mira
        // vuelve a la altura normal (suavizado exponencial, independiente de los FPS).
        if (control.follow > 0.0f) {
            const float k = smoothFactor(kFollowRate * control.follow, dt);
            m_targetYaw += game::wrapAngle(control.followYaw - m_targetYaw) * k;
            m_targetLook += (control.restLook - m_targetLook) * smoothFactor(kLookReturnRate * control.follow, dt);
        }
        if (recenter) {
            m_targetYaw = playerYaw;
            m_targetLook = kDefaultLook;
        }
    }
    m_targetYaw = game::wrapAngle(m_targetYaw);
    m_targetLook = math3d::clampf(m_targetLook, kMinLook, kMaxLook);

    m_kick *= 1.0f - smoothFactor(9.0f, dt);
    const float angleT = smoothFactor(kAngleRate, dt);
    m_yaw = game::wrapAngle(m_yaw + game::wrapAngle(m_targetYaw - m_yaw) * angleT);
    m_look += (m_targetLook - m_look) * angleT;

    const Vec3 wanted = playerPosition + Vec3{0.0f, kPivotHeight, 0.0f};
    const float h = smoothFactor(kFollowHorizontal, dt);
    // En caida libre (20 m/s) el seguimiento vertical suave se quedaria atras.
    const float v = smoothFactor(fabsf(wanted.y - m_pivot.y) > 2.0f ? 20.0f : kFollowVertical, dt);
    m_distance += (control.distance - m_distance) * smoothFactor(3.0f, dt);
    m_pivot.x += (wanted.x - m_pivot.x) * h;
    m_pivot.z += (wanted.z - m_pivot.z) * h;
    m_pivot.y += (wanted.y - m_pivot.y) * v;

    computeEye(world);
}

void ThirdPersonCamera::computeEye(const world::CollisionWorld* world) {
    const float back = m_distance * cosf(kOrbitPitch);
    const Vec3 desired =
        m_pivot + Vec3{sinf(m_yaw) * back, m_distance * sinf(kOrbitPitch), cosf(m_yaw) * back};
    m_eye = desired;
    if (world) {
        world::RayHit hit;
        if (world->raycast(m_pivot, desired, world::kBlocksShots, &hit)) {
            const float len = math3d::length(desired - m_pivot);
            const float t = fmaxf(0.1f, hit.t - kWallMargin / len);
            m_eye = m_pivot + (desired - m_pivot) * t;
        }
    }
    // Nunca por debajo del suelo (ni tan cerca que el plano cercano lo recorte).
    const float minY = world::groundHeightAt(m_eye.x, m_eye.z) + kMinEyeHeight;
    if (m_eye.y < minY) m_eye.y = minY;
}

void ThirdPersonCamera::setAim(float yaw, float look) {
    m_yaw = m_targetYaw = game::wrapAngle(yaw);
    m_look = m_targetLook = math3d::clampf(look, kMinLook, kMaxLook);
}

void ThirdPersonCamera::aimAt(const Vec3& point, const world::CollisionWorld* world) {
    // El ojo orbita alrededor del pivote segun el yaw: unas pocas iteraciones bastan para
    // que el rayo del ojo nuevo pase por el punto.
    for (int k = 0; k < 4; ++k) {
        computeEye(world);
        const Vec3 d = point - m_eye;
        const float flat = sqrtf(d.x * d.x + d.z * d.z);
        if (flat < 1.0f) return;  // demasiado cerca: se deja como estaba
        setAim(atan2f(-d.x, -d.z), atan2f(d.y, flat));
    }
    computeEye(world);
}

Vec3 ThirdPersonCamera::lookDirection() const {
    const float look = m_look + m_kick;
    const float c = cosf(look);
    return {-sinf(m_yaw) * c, sinf(look), -cosf(m_yaw) * c};
}

math3d::Mat4 ThirdPersonCamera::viewMatrix() const {
    return math3d::Mat4::lookAt(m_eye, m_eye + lookDirection(), {0.0f, 1.0f, 0.0f});
}

}  // namespace camera
