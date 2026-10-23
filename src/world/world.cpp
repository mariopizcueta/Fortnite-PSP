#include "world/world.h"

#include <algorithm>

#include "gfx/graphics.h"
#include "world/world_units.h"

namespace world {

bool World::build() {
    if (!m_textures.build()) return false;

    SceneBuilder builder(kMaxSegment);
    m_collision.clear();
    buildGround(builder, m_collision, m_static);
    buildStructures(builder, m_collision, m_static, m_pointsOfInterest);
    if (builder.failed()) return false;
    m_collision.build(kMapHalfSize);

    // Agrupa por textura para minimizar cambios de estado al dibujar.
    std::stable_sort(m_static.begin(), m_static.end(),
                     [](const StaticBatch& a, const StaticBatch& b) { return a.texture < b.texture; });

    m_totalTriangles = 0;
    m_geometryBytes = 0;
    for (const StaticBatch& b : m_static) {
        m_totalTriangles += b.triangles;
        m_geometryBytes += sizeof(gfx::Vertex) * b.mesh.vertexCount + sizeof(u16) * b.mesh.indexCount;
    }
    return true;
}

void World::destroy() {
    for (StaticBatch& b : m_static) gfx::destroyMesh(b.mesh);
    m_static.clear();
    m_collision.clear();
    m_pointsOfInterest.clear();
    m_textures.destroy();
}

RenderStats World::draw(const math3d::Frustum& frustum, const math3d::Vec3& eye,
                        float maxDistance) const {
    RenderStats stats;
    gfx::setModelMatrix(math3d::Mat4::identity());
    for (const StaticBatch& b : m_static) {
        if (math3d::length(b.center - eye) - b.radius > maxDistance) continue;
        if (!frustum.sphereVisible(b.center, b.radius)) continue;
        gfx::bindTexture(m_textures.get(b.texture));
        gfx::drawMesh(b.mesh);
        ++stats.batchesDrawn;
        stats.trianglesDrawn += b.triangles;
    }
    return stats;
}

}  // namespace world
