#include "app/lobby_scene.h"

#include <math.h>

#include <initializer_list>

namespace app {

using gfx::rgba;
using math3d::Mat4;
using math3d::Vec3;

namespace {

constexpr float kHorizonY = 178.0f;  // donde el suelo de arena se junta con la pared

float hash01(int x, int y) {
    unsigned h = static_cast<unsigned>(x) * 374761393u + static_cast<unsigned>(y) * 668265263u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return static_cast<float>((h ^ (h >> 16)) & 0xFFFF) / 65536.0f;
}

void drawBatches(const std::vector<world::StaticBatch>& batches, const world::TextureLibrary* textures) {
    for (const world::StaticBatch& b : batches) {
        gfx::bindTexture(textures ? textures->get(b.texture) : nullptr);
        gfx::drawMesh(b.mesh);
    }
}

}  // namespace

bool LobbyScene::build() {
    world::SceneBuilder b(3.0f);
    // Suelo de arena hasta una "pared" lejana (la linea del horizonte de la referencia).
    const world::Material sand{world::kTexSand, rgba(230, 214, 176), 2.0f};
    b.quad(sand, {-14.0f, 0.0f, 9.0f}, {14.0f, 0.0f, 9.0f}, {14.0f, 0.0f, -3.0f}, {-14.0f, 0.0f, -3.0f});
    b.flushGroup(m_floor);
    // Plataforma octogonal gris claro con la cara superior azul brillante.
    const world::Material rim{world::kTexNone, rgba(214, 220, 226), 1.0f};
    const world::Material top{world::kTexNone, rgba(120, 230, 255), 1.0f};
    b.cylinder(rim, top, {0.0f, 0.0f, 0.0f}, 0.85f, 0.14f, 8, true);
    b.cylinder(rim, rim, {0.0f, 0.0f, 0.0f}, 0.98f, 0.06f, 8, true);
    b.flushGroup(m_pedestal);
    // Cortina de luz semitransparente (como el halo de la referencia).
    const world::Material glow{world::kTexNone, rgba(110, 220, 255, 70), 1.0f};
    b.cylinder(glow, glow, {0.0f, 0.14f, 0.0f}, 0.8f, 0.55f, 8, false);
    b.flushGroup(m_glow);

    // Reticula triangular del fondo: puntos con algo de desorden unidos con sus vecinos.
    constexpr int kCols = 12, kRows = 6;
    Vec3 points[kRows][kCols];
    for (int r = 0; r < kRows; ++r) {
        for (int c = 0; c < kCols; ++c) {
            const float x = -20.0f + c * 46.0f + (r % 2) * 23.0f + (hash01(c, r) - 0.5f) * 14.0f;
            const float y = 30.0f + r * 30.0f + (hash01(r, c + 7) - 0.5f) * 10.0f;
            points[r][c] = {x, y, 0.0f};
        }
    }
    const u32 line = rgba(110, 170, 230, 90);
    auto add = [&](const Vec3& a, const Vec3& c) {
        m_grid.push_back({line, a.x, a.y, 0.0f});
        m_grid.push_back({line, c.x, c.y, 0.0f});
    };
    for (int r = 0; r < kRows; ++r) {
        for (int c = 0; c < kCols; ++c) {
            if (c + 1 < kCols) add(points[r][c], points[r][c + 1]);
            if (r + 1 < kRows) {
                add(points[r][c], points[r + 1][c]);
                const int diag = (r % 2) ? c + 1 : c - 1;
                if (diag >= 0 && diag < kCols) add(points[r][c], points[r + 1][diag]);
            }
        }
    }
    return !b.failed();
}

void LobbyScene::destroy() {
    for (auto* list : {&m_floor, &m_pedestal, &m_glow}) {
        for (world::StaticBatch& batch : *list) gfx::destroyMesh(batch.mesh);
        list->clear();
    }
    m_grid.clear();
}

void LobbyScene::drawBackground(float time) const {
    // Degradado azul en bandas (con el tramado de 16 bits queda suave).
    constexpr int kBands = 12;
    for (int i = 0; i < kBands; ++i) {
        const float t = static_cast<float>(i) / (kBands - 1);
        const u32 c = rgba(static_cast<u32>(18 + 30 * t), static_cast<u32>(52 + 60 * t), static_cast<u32>(118 + 70 * t));
        gfx::fillRect(0, kHorizonY * i / kBands, gfx::kScreenWidth, kHorizonY / kBands + 1, c);
    }
    // La reticula "respira" un poco (alfa) para que el fondo no este muerto.
    (void)time;
    gfx::drawLines2D(m_grid.data(), static_cast<int>(m_grid.size()));
}

void LobbyScene::drawScene(const game::CharacterModel& model, const game::CharacterAnim& anim,
                           const world::TextureLibrary& textures, float characterYaw, float time,
                           Mat4* viewProjection) const {
    const Mat4 projection = Mat4::perspective(math3d::toRadians(42.0f), 480.0f / 272.0f, 0.5f, 40.0f);
    const Vec3 eye{1.1f, 1.55f, 4.6f};
    const Mat4 view = Mat4::lookAt(eye, {1.1f, 1.05f, 0.0f}, {0.0f, 1.0f, 0.0f});
    gfx::FogSettings fog;
    fog.enabled = false;
    gfx::begin3D(projection, view, fog);
    drawBatches(m_floor, &textures);
    drawBatches(m_pedestal, nullptr);

    game::CharacterState s;
    s.position = {0.0f, 0.14f, 0.0f};
    s.yaw = characterYaw;
    model.draw(s, anim, textures, false);

    gfx::setModelMatrix(Mat4::identity());
    gfx::setBlend(true);
    // El halo late suavemente.
    const float pulse = 1.0f + 0.06f * sinf(time * 3.0f);
    gfx::setModelMatrix(Mat4::scale({pulse, 1.0f, pulse}));
    drawBatches(m_glow, nullptr);
    gfx::setBlend(false);
    gfx::setModelMatrix(Mat4::identity());
    if (viewProjection) *viewProjection = projection * view;
}

}  // namespace app
