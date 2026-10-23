// Camara en tercera persona: detras y por encima del jugador, con seguimiento suave,
// colision con paredes y fijacion de objetivo.
//
//   pivote = posicion suavizada del jugador + (0, kPivotHeight, 0)
//   ojo    = pivote + atras(yaw) * D*cos(o) + (0, D*sin(o), 0)       (D = 5 m, o = 0.24 rad)
//   mira   = direccion (yaw, look): el centro de la pantalla
//
// El ojo esta por encima de la cabeza y la mira apunta casi horizontal (look ~ -4 grados),
// asi el personaje queda debajo de la mira (como en Referencias/Referencia gameplay.png)
// y la mira senala a la altura del pecho de los enemigos. Mismo convenio de yaw que el
// jugador: la camara mira hacia (-sin(yaw), 0, -cos(yaw)).
//
// Si hay una pared entre el pivote y el ojo deseado, el ojo se acerca al jugador (dentro
// de las naves la camara nunca queda al otro lado de la pared).
#pragma once

#include "input/input.h"
#include "math/math3d.h"
#include "world/collision.h"

namespace camera {

class ThirdPersonCamera {
public:
    void reset(const math3d::Vec3& playerPosition, float playerYaw);

    // dt = tiempo de frame (render). Cruceta: girar (izq/dcha) y mirar arriba/abajo.
    // `recenter`: volver detras del jugador. `lockTarget`: punto a encarar (o nullptr).
    // `world`: colision de la camara con paredes (o nullptr).
    void update(const input::Input& in, const math3d::Vec3& playerPosition, float playerYaw,
                float dt, bool recenter, const math3d::Vec3* lockTarget,
                const world::CollisionWorld* world);

    math3d::Vec3 eye() const { return m_eye; }
    math3d::Vec3 focus() const { return m_pivot; }
    math3d::Vec3 lookDirection() const;
    math3d::Mat4 viewMatrix() const;

    // Yaw efectivo (suavizado): define "adelante" para el movimiento del jugador.
    float yaw() const { return m_yaw; }
    float pitch() const { return m_look; }
    float distance() const { return math3d::length(m_eye - m_pivot); }

private:
    void computeEye(const world::CollisionWorld* world);

    math3d::Vec3 m_pivot;
    math3d::Vec3 m_eye;
    float m_yaw = 0.0f, m_targetYaw = 0.0f;
    float m_look = 0.0f, m_targetLook = 0.0f;
};

}  // namespace camera
