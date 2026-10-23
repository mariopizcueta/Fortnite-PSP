#include "game/player.h"

#include <math.h>

namespace game {

MoveIntent playerIntent(const PlayerInput& input) {
    MoveIntent intent;
    const float cy = cosf(input.cameraYaw), sy = sinf(input.cameraYaw);
    const math3d::Vec3 forward{-sy, 0.0f, -cy};
    const math3d::Vec3 right{cy, 0.0f, -sy};
    intent.wish = forward * (-input.moveY) + right * input.moveX;
    intent.jumpPressed = input.jumpPressed;
    intent.jumpHeld = input.jumpHeld;
    // Apuntando, el cuerpo se orienta con la camara (se puede andar de lado o de espaldas).
    intent.faceYawOverride = input.aiming;
    intent.faceYaw = input.cameraYaw;
    return intent;
}

void PlayerController::reset(const math3d::Vec3& spawn, float yaw, const world::CollisionWorld& world) {
    m_body.reset(spawn, yaw, world);
    m_input = PlayerInput{};
    m_pendingJump = false;
}

void PlayerController::setInput(const PlayerInput& input) {
    m_input = input;
    // La pulsacion se entrega en el siguiente paso aunque este frame no tenga ninguno.
    if (input.jumpPressed) m_pendingJump = true;
}

void PlayerController::step(float dt, const world::CollisionWorld& world) {
    PlayerInput in = m_input;
    in.jumpPressed = m_pendingJump;
    m_pendingJump = false;
    m_body.step(playerIntent(in), dt, world);
}

}  // namespace game
