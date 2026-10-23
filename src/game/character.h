// Fisica comun de los personajes (jugador y bots): andar, saltar, gravedad y colisiones.
//
// Unidades: metros, segundos, radianes. La posicion es la de los PIES.
// Orientacion (yaw): el personaje mira hacia (-sin(yaw), 0, -cos(yaw)); yaw = 0 mira al
// norte (-Z). Es la misma convencion que el yaw de las camaras.
//
// Cuerpo de colision: caja de 2*kBodyRadius x kBodyHeight centrada en los pies.
// Las cajas del mundo con kBlocksMovement lo detienen eje a eje (X y luego Z). Los
// obstaculos de hasta kStepHeight se suben solos (escalones, bordillos); los mas altos
// bloquean. En vertical se apoya en el techo de la caja mas alta bajo los pies.
#pragma once

#include "math/math3d.h"
#include "world/collision.h"

namespace game {

enum class JumpState { Grounded, Rising, Falling };

struct CharacterState {
    math3d::Vec3 position;  // pies, en coordenadas de mundo
    math3d::Vec3 velocity;  // m/s
    float yaw = 0.0f;
    float health = 100.0f;
    float maxHealth = 100.0f;
    float shield = 0.0f;      // absorbe el dano antes que la vida (pociones de escudo)
    float maxShield = 100.0f;
    JumpState jumpState = JumpState::Grounded;
    float airTime = 0.0f;   // segundos desde que dejo el suelo (0 en el suelo)

    bool grounded() const { return jumpState == JumpState::Grounded; }
    float horizontalSpeed() const;
};

// Lo que el personaje quiere hacer en este paso (lo rellena la entrada o la IA).
struct MoveIntent {
    math3d::Vec3 wish;        // direccion deseada en XZ, longitud <= 1 (0 = quieto)
    float speedScale = 1.0f;  // fraccion de kRunSpeed
    bool jumpPressed = false;
    bool jumpHeld = false;
    bool faceYawOverride = false;  // true: mirar a faceYaw (apuntar) en vez de hacia donde anda
    float faceYaw = 0.0f;
};

namespace tuning {
constexpr float kRunSpeed = 5.5f;        // m/s con el stick al maximo
constexpr float kGroundAccel = 45.0f;    // m/s^2 hacia la velocidad deseada
constexpr float kGroundBrake = 60.0f;    // m/s^2 al soltar el stick
constexpr float kAirAccel = 12.0f;       // control en el aire
constexpr float kGravity = 24.0f;        // m/s^2 (mas que 9.8: salto "de juego", agil)
constexpr float kJumpSpeed = 8.0f;       // m/s -> altura maxima ~1.27 m
constexpr float kJumpCutFactor = 0.5f;   // velocidad vertical que queda al soltar X subiendo
constexpr float kMaxFallSpeed = 40.0f;   // velocidad terminal
constexpr float kTurnRate = 12.0f;       // rad/s para girar el modelo
constexpr float kJumpBuffer = 0.12f;     // s que se recuerda una pulsacion de salto
constexpr float kCoyoteTime = 0.10f;     // s en los que aun se puede saltar tras dejar el suelo
constexpr float kMapMargin = 0.5f;       // distancia minima al borde del mapa

constexpr float kBodyRadius = 0.35f;     // media anchura de la caja del cuerpo
constexpr float kBodyHeight = 1.8f;
constexpr float kStepHeight = 0.45f;     // escalon maximo que se sube andando
constexpr float kFootRadius = 0.30f;     // huella para apoyarse (algo menor que el cuerpo)
constexpr float kEyeHeight = 1.55f;      // ojos (linea de vision de la IA)
constexpr float kChestHeight = 1.25f;    // punto al que se apunta
}  // namespace tuning

// Cuerpo simulado con paso fijo: estado actual, anterior (para interpolar) y memoria
// del salto (buffer, coyote).
class CharacterBody {
public:
    void reset(const math3d::Vec3& spawn, float yaw, const world::CollisionWorld& world);
    // Un paso de duracion fija. La pulsacion de salto se guarda en un buffer.
    void step(const MoveIntent& intent, float dt, const world::CollisionWorld& world);

    CharacterState& state() { return m_state; }
    const CharacterState& state() const { return m_state; }
    const CharacterState& previous() const { return m_previous; }
    CharacterState interpolated(float alpha) const;

    // Caja de colision del cuerpo en la posicion actual.
    world::Aabb bounds() const;
    // Empuja el cuerpo fuera de cualquier caja que lo solape (p. ej. tras separarlo de
    // otro personaje). Devuelve true si se ha movido.
    bool depenetrate(const world::CollisionWorld& world);

private:
    void moveAxis(int axis, float delta, const world::CollisionWorld& world);

    CharacterState m_state;
    CharacterState m_previous;
    float m_jumpBuffer = 0.0f;
    float m_coyote = 0.0f;
    bool m_jumpCut = false;
};

// Utilidades de angulos.
float wrapAngle(float radians);  // a (-pi, pi]
float approachAngle(float from, float to, float maxDelta);
// Yaw que mira desde `from` hacia `to` (en XZ).
float yawTowards(const math3d::Vec3& from, const math3d::Vec3& to);
math3d::Vec3 forwardFromYaw(float yaw);

const char* jumpStateName(JumpState state);
const char* locomotionName(const CharacterState& state);

}  // namespace game
