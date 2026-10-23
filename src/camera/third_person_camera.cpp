#include "camera/third_person_camera.h"

#include <math.h>

#include "game/character.h"
#include "world/terrain.h"

namespace camera {

using math3d::Vec3;

namespace {

constexpr float kDistance = 5.0f;       // metros del pivote al ojo
constexpr float kOrbitPitch = 0.24f;    // el ojo queda ~1.2 m por encima del pivote
constexpr float kPivotHeight = 1.7f;    // sobre los pies
constexpr float kDefaultLook = -0.065f; // ~ -4 grados: la mira cae a la altura del pecho
constexpr float kMinLook = -0.50f;
constexpr float kMaxLook = 0.45f;
constexpr float kYawSpeed = 2.4f;       // rad/s con la cruceta
constexpr float kLookSpeed = 1.0f;
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

void ThirdPersonCamera::update(const input::Input& in, const Vec3& playerPosition, float playerYaw,
                               float dt, bool recenter, const Vec3* lockTarget,
                               const world::CollisionWorld* world) {
    if (lockTarget) {
        // Fijar objetivo: encararlo en horizontal y en vertical.
        m_targetYaw = game::yawTowards(m_pivot, *lockTarget);
        const Vec3 d = *lockTarget - m_eye;
        m_targetLook = atan2f(d.y, sqrtf(d.x * d.x + d.z * d.z));
    } else {
        // Cruceta: izquierda gira la vista a la izquierda (yaw crece hacia el oeste).
        if (in.held(PSP_CTRL_LEFT)) m_targetYaw += kYawSpeed * dt;
        if (in.held(PSP_CTRL_RIGHT)) m_targetYaw -= kYawSpeed * dt;
        if (in.held(PSP_CTRL_UP)) m_targetLook += kLookSpeed * dt;    // mirar arriba
        if (in.held(PSP_CTRL_DOWN)) m_targetLook -= kLookSpeed * dt;  // mirar abajo
        if (recenter) {
            m_targetYaw = playerYaw;
            m_targetLook = kDefaultLook;
        }
    }
    m_targetYaw = game::wrapAngle(m_targetYaw);
    m_targetLook = math3d::clampf(m_targetLook, kMinLook, kMaxLook);

    const float angleT = smoothFactor(kAngleRate, dt);
    m_yaw = game::wrapAngle(m_yaw + game::wrapAngle(m_targetYaw - m_yaw) * angleT);
    m_look += (m_targetLook - m_look) * angleT;

    const Vec3 wanted = playerPosition + Vec3{0.0f, kPivotHeight, 0.0f};
    const float h = smoothFactor(kFollowHorizontal, dt);
    const float v = smoothFactor(kFollowVertical, dt);
    m_pivot.x += (wanted.x - m_pivot.x) * h;
    m_pivot.z += (wanted.z - m_pivot.z) * h;
    m_pivot.y += (wanted.y - m_pivot.y) * v;

    computeEye(world);
}

void ThirdPersonCamera::computeEye(const world::CollisionWorld* world) {
    const float back = kDistance * cosf(kOrbitPitch);
    const Vec3 desired =
        m_pivot + Vec3{sinf(m_yaw) * back, kDistance * sinf(kOrbitPitch), cosf(m_yaw) * back};
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

Vec3 ThirdPersonCamera::lookDirection() const {
    const float c = cosf(m_look);
    return {-sinf(m_yaw) * c, sinf(m_look), -cosf(m_yaw) * c};
}

math3d::Mat4 ThirdPersonCamera::viewMatrix() const {
    return math3d::Mat4::lookAt(m_eye, m_eye + lookDirection(), {0.0f, 1.0f, 0.0f});
}

}  // namespace camera
