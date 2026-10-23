// Camara orbital de depuracion: gira alrededor de un punto objetivo sobre el suelo.
//
//   eye = target + distance * (cos(pitch) * sin(yaw), sin(pitch), cos(pitch) * cos(yaw))
//
// yaw = 0 coloca la camara al sur del objetivo (+Z) mirando al norte (-Z); yaw crece
// hacia el este. pitch es la elevacion sobre el horizonte.
#pragma once

#include "input/input.h"
#include "math/math3d.h"

namespace camera {

struct Projection {
    float fovYDegrees = 60.0f;
    float aspect = 480.0f / 272.0f;
    float zNear = 1.0f;
    float zFar = 140.0f;

    math3d::Mat4 matrix() const;
};

class OrbitCamera {
public:
    void reset();
    // Stick: orbitar. Cruceta: mover el objetivo. R/L: acercar/alejar.
    void update(const input::Input& in, float dt);
    // Coloca la camara directamente (se aplican los mismos limites que en update()).
    void set(const math3d::Vec3& target, float yaw, float pitch, float distance);

    math3d::Vec3 eye() const;
    math3d::Vec3 target() const { return m_target; }
    math3d::Mat4 viewMatrix() const;

    float yaw() const { return m_yaw; }
    float pitch() const { return m_pitch; }
    float distance() const { return m_distance; }

private:
    void clampState();

    math3d::Vec3 m_target;
    float m_yaw = 0.0f;
    float m_pitch = 0.0f;
    float m_distance = 0.0f;
};

}  // namespace camera
