// Previsualizador de host de la escena 3D (no se compila para PSP).
//
// Compila el MISMO codigo del mundo, la camara y las matrices que el juego, pero
// sustituye las funciones 3D de gfx:: por un rasterizador por software que imita a
// la GE: z-buffer, texturas 5551 con muestreo GU_NEAREST + GU_REPEAT, color de
// vertice con GU_TFX_MODULATE, niebla lineal (fogEnd - z_vista) / (fogEnd - fogStart),
// recorte contra el plano cercano y descarte de primitivas fuera de la banda de guarda
// de +-2048 px. Es una aproximacion para revisar el escenario sin hardware: no
// sustituye a probar en PPSSPP o en una PSP.
//
// Uso: tools/scene_preview/build_and_run.sh  ->  build/preview/*.png
#include <math.h>
#include <stdio.h>
#include <string.h>

#include <vector>

#include "app/build_visuals.h"
#include "camera/orbit_camera.h"
#include "camera/third_person_camera.h"
#include "core/config.h"
#include "game/character.h"
#include "game/character_model.h"
#include "game/nav_grid.h"
#include "game/structures.h"
#include "input/input.h"
#include "gfx/graphics.h"
#include "gfx/mesh.h"
#include "gfx/texture.h"
#include "world/world.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

using math3d::Mat4;
using math3d::Vec3;

namespace {

constexpr int kW = gfx::kScreenWidth;
constexpr int kH = gfx::kScreenHeight;
constexpr float kGuardBand = 2048.0f;

struct Raster {
    std::vector<float> rgb = std::vector<float>(kW * kH * 3);
    std::vector<float> depth = std::vector<float>(kW * kH);
    Mat4 proj = Mat4::identity(), view = Mat4::identity(), model = Mat4::identity();
    gfx::FogSettings fog;
    const gfx::Texture* texture = nullptr;
    bool blend = false;

