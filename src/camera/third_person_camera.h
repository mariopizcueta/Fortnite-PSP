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
//
// Control (fase 7): la cruceta ya no mueve la camara. Al andar, la camara se coloca sola
// detras del jugador (`follow`); en el modo apuntado (L sin objetivo) el stick la gira y
// la sube o baja (`turn`, `look`).
#pragma once

#include "math/math3d.h"
#include "world/collision.h"

namespace camera {

struct CameraInput {
    float turn = 0.0f;       // -1..1: + gira a la derecha
    float look = 0.0f;       // -1..1: + mira hacia arriba
    float followYaw = 0.0f;  // hacia donde mira el jugador
    float follow = 0.0f;     // 0..1: fuerza con la que la camara se coloca detras de el
    float distance = 5.0f;   // m del pivote al ojo (mas lejos en el autobus y cayendo)
    float restLook = -0.065f;  // inclinacion a la que vuelve la mira al seguir (rad)
};

class ThirdPersonCamera {
public:
    void reset(const math3d::Vec3& playerPosition, float playerYaw);

    // dt = tiempo de frame (render). `control`: giro manual y seguimiento automatico.
    // `recenter`: volver detras del jugador. `lockTarget`: punto a encarar (o nullptr).
    // `world`: colision de la camara con paredes (o nullptr).
    void update(const CameraInput& control, const math3d::Vec3& playerPosition, float playerYaw,
                float dt, bool recenter, const math3d::Vec3* lockTarget,
                const world::CollisionWorld* world);

    // Orienta la camara al instante (sin suavizado). `look` se limita a su rango.
    void setAim(float yaw, float look);
    // Orienta la camara para que su rayo (desde su propio ojo, que depende de la
    // orientacion) pase por `point`. Sirve para que la mira siga en el mismo sitio al
    // quitar la mirilla.
    void aimAt(const math3d::Vec3& point, const world::CollisionWorld* world);
    // Retroceso: la mira sube `radians` y vuelve sola en unas decimas.
    void kick(float radians) { m_kick += radians; }

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
    float m_distance = 5.0f;
    float m_kick = 0.0f;  // inclinacion extra del retroceso (rad), decae sola
};

}  // namespace camera
