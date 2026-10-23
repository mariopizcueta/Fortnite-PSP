// Escena del lobby (Referencias/Lobby.png): fondo azul con reticula triangular, suelo de
// arena y el personaje del jugador sobre una plataforma octogonal con luz azul.
//
// Todo se construye una vez: mallas de suelo y plataforma, y la reticula de fondo como
// una lista fija de lineas 2D.
#pragma once

#include <vector>

#include "game/character_model.h"
#include "gfx/graphics.h"
#include "world/scene_builder.h"
#include "world/textures.h"

namespace app {

class LobbyScene {
public:
    bool build();
    void destroy();

    // Fondo 2D (degradado y reticula). Llamar antes de la parte 3D.
    void drawBackground(float time) const;
    // Suelo, plataforma y personaje. Deja en `viewProjection` la matriz usada (para
    // proyectar el nombre sobre la cabeza).
    void drawScene(const game::CharacterModel& model, const game::CharacterAnim& anim,
                   const world::TextureLibrary& textures, float characterYaw, float time,
                   math3d::Mat4* viewProjection) const;

    // Punto sobre la cabeza del personaje (para el nombre).
    static math3d::Vec3 namePoint() { return {0.0f, 2.25f, 0.0f}; }

private:
    std::vector<world::StaticBatch> m_floor;
    std::vector<world::StaticBatch> m_pedestal;
    std::vector<world::StaticBatch> m_glow;
    std::vector<gfx::LineVertex> m_grid;
};

}  // namespace app