    int trianglesIn = 0;
    int nearClipped = 0;
    int guardBandCulled = 0;
    int badIndices = 0;
} g;

struct ClipVertex {
    float x, y, z, w;     // espacio de recorte
    float u, v;
    float r, gg, b, a;
    float viewDepth;      // distancia positiva delante de la camara
};

void mul4(const Mat4& m, const float in[4], float out[4]) {
    for (int r = 0; r < 4; ++r) {
        out[r] = m.m[r] * in[0] + m.m[4 + r] * in[1] + m.m[8 + r] * in[2] + m.m[12 + r] * in[3];
    }
}

ClipVertex toClip(const gfx::Vertex& v) {
    const Vec3 world = g.model.transformPoint({v.x, v.y, v.z});
    const Vec3 viewPos = g.view.transformPoint(world);
    const float vp[4] = {viewPos.x, viewPos.y, viewPos.z, 1.0f};
    float clip[4];
    mul4(g.proj, vp, clip);
    return {clip[0], clip[1], clip[2], clip[3], v.u, v.v,
            static_cast<float>(v.color & 0xFF), static_cast<float>((v.color >> 8) & 0xFF),
            static_cast<float>((v.color >> 16) & 0xFF), static_cast<float>(v.color >> 24) / 255.0f,
            -viewPos.z};
}

ClipVertex lerpClip(const ClipVertex& a, const ClipVertex& b, float t) {
    auto L = [t](float x, float y) { return x + (y - x) * t; };
    return {L(a.x, b.x), L(a.y, b.y), L(a.z, b.z), L(a.w, b.w), L(a.u, b.u), L(a.v, b.v),
            L(a.r, b.r), L(a.gg, b.gg), L(a.b, b.b), L(a.a, b.a), L(a.viewDepth, b.viewDepth)};
}

void sampleTexture(float u, float v, float out[3]) {
    const gfx::Texture* t = g.texture;
    if (!t) {
        out[0] = out[1] = out[2] = 255.0f;
        return;
    }
    int tx = static_cast<int>(floorf(u * t->texWidth)) % t->texWidth;
    int ty = static_cast<int>(floorf(v * t->texHeight)) % t->texHeight;
    if (tx < 0) tx += t->texWidth;
    if (ty < 0) ty += t->texHeight;
    if (t->psm == GU_PSM_5551) {
        const u16 p = static_cast<const u16*>(t->pixels)[ty * t->stride + tx];
        out[0] = static_cast<float>((p & 31) << 3);
        out[1] = static_cast<float>(((p >> 5) & 31) << 3);
        out[2] = static_cast<float>(((p >> 10) & 31) << 3);
    } else {
        const u32 p = static_cast<const u32*>(t->pixels)[ty * t->stride + tx];
        out[0] = static_cast<float>(p & 0xFF);
        out[1] = static_cast<float>((p >> 8) & 0xFF);
        out[2] = static_cast<float>((p >> 16) & 0xFF);
    }
}

void rasterTriangle(const ClipVertex* v) {
    float sx[3], sy[3], sz[3], invW[3];
    for (int i = 0; i < 3; ++i) {
        invW[i] = 1.0f / v[i].w;
        sx[i] = kW * 0.5f + v[i].x * invW[i] * kW * 0.5f;
        sy[i] = kH * 0.5f - v[i].y * invW[i] * kH * 0.5f;
        sz[i] = v[i].z * invW[i];
        if (fabsf(sx[i] - kW * 0.5f) >= kGuardBand || fabsf(sy[i] - kH * 0.5f) >= kGuardBand) {
            ++g.guardBandCulled;  // la GE descarta la primitiva entera
            return;
        }
    }
    const float area = (sx[1] - sx[0]) * (sy[2] - sy[0]) - (sx[2] - sx[0]) * (sy[1] - sy[0]);
    if (fabsf(area) < 1e-6f) return;

    const int x0 = static_cast<int>(fmaxf(0.0f, floorf(fminf(sx[0], fminf(sx[1], sx[2])))));
    const int x1 = static_cast<int>(fminf(kW - 1.0f, ceilf(fmaxf(sx[0], fmaxf(sx[1], sx[2])))));
    const int y0 = static_cast<int>(fmaxf(0.0f, floorf(fminf(sy[0], fminf(sy[1], sy[2])))));
    const int y1 = static_cast<int>(fminf(kH - 1.0f, ceilf(fmaxf(sy[0], fmaxf(sy[1], sy[2])))));

    for (int py = y0; py <= y1; ++py) {
        for (int px = x0; px <= x1; ++px) {
            const float cx = px + 0.5f, cy = py + 0.5f;
            float b0 = ((sx[1] - cx) * (sy[2] - cy) - (sx[2] - cx) * (sy[1] - cy)) / area;
            float b1 = ((sx[2] - cx) * (sy[0] - cy) - (sx[0] - cx) * (sy[2] - cy)) / area;
            float b2 = 1.0f - b0 - b1;
            if (b0 < 0 || b1 < 0 || b2 < 0) continue;

            const float z = b0 * sz[0] + b1 * sz[1] + b2 * sz[2];
            if (z < -1.0f || z > 1.0f) continue;
            const int idx = py * kW + px;
            if (z >= g.depth[idx]) continue;

            // Interpolacion con correccion de perspectiva.
            const float w0 = b0 * invW[0], w1 = b1 * invW[1], w2 = b2 * invW[2];
            const float norm = 1.0f / (w0 + w1 + w2);
            auto P = [&](float a, float b, float c) { return (a * w0 + b * w1 + c * w2) * norm; };

            float tex[3];
            sampleTexture(P(v[0].u, v[1].u, v[2].u), P(v[0].v, v[1].v, v[2].v), tex);
            float col[3] = {tex[0] * P(v[0].r, v[1].r, v[2].r) / 255.0f,
                            tex[1] * P(v[0].gg, v[1].gg, v[2].gg) / 255.0f,
                            tex[2] * P(v[0].b, v[1].b, v[2].b) / 255.0f};
            if (g.fog.enabled) {
                const float d = P(v[0].viewDepth, v[1].viewDepth, v[2].viewDepth);
                float f = (g.fog.end - d) / (g.fog.end - g.fog.start);
                f = f < 0 ? 0 : (f > 1 ? 1 : f);
                const float fogRgb[3] = {static_cast<float>(g.fog.color & 0xFF),
                                         static_cast<float>((g.fog.color >> 8) & 0xFF),
                                         static_cast<float>((g.fog.color >> 16) & 0xFF)};
                for (int k = 0; k < 3; ++k) col[k] = col[k] * f + fogRgb[k] * (1.0f - f);
            }
            g.depth[idx] = z;
            if (g.blend) {
                const float alpha = P(v[0].a, v[1].a, v[2].a);
                for (int k = 0; k < 3; ++k) {
                    g.rgb[idx * 3 + k] = col[k] * alpha + g.rgb[idx * 3 + k] * (1.0f - alpha);
                }
            } else {
                for (int k = 0; k < 3; ++k) g.rgb[idx * 3 + k] = col[k];
            }
        }
    }
}

void clear(u32 color) {
    for (int i = 0; i < kW * kH; ++i) {
        g.rgb[i * 3 + 0] = static_cast<float>(color & 0xFF);
        g.rgb[i * 3 + 1] = static_cast<float>((color >> 8) & 0xFF);
        g.rgb[i * 3 + 2] = static_cast<float>((color >> 16) & 0xFF);
        g.depth[i] = 1.0f;
    }
}

void savePng(const char* path) {
    std::vector<unsigned char> out(kW * kH * 3);
    for (int i = 0; i < kW * kH * 3; ++i) {
        const float c = g.rgb[i];
        out[i] = static_cast<unsigned char>(c < 0 ? 0 : (c > 255 ? 255 : c));
    }
    stbi_write_png(path, kW, kH, 3, out.data(), kW * 3);
}

}  // namespace

