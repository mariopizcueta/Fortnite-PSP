#include "app/build_visuals.h"

#include <stdio.h>
#include <string.h>

#include "gfx/graphics.h"

namespace app {

using gfx::rgba;
using math3d::Mat4;
using math3d::Vec3;

bool BuildVisuals::build(const game::BuildConfig& config) {
    m_config = config;
    const float g = config.gridSize, h = config.wallHeight, t = config.wallThickness;
    const world::Material materials[kVariantCount] = {
        {world::kTexWoodWall, rgba(255, 255, 255), 2.0f},
        {world::kTexWoodWall, rgba(150, 130, 120), 2.0f},
        {world::kTexWoodWall, rgba(255, 240, 210), 2.0f},
        {world::kTexNone, rgba(90, 170, 255, 110), 1.0f},
        {world::kTexNone, rgba(255, 70, 50, 110), 1.0f},
    };
    world::SceneBuilder b(1.0f);
    for (int v = 0; v < kVariantCount; ++v) {
        // Caja local con la esquina minima en el origen.
        b.box(materials[v], materials[v], {0, 0, 0}, {g, h, t}, true);
        b.flushGroup(m_meshes[v][0]);
        b.box(materials[v], materials[v], {0, 0, 0}, {t, h, g}, true);
        b.flushGroup(m_meshes[v][1]);
    }
    return !b.failed();
}

void BuildVisuals::destroy() {
    for (auto& variant : m_meshes) {
        for (auto& list : variant) {
            for (world::StaticBatch& batch : list) gfx::destroyMesh(batch.mesh);
            list.clear();
        }
    }
}

void BuildVisuals::drawBox(const world::Aabb& box, bool alongX, Variant variant,
                           const world::TextureLibrary* textures) const {
    // Misma caja que la colision: traslacion a su esquina y escala en Y si esta recortada.
    const float heightScale = (box.max.y - box.min.y) / m_config.wallHeight;
    gfx::setModelMatrix(Mat4::translation(box.min) * Mat4::scale({1.0f, heightScale, 1.0f}));
    for (const world::StaticBatch& batch : m_meshes[variant][alongX ? 0 : 1]) {
        gfx::bindTexture(textures ? textures->get(batch.texture) : nullptr);
        gfx::drawMesh(batch.mesh);
    }
}

void BuildVisuals::drawWalls(const game::StructureSystem& structures,
                             const world::TextureLibrary& textures) const {
    for (int i = 0; i < structures.capacity(); ++i) {
        const game::BuiltWall& w = structures.wall(i);
        if (!w.active) continue;
        Variant v = w.health < w.maxHealth * 0.5f ? kDamaged : kNormal;
        if (w.sinceHit < 0.08f) v = kFlash;
        drawBox(w.box, w.alongX, v, &textures);
    }
    gfx::setModelMatrix(Mat4::identity());
}

void BuildVisuals::drawGhost(const game::WallPlacement& placement) const {
    gfx::setBlend(true);
    drawBox(placement.box, placement.alongX, placement.valid ? kGhostValid : kGhostInvalid, nullptr);
    gfx::setBlend(false);
    gfx::setModelMatrix(Mat4::identity());
}

void BuildVisuals::drawHighlight(const game::BuiltWall& wall) const {
    // Un poco mayor que la pared para que no haya z-fighting con ella.
    world::Aabb box = wall.box;
    box.min = box.min - Vec3{0.03f, 0.03f, 0.03f};
    box.max = box.max + Vec3{0.03f, 0.03f, 0.03f};
    gfx::setBlend(true);
    gfx::setModelMatrix(Mat4::translation(box.min) *
                        Mat4::scale({(box.max.x - box.min.x) / (wall.alongX ? m_config.gridSize : m_config.wallThickness),
                                     (box.max.y - box.min.y) / m_config.wallHeight,
                                     (box.max.z - box.min.z) / (wall.alongX ? m_config.wallThickness : m_config.gridSize)}));
    for (const world::StaticBatch& batch : m_meshes[kGhostInvalid][wall.alongX ? 0 : 1]) {
        gfx::bindTexture(nullptr);
        gfx::drawMesh(batch.mesh);
    }
    gfx::setBlend(false);
    gfx::setModelMatrix(Mat4::identity());
}

// --- Numeros grandes -----------------------------------------------------------------------

namespace {

// Fuente de 3x5: cada fila son 3 bits (bit 2 = columna izquierda).
const u8 kDigits[10][5] = {
    {7, 5, 5, 5, 7}, {2, 6, 2, 2, 7}, {7, 1, 7, 4, 7}, {7, 1, 7, 1, 7}, {5, 5, 7, 1, 1},
    {7, 4, 7, 1, 7}, {7, 4, 7, 5, 7}, {7, 1, 1, 1, 1}, {7, 5, 7, 5, 7}, {7, 5, 7, 1, 7},
};

void drawDigit(float x, float y, int digit, float scale, u32 color) {
    for (int row = 0; row < 5; ++row) {
        const u8 bits = kDigits[digit][row];
        // Se agrupan los bloques contiguos de cada fila en un solo rectangulo.
        int col = 0;
        while (col < 3) {
            if (!(bits & (4 >> col))) {
                ++col;
                continue;
            }
            int run = 1;
            while (col + run < 3 && (bits & (4 >> (col + run)))) ++run;
            gfx::fillRect(x + col * scale, y + row * scale, run * scale, scale, color);
            col += run;
        }
    }
}

}  // namespace

int bigNumberWidth(int value, float scale) {
    char text[16];
    snprintf(text, sizeof(text), "%d", value < 0 ? 0 : value);
    const int n = static_cast<int>(strlen(text));
    return static_cast<int>(n * 4 * scale - scale);
}

int drawBigNumber(float x, float y, int value, float scale, u32 color, bool shadow) {
    char text[16];
    snprintf(text, sizeof(text), "%d", value < 0 ? 0 : value);
    float cx = x;
    for (const char* c = text; *c; ++c) {
        const int d = *c - '0';
        if (shadow) {
            const u32 shadowColor = rgba(0, 0, 0, (color >> 24) * 3 / 4);
            drawDigit(cx + scale * 0.5f, y + scale * 0.5f, d, scale, shadowColor);
        }
        drawDigit(cx, y, d, scale, color);
        cx += 4.0f * scale;
    }
    return static_cast<int>(cx - x - scale);
}

}  // namespace app
