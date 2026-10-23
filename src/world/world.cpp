#include "world/world.h"

#include <algorithm>

#include "gfx/graphics.h"
#include "world/world_units.h"

namespace world {

const char* resourceName(Resource resource) {
    switch (resource) {
        case Resource::Wood: return "madera";
        case Resource::Stone: return "piedra";
        case Resource::Metal: return "metal";
        case Resource::Count: break;
    }
    return "?";
}

bool World::build() {
    if (!m_textures.build()) return false;

    SceneBuilder builder(kMaxSegment);
    m_collision.clear();
    buildGround(builder, m_collision, m_static);
    m_harvestables.clear();
    buildStructures(builder, m_collision, m_static, m_pointsOfInterest, m_harvestables);
    buildStormWall(builder, m_stormWall);
    if (builder.failed()) return false;
    m_collision.build(kMapHalfSize);
    m_harvestByCollider.assign(m_collision.count(), -1);
    for (int i = 0; i < static_cast<int>(m_harvestables.size()); ++i) {
        m_harvestByCollider[m_harvestables[i].collider] = static_cast<short>(i);
    }

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
    for (StaticBatch& b : m_stormWall) gfx::destroyMesh(b.mesh);
    m_stormWall.clear();
    m_collision.clear();
    m_pointsOfInterest.clear();
    m_harvestables.clear();
    m_harvestByCollider.clear();
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

RenderStats World::drawStormWall(const math3d::Frustum& frustum, const math3d::Vec3& eye,
                                 float maxDistance) const {
    RenderStats stats;
    gfx::setModelMatrix(math3d::Mat4::identity());
    gfx::bindTexture(nullptr);
    gfx::setBlend(true);
    for (const StaticBatch& b : m_stormWall) {
        if (math3d::length(b.center - eye) - b.radius > maxDistance) continue;
        if (!frustum.sphereVisible(b.center, b.radius)) continue;
        gfx::drawMesh(b.mesh);
        ++stats.batchesDrawn;
        stats.trianglesDrawn += b.triangles;
    }
    gfx::setBlend(false);
    return stats;
}

}  // namespace world