// --- Sustitutos de host de las funciones 3D de gfx:: -------------------------------------
namespace gfx {

void begin3D(const Mat4& projection, const Mat4& view, const FogSettings& fog) {
    g.proj = projection;
    g.view = view;
    g.model = Mat4::identity();
    g.fog = fog;
    g.texture = nullptr;
}
void setModelMatrix(const Mat4& model) { g.model = model; }
void bindTexture(const Texture* texture) { g.texture = texture; }
void setBlend(bool enabled) { g.blend = enabled; }
void drawLines(const LineVertex*, int) {}  // las trazadoras no se previsualizan
void fillRect(float, float, float, float, u32) {}  // el HUD 2D no se previsualiza
void begin2D() {}

void drawMesh(const Mesh& mesh) {
    for (int i = 0; i + 2 < mesh.indexCount; i += 3) {
        ClipVertex tri[3];
        bool bad = false;
        for (int k = 0; k < 3; ++k) {
            const int idx = mesh.indices[i + k];
            if (idx >= mesh.vertexCount) bad = true;
            else tri[k] = toClip(mesh.vertices[idx]);
        }
        if (bad) {
            ++g.badIndices;
            continue;
        }
        ++g.trianglesIn;

        // Recorte Sutherland-Hodgman contra el plano cercano (z >= -w), como GU_CLIP_PLANES.
        ClipVertex poly[4];
        int n = 0;
        bool clipped = false;
        for (int k = 0; k < 3; ++k) {
            const ClipVertex& a = tri[k];
            const ClipVertex& b = tri[(k + 1) % 3];
            const float da = a.z + a.w, db = b.z + b.w;
            if (da >= 0) poly[n++] = a;
            if ((da >= 0) != (db >= 0)) {
                poly[n++] = lerpClip(a, b, da / (da - db));
                clipped = true;
            }
            if (da < 0) clipped = true;
        }
        if (clipped) ++g.nearClipped;
        for (int k = 1; k + 1 < n; ++k) {
            const ClipVertex t[3] = {poly[0], poly[k], poly[k + 1]};
            rasterTriangle(t);
        }
    }
}

}  // namespace gfx

