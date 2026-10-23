// Jugador: traduce la entrada (stick relativo a la camara, salto, apuntar) a una
// intencion de movimiento para el CharacterBody comun (game/character.h).
#pragma once

#include "game/character.h"

namespace game {

// El jugador usa el mismo estado que cualquier personaje.
using Player = CharacterState;

// Entrada ya interpretada para un frame.
struct PlayerInput {
    float moveX = 0.0f;       // stick: -1 izquierda .. +1 derecha
    float moveY = 0.0f;       // stick: -1 arriba (adelante) .. +1 abajo (atras)
    float cameraYaw = 0.0f;   // el movimiento es relativo a hacia donde mira la camara
    bool jumpPressed = false; // flanco de pulsacion (se guarda unos ms en un buffer)
    bool jumpHeld = false;    // soltar pronto corta el salto (salto variable)
    bool aiming = false;      // disparando o fijando objetivo: el cuerpo mira con la camara
};

// Convierte la entrada del jugador en una intencion de movimiento.
MoveIntent playerIntent(const PlayerInput& input);

// Envoltorio para controlar un CharacterBody directamente con PlayerInput (lo usan las
// pruebas de host; la partida usa playerIntent() sobre su propio cuerpo).
class PlayerController {
public:
    void reset(const math3d::Vec3& spawn, float yaw, const world::CollisionWorld& world);
    void setInput(const PlayerInput& input);
    void step(float dt, const world::CollisionWorld& world);

    const Player& state() const { return m_body.state(); }
    const Player& previous() const { return m_body.previous(); }
    Player interpolated(float alpha) const { return m_body.interpolated(alpha); }

private:
    CharacterBody m_body;
    PlayerInput m_input;
    bool m_pendingJump = false;
};

}  // namespace game
