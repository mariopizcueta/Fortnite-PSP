// Previsualizador de host de la escena 3D (no se compila para PSP).
//
// Compila el MISMO codigo del mundo, la camara y las matrices que el juego, pero
// sustituye gfx:: por el rasterizador por software de tools/host/soft_gfx.cpp, que imita
// a la GE (z-buffer, texturas con GU_NEAREST, niebla lineal, recorte contra el plano
// cercano, banda de guarda). Es una aproximacion para revisar el escenario sin hardware:
// no sustituye a probar en PPSSPP o en una PSP.
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
#include "game/item_models.h"
#include "game/nav_grid.h"
#include "game/structures.h"
#include "input/input.h"
#include "gfx/graphics.h"
#include "gfx/mesh.h"
#include "gfx/texture.h"
#include "world/world.h"

#include "soft_gfx.h"

using math3d::Mat4;
using math3d::Vec3;
using softgfx::clear;
using softgfx::g;
using softgfx::kH;
using softgfx::kW;
using softgfx::mul4;
using softgfx::savePng;

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
    game::ItemModels items;
    if (!items.build()) {
        fprintf(stderr, "ERROR construyendo los modelos de armas\n");
        return 1;
    }

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
            const camera::CameraInput none;
            // Un segundo de seguimiento (camara asentada) con colision con paredes.
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
            for (int i = 0; i < 25; ++i) anim.update(s, aiming, true, 1.0f / 60.0f);
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
    // --- Personajes de cerca: de frente, de espaldas y primer plano de la cara ------------
    {
        struct Closeup {
            const char* name;
            Vec3 eye, target;
            float yaw;  // hacia donde miran los personajes
            int count;
        };
        const Closeup closeups[] = {
            {"17_personajes_frente", {0.0f, 1.35f, 34.6f}, {0.0f, 0.95f, 30.0f}, pi, 4},
            {"18_personajes_espalda", {0.0f, 1.35f, 34.6f}, {0.0f, 0.95f, 30.0f}, 0.0f, 4},
            {"19_cara_primer_plano", {0.25f, 1.72f, 31.3f}, {0.15f, 1.62f, 30.0f}, pi * 0.94f, 1},
        };
        const Mat4 proj = Mat4::perspective(math3d::toRadians(config::kFovYDegrees), 480.0f / 272.0f, 0.2f, 140.0f);
        for (const Closeup& c : closeups) {
            clear(config::kColorSky);
            const Mat4 view = Mat4::lookAt(c.eye, c.target, {0.0f, 1.0f, 0.0f});
            gfx::FogSettings fog;
            fog.enabled = true;
            fog.start = config::kFogStart;
            fog.end = config::kFogEnd;
            fog.color = config::kColorSky;
            gfx::begin3D(proj, view, fog);
            w.draw(math3d::Frustum::fromMatrix(proj * view), c.eye, config::kFogEnd);
            // Jugador apuntando con el fusil, escopeta corriendo, subfusil agachado y
            // francotirador bailando (el baile no lleva nada en la mano).
            const game::ItemModel held[4] = {game::ItemModel::Scar, game::ItemModel::PumpShotgun,
                                             game::ItemModel::SuppressedSmg, game::ItemModel::None};
            const game::BodyPose poses[4] = {game::BodyPose::Normal, game::BodyPose::Normal, game::BodyPose::Crouch,
                                             game::BodyPose::Dance};
            for (int i = 0; i < c.count; ++i) {
                game::CharacterState s;
                s.position = {c.count == 1 ? 0.15f : -1.65f + 1.1f * i, 0.0f, 30.0f};
                s.yaw = c.yaw;
                s.velocity = i == 1 ? game::forwardFromYaw(c.yaw) * 5.5f : Vec3{0, 0, 0};
                game::CharacterAnim anim;
                for (int k = 0; k < 40; ++k) anim.update(s, i == 0 && c.count > 1, true, 1.0f / 60.0f);
                models[i].draw(s, anim, w.textures(), false, held[i], false,
                               c.count == 1 ? game::BodyPose::Normal : poses[i], &items);
            }
            char path[512];
            snprintf(path, sizeof(path), "%s/%s.png", outDir, c.name);
            savePng(path, true);
            printf("%-22s modelo %d triangulos (lejos %d)\n", c.name, models[0].triangles(),
                   models[0].lowDetailTriangles());
        }
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

    // --- Parche (fase 8): galeria de armas y objetos, y cada arma en la mano ----------------
    {
        const Mat4 proj = Mat4::perspective(math3d::toRadians(40.0f), 480.0f / 272.0f, 0.1f, 50.0f);
        const game::ItemModel gallery[] = {
            game::ItemModel::AssaultRifle, game::ItemModel::Scar,          game::ItemModel::BurstRifle,
            game::ItemModel::ScopedRifle,  game::ItemModel::SuppressedSmg, game::ItemModel::Smg,
            game::ItemModel::PumpShotgun,  game::ItemModel::TacticalShotgun, game::ItemModel::Pistol,
            game::ItemModel::Revolver,     game::ItemModel::BoltSniper,    game::ItemModel::SemiAutoSniper,
            game::ItemModel::RocketLauncher, game::ItemModel::GrenadeLauncher, game::ItemModel::Grenade,
            game::ItemModel::Pickaxe,      game::ItemModel::Bandage,       game::ItemModel::Medkit,
            game::ItemModel::SmallShield,  game::ItemModel::ShieldPotion,  game::ItemModel::Rocket,
        };
        const int n = sizeof(gallery) / sizeof(gallery[0]);
        // Paginas de 6 (3 x 2), de perfil y algo girados, para verlos de cerca.
        int tris = 0;
        char path[512];
        for (int page = 0; page * 6 < n; ++page) {
            clear(gfx::rgba(40, 60, 110));
            const Mat4 view = Mat4::lookAt({0.0f, 0.0f, 2.6f}, {0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f});
            gfx::FogSettings none;
            none.enabled = false;
            gfx::begin3D(proj, view, none);
            for (int k = 0; k < 6 && page * 6 + k < n; ++k) {
                const game::ItemModel model = gallery[page * 6 + k];
                const float x = -0.75f + 0.75f * (k % 3), y = 0.32f - 0.62f * (k / 3);
                const Mat4 m = Mat4::translation({x, y, 0.0f}) * Mat4::rotationY(-0.5f * pi + 0.45f) *
                               Mat4::rotationX(0.2f) * Mat4::scale({0.85f, 0.85f, 0.85f}) *
                               Mat4::translation(items.info(model).center * -1.0f);
                items.draw(model, m);
                tris += items.triangles(model);
            }
            snprintf(path, sizeof(path), "%s/20_armas_%d.png", outDir, page + 1);
            savePng(path, true);
        }
        printf("20_armas_N         %d modelos, %d triangulos en total\n", n, tris);

        // Cada arma en la mano del personaje (apuntando y corriendo).
        const game::ItemModel held[] = {game::ItemModel::AssaultRifle, game::ItemModel::BurstRifle,
                                        game::ItemModel::Smg,          game::ItemModel::TacticalShotgun,
                                        game::ItemModel::Pistol,       game::ItemModel::SemiAutoSniper,
                                        game::ItemModel::RocketLauncher, game::ItemModel::GrenadeLauncher};
        const Mat4 proj2 = Mat4::perspective(math3d::toRadians(config::kFovYDegrees), 480.0f / 272.0f, 0.2f, 140.0f);
        for (int pass = 0; pass < 2; ++pass) {
            clear(config::kColorSky);
            const Vec3 eye{pass == 0 ? 2.6f : -2.6f, 1.6f, 34.4f}, target{0.0f, 1.0f, 30.0f};
            const Mat4 v = Mat4::lookAt(eye, target, {0.0f, 1.0f, 0.0f});
            gfx::FogSettings fog;
            fog.enabled = true;
            fog.start = config::kFogStart;
            fog.end = config::kFogEnd;
            fog.color = config::kColorSky;
            gfx::begin3D(proj2, v, fog);
            w.draw(math3d::Frustum::fromMatrix(proj2 * v), eye, config::kFogEnd);
            for (int i = 0; i < 8; ++i) {
                game::CharacterState s;
                s.position = {-3.5f + 1.0f * i, 0.0f, 30.0f - 0.4f * (i % 2)};
                s.yaw = pi * 0.85f;
                s.velocity = pass == 1 ? game::forwardFromYaw(s.yaw) * 5.5f : Vec3{0, 0, 0};
                game::CharacterAnim anim;
                for (int k = 0; k < 40; ++k) anim.update(s, pass == 0, true, 1.0f / 60.0f);
                models[i % 4].draw(s, anim, w.textures(), false, held[i], false, game::BodyPose::Normal, &items);
            }
            snprintf(path, sizeof(path), "%s/21_armas_en_mano_%s.png", outDir, pass == 0 ? "apuntando" : "corriendo");
            savePng(path, true);
            printf("21_armas_en_mano_%s\n", pass == 0 ? "apuntando" : "corriendo");
        }
    }

    // --- Parche (fase 8): hoja de posturas con cada tipo de arma ------------------------------
    {
        struct Sheet {
            const char* name;
            game::ItemModel held;
        };
        const Sheet sheets[] = {{"22_posturas_fusil", game::ItemModel::Scar},
                                {"22_posturas_pistola", game::ItemModel::Pistol},
                                {"22_posturas_lanzacohetes", game::ItemModel::RocketLauncher},
                                {"22_posturas_escopeta", game::ItemModel::PumpShotgun},
                                {"22_posturas_sin_arma", game::ItemModel::None},
                                {"22_posturas_curativo", game::ItemModel::ShieldPotion}};
        // Columnas: quieto, corriendo, apuntando, apuntando arriba, agachado apuntando,
        // recargando, saltando (subiendo), cayendo.
        const int kPoses = 8;
        const Mat4 proj = Mat4::perspective(math3d::toRadians(config::kFovYDegrees), 480.0f / 272.0f, 0.2f, 140.0f);
        // Dos imagenes por arma (cuatro posturas cada una), de frente-lado y de cerca.
        for (int half = 0; half < 2; ++half) {
            for (const Sheet& sh : sheets) {
                clear(config::kColorSky);
                const Vec3 eye{0.9f, 1.25f, 33.6f};
                const Mat4 v = Mat4::lookAt(eye, {0.0f, 0.95f, 30.0f}, {0.0f, 1.0f, 0.0f});
                gfx::FogSettings fog;
                fog.enabled = true;
                fog.start = config::kFogStart;
                fog.end = config::kFogEnd;
                fog.color = config::kColorSky;
                gfx::begin3D(proj, v, fog);
                w.draw(math3d::Frustum::fromMatrix(proj * v), eye, config::kFogEnd);
                for (int k = 0; k < 4; ++k) {
                    const int i = half * 4 + k;
                    game::CharacterState s;
                    s.position = {-1.8f + 1.15f * k, 0.0f, 30.0f};
                    s.yaw = -0.35f * pi;  // tres cuartos, mirando hacia +X
                    game::AnimInput in;
                    if (i == 1) s.velocity = game::forwardFromYaw(s.yaw) * 5.5f;
                    in.aiming = i == 2 || i == 3 || i == 4;
                    in.aimPitch = i == 3 ? 0.5f : 0.0f;
                    in.crouched = i == 4;
                    in.reloading = i == 5;
                    in.reloadProgress = 0.5f;
                    in.usingItem = sh.held == game::ItemModel::ShieldPotion && i == 2;
                    in.useProgress = 0.5f;
                    if (i == 6 || i == 7) {
                        s.jumpState = i == 6 ? game::JumpState::Rising : game::JumpState::Falling;
                        s.velocity = {0.0f, i == 6 ? 6.0f : -6.0f, 0.0f};
                        s.position.y = 0.5f;
                    }
                    game::CharacterAnim anim;
                    for (int f = 0; f < 50; ++f) anim.update(s, in, 1.0f / 60.0f);
                    models[i % 4].draw(s, anim, w.textures(), false, sh.held, false,
                                       in.crouched ? game::BodyPose::Crouch : game::BodyPose::Normal, &items);
                }
                char path[512];
                snprintf(path, sizeof(path), "%s/%s_%d.png", outDir, sh.name, half + 1);
                savePng(path, true);
            }
        }
        printf("22_posturas_*      quieto, corriendo, apuntando, arriba, agachado, recargando, saltando, cayendo\n");
    }

    items.destroy();
    for (game::CharacterModel& m : models) m.destroy();
    w.destroy();
    return 0;
}
