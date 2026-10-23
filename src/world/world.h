// Escenario: geometria visual (lotes de mallas) + geometria de colision (cajas AABB).
//
// Las dos se construyen a la vez en world_layout.cpp, pero son datos independientes:
// el dibujado solo usa los lotes y la fisica / IA / disparos solo usan CollisionWorld.
#pragma once

#include <vector>

#include "math/math3d.h"
#include "world/collision.h"
#include "world/harvest.h"
#include "world/scene_builder.h"
#include "world/textures.h"

namespace world {

struct RenderStats {
    int batchesDrawn = 0;
    int trianglesDrawn = 0;
};

class World {
public:
    // Genera texturas, geometria y colisiones. Devuelve false si falta memoria.
    bool build();
    void destroy();

    // Dibuja los lotes visibles (frustum + distancia maxima). Requiere gfx::begin3D().
    RenderStats draw(const math3d::Frustum& frustum, const math3d::Vec3& eye,
                     float maxDistance) const;

    const CollisionWorld& collision() const { return m_collision; }
    // Para anadir y quitar las paredes construidas (cajas dinamicas) durante la partida.
    CollisionWorld& mutableCollision() { return m_collision; }
    const TextureLibrary& textures() const { return m_textures; }
    // Lugares a los que tienden los bots al explorar (interiores de las naves, patio...).
    const std::vector<math3d::Vec3>& pointsOfInterest() const { return m_pointsOfInterest; }
    // Arboles, cajas, rocas y contenedores que se pueden picar.
    const std::vector<Harvestable>& harvestables() const { return m_harvestables; }
    // Objeto recolectable de una caja fija (RayHit::collider), o -1.
    int harvestableAt(int collider) const {
        return collider >= 0 && collider < static_cast<int>(m_harvestByCollider.size())
                   ? m_harvestByCollider[collider]
                   : -1;
    }

    int batchCount() const { return static_cast<int>(m_static.size()); }
    int totalTriangles() const { return m_totalTriangles; }
    size_t geometryBytes() const { return m_geometryBytes; }
    size_t textureBytes() const { return m_textures.totalBytes(); }

private:
    TextureLibrary m_textures;
    std::vector<StaticBatch> m_static;  // geometria visual, en coordenadas de mundo
    CollisionWorld m_collision;         // geometria de colision
    std::vector<math3d::Vec3> m_pointsOfInterest;
    std::vector<Harvestable> m_harvestables;
    std::vector<short> m_harvestByCollider;  // caja fija -> objeto recolectable (o -1)
    int m_totalTriangles = 0;
    size_t m_geometryBytes = 0;
};

// Color del minimapa en (x, z): tipo de suelo, tejados de las naves, meseta. 0xAABBGGRR.
u32 mapColorAt(float x, float z);

// Contenido del escenario (world_layout.cpp). Cada funcion registra por separado la
// parte visual (SceneBuilder) y la de colision (CollisionWorld).
void buildGround(SceneBuilder& visual, CollisionWorld& collision, std::vector<StaticBatch>& out);
void buildStructures(SceneBuilder& visual, CollisionWorld& collision,
                     std::vector<StaticBatch>& out, std::vector<math3d::Vec3>& pointsOfInterest,
                     std::vector<Harvestable>& harvestables);

}  // namespace world
