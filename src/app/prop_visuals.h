// Modelos low-poly del botin, el autobus y el ala delta (Referencias/: cofre, caja de
// municion, airdrop con globo, autobus de batalla, ala delta, armas).
//
// Se construyen una vez con world::SceneBuilder (mismas primitivas y luz horneada que el
// escenario) y se colocan con GU_MODEL. Los objetos del suelo giran y flotan un poco
// sobre un haz de luz del color de su rareza, como en Fortnite.
#pragma once

#include <vector>

#include "game/item_models.h"
#include "game/match.h"
#include "math/math3d.h"
#include "world/scene_builder.h"
#include "world/textures.h"

namespace app {

class PropVisuals {
public:
    bool build();
    void destroy();

    // Cofres, cajas, airdrops, la llama y objetos del suelo cercanos (opaco; las armas y
    // consumibles con los modelos de `items`). Requiere gfx::begin3D().
    void drawLoot(const game::Match& match, const math3d::Frustum& frustum, const math3d::Vec3& eye,
                  float maxDistance, float time, const world::TextureLibrary& textures,
                  const game::ItemModels& items) const;
    // Cohetes y granadas en vuelo (opaco) y explosiones (translucidas, al final).
    void drawProjectiles(const game::Match& match, const math3d::Frustum& frustum, const game::ItemModels& items) const;
    void drawExplosions(const game::Match& match, const math3d::Frustum& frustum) const;
    // Trampas colocadas (los pinchos salen al saltar) y la trampa fantasma al construir.
    void drawTraps(const game::Match& match, const math3d::Frustum& frustum, const math3d::Vec3& eye,
                   float maxDistance) const;
    void drawTrapGhost(const game::TrapPlacement& placement) const;
    // Haces de luz de rareza (translucidos): despues de todo lo opaco.
    void drawBeams(const game::Match& match, const math3d::Frustum& frustum, const math3d::Vec3& eye,
                   float time) const;
    void drawBus(const math3d::Vec3& position, const world::TextureLibrary& textures) const;
    // Ala delta sobre un personaje (pies en `feet`, mirando a `yaw`).
    void drawGlider(const math3d::Vec3& feet, float yaw) const;
    // Saco de dormir extendido en el suelo (el jugador duerme en el, v0.15).
    void drawSleepingBag(const math3d::Vec3& feet, float yaw) const;

    int triangles() const { return m_triangles; }

private:
    using Batches = std::vector<world::StaticBatch>;
    enum Mesh {
        kChest, kChestOpen, kAmmoBox, kAirdrop, kBalloon, kBus, kGlider, kLlama,
        kAmmoLight, kAmmoMedium, kAmmoHeavy, kAmmoShells, kAmmoRockets,
        kMatWood, kMatStone, kMatMetal,
        kBeam0, kBeam1, kBeam2, kBeam3, kBeam4,
        kBlast0, kBlast1, kBlast2,  // explosion: tres niveles de transparencia
        kTrapPlate, kTrapSpikes, kTrapGhostValid, kTrapGhostInvalid,
        kBlueprint,  // v0.14: plano enrollado (modo zombi)
        // v0.15: nevera y despensa (cerradas / abiertas), cama, mochila y saco extendido.
        kFridge, kFridgeOpen, kPantry, kPantryOpen, kBed, kBackpack, kSleepingBag,
        kMeshCount
    };

    void draw(Mesh mesh, const math3d::Mat4& model, const world::TextureLibrary* textures) const;

    Batches m_meshes[kMeshCount];
    int m_triangles = 0;
};

}  // namespace app
