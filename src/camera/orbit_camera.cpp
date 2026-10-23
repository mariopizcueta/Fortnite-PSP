#include "camera/orbit_camera.h"

#include <math.h>

#include "world/world_units.h"

namespace camera {

using math3d::Vec3;

namespace {

constexpr float kOrbitSpeed = 1.8f;    // rad/s con el stick al maximo
constexpr float kZoomSpeed = 1.2f;     // factor exponencial por segundo
constexpr float kPanSpeedFactor = 0.6f;  // m/s por metro de distancia
constexpr float kMinPanSpeed = 6.0f;

constexpr float kMinPitch = 0.05f;   // casi a ras de suelo
constexpr float kMaxPitch = 1.45f;   // ~83 grados: evita el polo de lookAt
constexpr float kMinDistance = 3.0f;
constexpr float kMaxDistance = 110.0f;
// Con FOV vertical de 60 grados, el suelo visible en la esquina inferior queda a una
// profundidad >= altura / 1.155. Por debajo de ~1.16 m quedaria mas cerca que el plano
// cercano (1 m) y se veria el cielo a traves del suelo en el borde inferior.
constexpr float kMinEyeHeight = 1.3f;

}  // namespace

math3d::Mat4 Projection::matrix() const {
    return math3d::Mat4::perspective(math3d::toRadians(fovYDegrees), aspect, zNear, zFar);
}

void OrbitCamera::reset() {
    // Vista inicial: desde el noreste, con oficinas, silos, naves y la grua al fondo.
    m_target = {-6.0f, 2.0f, -8.0f};
    m_yaw = 2.25f;
    m_pitch = 0.34f;
    m_distance = 58.0f;
}

void OrbitCamera::set(const Vec3& target, float yaw, float pitch, float distance) {
    m_target = target;
    m_yaw = yaw;
    m_pitch = pitch;
    m_distance = distance;
    clampState();
}

void OrbitCamera::update(const input::Input& in, float dt) {
    m_yaw += in.analogX() * kOrbitSpeed * dt;
    m_pitch -= in.analogY() * kOrbitSpeed * dt;  // stick arriba = camara mas alta

    float zoom = 0.0f;
    if (in.held(PSP_CTRL_RTRIGGER)) zoom -= 1.0f;  // R acerca
    if (in.held(PSP_CTRL_LTRIGGER)) zoom += 1.0f;  // L aleja (L+R se anulan)
    m_distance *= expf(zoom * kZoomSpeed * dt);

    // Cruceta: desplaza el objetivo en el plano del suelo, relativo a la camara.
    float forward = 0.0f, strafe = 0.0f;
    if (in.held(PSP_CTRL_UP)) forward += 1.0f;
    if (in.held(PSP_CTRL_DOWN)) forward -= 1.0f;
    if (in.held(PSP_CTRL_RIGHT)) strafe += 1.0f;
    if (in.held(PSP_CTRL_LEFT)) strafe -= 1.0f;
    if (forward != 0.0f || strafe != 0.0f) {
        float speed = m_distance * kPanSpeedFactor;
        if (speed < kMinPanSpeed) speed = kMinPanSpeed;
        // Adelante = de la camara hacia el objetivo, proyectado en XZ.
        const Vec3 fwd{-sinf(m_yaw), 0.0f, -cosf(m_yaw)};
        const Vec3 right{cosf(m_yaw), 0.0f, -sinf(m_yaw)};
        m_target += (fwd * forward + right * strafe) * (speed * dt);
    }

    clampState();
}

void OrbitCamera::clampState() {
    const float twoPi = 2.0f * math3d::kPi;
    if (m_yaw > twoPi) m_yaw -= twoPi;
    if (m_yaw < 0.0f) m_yaw += twoPi;
    m_distance = math3d::clampf(m_distance, kMinDistance, kMaxDistance);
    m_target.x = math3d::clampf(m_target.x, -world::kMapHalfSize, world::kMapHalfSize);
    m_target.z = math3d::clampf(m_target.z, -world::kMapHalfSize, world::kMapHalfSize);

    // La camara nunca baja del suelo: limita la elevacion minima segun la distancia.
    float minPitch = kMinPitch;
    const float needed = (kMinEyeHeight - m_target.y) / m_distance;
    if (needed > -1.0f && needed < 1.0f && asinf(needed) > minPitch) minPitch = asinf(needed);
    m_pitch = math3d::clampf(m_pitch, minPitch, kMaxPitch);
}

Vec3 OrbitCamera::eye() const {
    const float cp = cosf(m_pitch);
    return m_target + Vec3{cp * sinf(m_yaw), sinf(m_pitch), cp * cosf(m_yaw)} * m_distance;
}

math3d::Mat4 OrbitCamera::viewMatrix() const {
    return math3d::Mat4::lookAt(eye(), m_target, {0.0f, 1.0f, 0.0f});
}

}  // namespace camera