// --- Vistas de prueba ---------------------------------------------------------------------
namespace {

struct View {
    const char* name;
    bool useReset;
    Vec3 target;
    float yaw, pitch, distance;
    bool fog;
};

void renderView(const world::World& w, const View& view, const char* outDir) {
    camera::OrbitCamera cam;
    if (view.useReset) cam.reset();
    else cam.set(view.target, view.yaw, view.pitch, view.distance);

    camera::Projection projection;
    projection.fovYDegrees = config::kFovYDegrees;
    projection.zNear = config::kNearPlane;
    projection.zFar = config::kFarPlane;

    g.trianglesIn = g.nearClipped = g.guardBandCulled = g.badIndices = 0;
    clear(config::kColorSky);

    gfx::FogSettings fog;
    fog.enabled = view.fog;
    fog.start = config::kFogStart;
    fog.end = config::kFogEnd;
    fog.color = config::kColorSky;
    const Mat4 p = projection.matrix(), v = cam.viewMatrix();
    gfx::begin3D(p, v, fog);
    const world::RenderStats stats =
        w.draw(math3d::Frustum::fromMatrix(p * v), cam.eye(), view.fog ? config::kFogEnd : config::kFarPlane);

    // Comprobacion de la proyeccion: el objetivo debe caer en el centro de la pantalla.
    float clip[4];
    const Vec3 tv = v.transformPoint(cam.target());
    const float in[4] = {tv.x, tv.y, tv.z, 1.0f};
    mul4(p, in, clip);
    const float cx = kW * 0.5f + clip[0] / clip[3] * kW * 0.5f;
    const float cy = kH * 0.5f - clip[1] / clip[3] * kH * 0.5f;

    char path[512];
    snprintf(path, sizeof(path), "%s/%s.png", outDir, view.name);
    savePng(path);
    const Vec3 e = cam.eye();
    printf("%-14s ojo=(%6.1f,%5.1f,%6.1f) lotes=%3d tris=%5d recortados=%4d banda_guarda=%3d "
           "idx_malos=%d objetivo_en_pantalla=(%.1f,%.1f)\n",
           view.name, e.x, e.y, e.z, stats.batchesDrawn, stats.trianglesDrawn, g.nearClipped,
           g.guardBandCulled, g.badIndices, cx, cy);
}

}  // namespace

