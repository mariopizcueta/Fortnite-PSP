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
    // Madera (tablones), piedra (bloques) y metal (chapa gris), como en Fortnite.
    const int textures[kMaterials] = {world::kTexWoodWall, world::kTexBlockWall, world::kTexMetalSiding};
    const u32 tints[kMaterials] = {rgba(255, 255, 255), rgba(225, 218, 205), rgba(170, 176, 184)};
    world::SceneBuilder b(1.0f);
    for (int m = 0; m < kMaterials; ++m) {
        const world::Material materials[kVariantCount] = {
            {textures[m], tints[m], 2.0f},
            {textures[m], rgba(150, 130, 120), 2.0f},
            {textures[m], rgba(255, 240, 210), 2.0f},
            {world::kTexNone, rgba(90, 170, 255, 110), 1.0f},
            {world::kTexNone, rgba(255, 70, 50, 110), 1.0f},
        };
        auto& meshes = m_meshes[m];
        auto& floors = m_floor[m];
        auto& ramps = m_ramp[m];
        for (int v = 0; v < kVariantCount; ++v) {
            // Caja local con la esquina minima en el origen.
            b.box(materials[v], materials[v], {0, 0, 0}, {g, h, t}, true);
            b.flushGroup(meshes[v][0]);
            b.box(materials[v], materials[v], {0, 0, 0}, {t, h, g}, true);
            b.flushGroup(meshes[v][1]);
            b.box(materials[v], materials[v], {0, 0, 0}, {g, config.floorThickness, g}, true);
            b.flushGroup(floors[v]);
            // Rampa: losa inclinada (la superficie pasa por el centro de cada escalon de
            // colision) de rampThickness de grosor, con sus costados y sus extremos.
            const float d = config.rampThickness;
            const Vec3 lowTop{0, 0, 0}, highTop{g, h, 0}, lowBottom{0, -d, 0}, highBottom{g, h - d, 0};
            const Vec3 side{0, 0, g};
            b.quad(materials[v], lowTop + side, highTop + side, highTop, lowTop);              // superficie
            b.quad(materials[v], lowBottom, highBottom, highBottom + side, lowBottom + side);  // debajo
            b.quad(materials[v], lowBottom, lowTop, highTop, highBottom);                      // costado z = 0
            b.quad(materials[v], highBottom + side, highTop + side, lowTop + side, lowBottom + side);
            b.quad(materials[v], highBottom, highTop, highTop + side, highBottom + side);      // extremo alto
            b.quad(materials[v], lowBottom + side, lowTop + side, lowTop, lowBottom);          // extremo bajo
            b.flushGroup(ramps[v]);
        }
    }
    return !b.failed();
}

void BuildVisuals::destroy() {
    auto release = [](std::vector<world::StaticBatch>& list) {
        for (world::StaticBatch& batch : list) gfx::destroyMesh(batch.mesh);
        list.clear();
    };
    for (int m = 0; m < kMaterials; ++m) {
        for (int v = 0; v < kVariantCount; ++v) {
            release(m_meshes[m][v][0]);
            release(m_meshes[m][v][1]);
            release(m_floor[m][v]);
            release(m_ramp[m][v]);
        }
    }
}

void BuildVisuals::drawBox(int material, const world::Aabb& box, bool alongX, Variant variant,
                           const world::TextureLibrary* textures) const {
    // Misma caja que la colision: traslacion a su esquina y escala en Y si esta recortada.
    const float heightScale = (box.max.y - box.min.y) / m_config.wallHeight;
    gfx::setModelMatrix(Mat4::translation(box.min) * Mat4::scale({1.0f, heightScale, 1.0f}));
    for (const world::StaticBatch& batch : m_meshes[material][variant][alongX ? 0 : 1]) {
        gfx::bindTexture(textures ? textures->get(batch.texture) : nullptr);
        gfx::drawMesh(batch.mesh);
    }
}

void BuildVisuals::drawPiece(int material, game::PieceType type, const world::Aabb& box, bool alongX, int dir, float level,
                             Variant variant, const world::TextureLibrary* textures, float lift) const {
    if (type == game::PieceType::Wall) {
        drawBox(material, box, alongX, variant, textures);
        return;
    }
    const std::vector<world::StaticBatch>* mesh = &m_floor[material][variant];
    Mat4 model = Mat4::translation(box.min + Vec3{0.0f, lift, 0.0f});
    if (type == game::PieceType::Ramp) {
        // La malla sube hacia +X; se gira alrededor del centro de la celda segun `dir`.
        const float half = m_config.gridSize * 0.5f;
        const float angles[4] = {0.0f, math3d::kPi, -math3d::kPi * 0.5f, math3d::kPi * 0.5f};
        model = Mat4::translation({box.min.x + half, level + lift, box.min.z + half}) * Mat4::rotationY(angles[dir & 3]) *
                Mat4::translation({-half, 0.0f, -half});
        mesh = &m_ramp[material][variant];
    }
    gfx::setModelMatrix(model);
    for (const world::StaticBatch& batch : *mesh) {
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
        drawPiece(static_cast<int>(w.material), w.type, w.box, w.alongX, w.dir, w.level, v, &textures);
    }
    gfx::setModelMatrix(Mat4::identity());
}

void BuildVisuals::drawGhost(const game::WallPlacement& placement) const {
    gfx::setBlend(true);
    drawPiece(0, placement.type, placement.box, placement.alongX, placement.dir, placement.level,
              placement.valid ? kGhostValid : kGhostInvalid, nullptr);
    gfx::setBlend(false);
    gfx::setModelMatrix(Mat4::identity());
}

void BuildVisuals::drawHighlight(const game::BuiltWall& wall) const {
    if (wall.type != game::PieceType::Wall) {
        // Suelo y rampa: la misma pieza en rojo, un poco por encima (sin z-fighting).
        gfx::setBlend(true);
        drawPiece(0, wall.type, wall.box, wall.alongX, wall.dir, wall.level, kGhostInvalid, nullptr, 0.04f);
        gfx::setBlend(false);
        gfx::setModelMatrix(Mat4::identity());
        return;
    }
    // Un poco mayor que la pared para que no haya z-fighting con ella.
    world::Aabb box = wall.box;
    box.min = box.min - Vec3{0.03f, 0.03f, 0.03f};
    box.max = box.max + Vec3{0.03f, 0.03f, 0.03f};
    gfx::setBlend(true);
    gfx::setModelMatrix(Mat4::translation(box.min) *
                        Mat4::scale({(box.max.x - box.min.x) / (wall.alongX ? m_config.gridSize : m_config.wallThickness),
                                     (box.max.y - box.min.y) / m_config.wallHeight,
                                     (box.max.z - box.min.z) / (wall.alongX ? m_config.wallThickness : m_config.gridSize)}));
    for (const world::StaticBatch& batch : m_meshes[0][kGhostInvalid][wall.alongX ? 0 : 1]) {
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
