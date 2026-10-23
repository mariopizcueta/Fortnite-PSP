// Modelos low-poly del botin, el autobus y el ala delta (Referencias/: cofre, caja de
// municion, airdrop con globo, autobus de batalla, ala delta, armas).
//
// Se construyen una vez con world::SceneBuilder (mismas primitivas y luz horneada que el
// escenario) y se colocan con GU_MODEL. Los objetos del suelo giran y flotan un poco
// sobre un haz de luz del color de su rareza, como en Fortnite.
#pragma once

#include <vector>

#include "game/match.h"
#include "math/math3d.h"
#include "world/scene_builder.h"
#include "world/textures.h"

namespace app {

class PropVisuals {
public:
    bool build();
    void destroy();

    // Cofres, cajas, airdrops y objetos del suelo cercanos (opaco). Requiere gfx::begin3D().
    void drawLoot(const game::Match& match, const math3d::Frustum& frustum, const math3d::Vec3& eye,
                  float maxDistance, float time, const world::TextureLibrary& textures) const;
    // Haces de luz de rareza (translucidos): despues de todo lo opaco.
    void drawBeams(const game::Match& match, const math3d::Frustum& frustum, const math3d::Vec3& eye,
                   float time) const;
    void drawBus(const math3d::Vec3& position, const world::TextureLibrary& textures) const;
    // Ala delta sobre un personaje (pies en `feet`, mirando a `yaw`).
    void drawGlider(const math3d::Vec3& feet, float yaw) const;

    int triangles() const { return m_triangles; }

private:
    using Batches = std::vector<world::StaticBatch>;
    enum Mesh {
        kChest, kChestOpen, kAmmoBox, kAirdrop, kBalloon, kBus, kGlider,
        kRifle, kShotgun, kSmg, kSniper, kBandage, kMedkit, kSmallShield, kShieldPotion,
        kAmmoLight, kAmmoMedium, kAmmoHeavy, kAmmoShells,
        kBeam0, kBeam1, kBeam2, kBeam3, kBeam4,
        kMeshCount
    };

    void draw(Mesh mesh, const math3d::Mat4& model, const world::TextureLibrary* textures) const;
    static Mesh pickupMesh(const game::Pickup& pickup);

    Batches m_meshes[kMeshCount];
    int m_triangles = 0;
};

}  // namespace app