int main(int argc, char** argv) {
    const char* outDir = argc > 1 ? argv[1] : ".";
    world::World w;
    if (!w.build()) {
        fprintf(stderr, "ERROR construyendo el mundo\n");
        return 1;
    }
    printf("mundo: %d lotes, %d triangulos, geometria %zu KB, texturas %zu KB, %d cajas de colision\n",
           w.batchCount(), w.totalTriangles(), w.geometryBytes() / 1024, w.textureBytes() / 1024,
           w.collision().count());

    const float pi = math3d::kPi;
    const View views[] = {
        {"01_deposito_referencia", false, {2.0f, 5.0f, -10.0f}, -0.35f, 0.10f, 52.0f, true},
        {"02_deposito_lateral", false, {-8.0f, 4.0f, -8.0f}, -0.75f, 0.06f, 42.0f, true},
        {"03_cenital", false, {0.0f, 0.0f, 0.0f}, 0.0f, 1.45f, 110.0f, false},
        {"04_meseta", false, {58.0f, 10.0f, -60.0f}, -0.6f, 0.12f, 70.0f, true},
        {"05_lago", false, {-30.0f, 0.0f, 56.0f}, 0.4f, 0.35f, 40.0f, true},
        {"06_bosque", false, {-66.0f, 1.5f, -20.0f}, 0.9f, 0.05f, 6.0f, true},
        {"07_trasera_naves", false, {0.0f, 3.0f, -28.0f}, pi, 0.15f, 30.0f, true},
    };
    for (const View& v : views) renderView(w, v, outDir);

    // --- Vistas de juego: personajes + camara en tercera persona ------------------------
    game::CharacterModel models[4];
    const game::CharacterPalette* palettes[4] = {&game::palettes::kPlayer, &game::palettes::kCommando,
                                                 &game::palettes::kOrange, &game::palettes::kRanger};
    for (int i = 0; i < 4; ++i) {
        if (!models[i].build(*palettes[i])) {
            fprintf(stderr, "ERROR construyendo los modelos\n");
            return 1;
        }
    }
    printf("modelo de personaje: %d triangulos\n", models[0].triangles());

    struct Actor {
        int model;
        Vec3 position;
        float yaw;
        bool aiming, eliminated, running;
    };
    struct GameView {
        const char* name;
        Vec3 player;
        float yaw;
        float look;      // ajuste de la mira (rad) aplicado con la cruceta
        bool orbit;      // camara orbital en lugar de tercera persona
        float orbitYaw, orbitPitch, orbitDistance;
        Actor actors[4];
        int actorCount;
    };
    const GameView gameViews[] = {
        {"11_interior_nave", {0.0f, 0.0f, -4.0f}, 0.0f, 0.0f, false, 0, 0, 0,
         {{1, {3.0f, 0.0f, -14.0f}, pi * 0.8f, true, false, false}, {2, {-4.0f, 0.0f, -17.0f}, 0.4f, false, true, false}}, 2},
        {"12_altillo", {-22.0f, 3.6f, -22.5f}, pi, -0.25f, false, 0, 0, 0,
         {{3, {-20.0f, 0.0f, -8.0f}, 0.2f, true, false, false}, {1, {-24.0f, 0.0f, -2.0f}, 0.0f, false, false, true}}, 2},
        {"13_patio_bots", {0.0f, 0.0f, 24.0f}, 0.0f, 0.0f, false, 0, 0, 0,
         {{1, {-4.0f, 0.0f, 8.0f}, pi, true, false, false}, {2, {9.0f, 0.0f, 3.0f}, pi * 0.9f, false, false, true},
          {3, {-22.0f, 0.0f, 10.0f}, pi * 0.7f, true, false, false}}, 3},
        {"14_escalera", {15.2f, 1.8f, -17.2f}, 0.0f, 0.15f, false, 0, 0, 0, {}, 0},
        {"15_aspectos", {-1.8f, 0.0f, 30.0f}, 0.0f, 0.0f, true, pi * 0.95f, 0.12f, 6.5f,
         {{1, {-0.6f, 0.0f, 30.0f}, 0.0f, false, false, false}, {2, {0.6f, 0.0f, 30.0f}, 0.0f, false, false, false},
          {3, {1.8f, 0.0f, 30.0f}, 0.0f, true, false, false}}, 3},
    };
    for (const GameView& gv : gameViews) {
        Mat4 view;
        Vec3 eye;
        Vec3 orbitTarget = gv.player + Vec3{1.2f, 1.0f, 0.0f};
        if (gv.orbit) {
            camera::OrbitCamera cam;
            cam.set(orbitTarget, gv.orbitYaw, gv.orbitPitch, gv.orbitDistance);
            view = cam.viewMatrix();
            eye = cam.eye();
        } else {
            camera::ThirdPersonCamera cam;
            cam.reset(gv.player, gv.yaw);
            input::Input none;
            // Ajuste de la mira como si se mantuviera la cruceta, y colision con paredes.
            for (int i = 0; i < 60; ++i) {
                cam.update(none, gv.player, gv.yaw, 1.0f / 60.0f, false, nullptr, &w.collision());
            }
            view = cam.viewMatrix();
            if (gv.look != 0.0f) {
                const Vec3 d = cam.lookDirection();
                const float c = cosf(gv.look), sn = sinf(gv.look);
                const Vec3 horiz = math3d::normalize(Vec3{d.x, 0.0f, d.z});
                const Vec3 dir = horiz * c + Vec3{0.0f, sn, 0.0f};
                view = Mat4::lookAt(cam.eye(), cam.eye() + dir, {0.0f, 1.0f, 0.0f});
            }
            eye = cam.eye();
        }
        camera::Projection projection;
        projection.fovYDegrees = config::kFovYDegrees;
        projection.zNear = config::kNearPlane;
        projection.zFar = config::kFarPlane;
        const Mat4 proj = projection.matrix();

        g.trianglesIn = g.nearClipped = g.guardBandCulled = g.badIndices = 0;
        clear(config::kColorSky);
        gfx::FogSettings fog;
        fog.enabled = true;
        fog.start = config::kFogStart;
        fog.end = config::kFogEnd;
        fog.color = config::kColorSky;
        gfx::begin3D(proj, view, fog);
        const world::RenderStats stats = w.draw(math3d::Frustum::fromMatrix(proj * view), eye, config::kFogEnd);

        auto drawActor = [&](int model, const Vec3& pos, float yaw, bool aiming, bool eliminated, bool running) {
            game::CharacterState s;
            s.position = pos;
            s.yaw = yaw;
            s.velocity = running ? game::forwardFromYaw(yaw) * 5.5f : Vec3{0, 0, 0};
            game::CharacterAnim anim;
            for (int i = 0; i < 25; ++i) anim.update(s, aiming, 1.0f / 60.0f);
            models[model].draw(s, anim, w.textures(), eliminated);
        };
        drawActor(0, gv.player, gv.yaw, gv.look != 0.0f, false, false);
        for (int i = 0; i < gv.actorCount; ++i) {
            const Actor& a = gv.actors[i];
            drawActor(a.model, a.position, a.yaw, a.aiming, a.eliminated, a.running);
        }

        char path[512];
        snprintf(path, sizeof(path), "%s/%s.png", outDir, gv.name);
        savePng(path);
        printf("%-18s ojo=(%6.1f,%5.1f,%6.1f) tris=%5d banda_guarda=%3d idx_malos=%d\n", gv.name, eye.x,
               eye.y, eye.z, stats.trianglesDrawn, g.guardBandCulled, g.badIndices);
    }
    // --- Construccion: fuerte de paredes de madera, una danada y la pared fantasma -------
    {
        game::NavGrid noNav;  // sin navegacion: solo colision y dibujo
        game::StructureSystem structures;
        game::BuildConfig build;
        structures.reset(w.mutableCollision(), noNav, build);
        app::BuildVisuals visuals;
        visuals.build(build);
        const Vec3 player{70.0f, 0.0f, 25.0f};
        const world::Aabb playerBox{{69.65f, 0.0f, 24.65f}, {70.35f, 1.8f, 25.35f}};
        const float yaws[] = {pi, pi * 0.5f, -pi * 0.5f};
        for (float yaw : yaws) structures.place(structures.plan(player, yaw, playerBox, nullptr, 0));
        structures.damage(1, 100.0f);  // la oeste, danada
        const game::WallPlacement ghost = structures.plan(player, 0.0f, playerBox, nullptr, 0);
        const game::WallPlacement blocked = structures.plan({58.0f, 0.0f, 25.0f}, 0.0f, playerBox, nullptr, 0);

        camera::OrbitCamera cam;
        cam.set(player + Vec3{0.0f, 1.0f, 0.0f}, pi * 0.82f, 0.32f, 11.0f);
        camera::Projection projection;
        projection.fovYDegrees = config::kFovYDegrees;
        projection.zNear = config::kNearPlane;
        projection.zFar = config::kFarPlane;
        const Mat4 proj = projection.matrix(), view = cam.viewMatrix();
        clear(config::kColorSky);
        gfx::FogSettings fog;
        fog.enabled = true;
        fog.start = config::kFogStart;
        fog.end = config::kFogEnd;
        fog.color = config::kColorSky;
        gfx::begin3D(proj, view, fog);
        w.draw(math3d::Frustum::fromMatrix(proj * view), cam.eye(), config::kFogEnd);
        visuals.drawWalls(structures, w.textures());
        game::CharacterState s;
        s.position = player;
        game::CharacterAnim anim;
        models[0].draw(s, anim, w.textures(), false);
        visuals.drawGhost(ghost);
        char path[512];
        snprintf(path, sizeof(path), "%s/16_construccion.png", outDir);
        savePng(path);
        printf("16_construccion    paredes %d, fantasma %s, fantasma lejos %s\n", structures.count(),
               ghost.valid ? "valida" : "invalida", game::placementErrorName(blocked.error));
        visuals.destroy();
        structures.reset(w.mutableCollision(), noNav, build);
    }

    for (game::CharacterModel& m : models) m.destroy();
    w.destroy();
    return 0;
}
