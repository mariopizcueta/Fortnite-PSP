// Escenario de la fase 4: deposito "Polvorin", segun Referencias/Referencias Primer Mapa.
//
// Coordenadas en metros (ver world_units.h y docs/MUNDO.md). Plano de planta:
//
//                 bosque (x < -44)                       meseta de roca (58, -60)
//   z=-32  +----------- valla metalica (puerta x -5..5) -----------+
//          |  [Nave azul]     [Nave gris]      [Nave roja]         |
//          |  x -30..-14      x -8..8          x 14..30            |
//   puerta |  z -26..4        z -26..4         z -26..4            | puerta este
//   oeste  |  altillo + escalera al fondo de cada nave               |  (camino)
//          |                                                       |
//   z=14   +--muro--[puerta x -25..-19]--valla--[puerta x 19..25]--muro--+
//                                                  \ camino de tierra hacia el sur
//            lago (-30, 56)
//
// Cada pieza registra por separado su parte VISUAL (SceneBuilder) y su COLISION
// (CollisionWorld): ver solid(), wallAlongX(), etc. Algunas cosas solo tienen una de
// las dos: copas de arbol y tejados (solo visual), agua y barandillas (solo colision de
// movimiento, los disparos las atraviesan).
#include <math.h>

#include <initializer_list>

#include "gfx/graphics.h"
#include "world/world.h"
#include "world/world_units.h"

namespace world {
namespace {

using gfx::rgba;
using math3d::Vec3;

// --- Zonas -------------------------------------------------------------------------------
constexpr float kYardMinX = -38.0f, kYardMaxX = 38.0f;
constexpr float kYardMinZ = -34.0f, kYardMaxZ = 16.0f;
constexpr float kRoadHalfWidth = 3.5f;
constexpr float kLakeX = -30.0f, kLakeZ = 56.0f, kLakeRx = 20.0f, kLakeRz = 12.0f;
constexpr float kMesaX = 58.0f, kMesaZ = -60.0f;
constexpr float kForestMaxX = -44.0f, kForestMaxZ = 36.0f;
constexpr float kWarehouseZ0 = -26.0f, kWarehouseZ1 = 4.0f;

float southRoadX(float z) { return 22.0f + (z - kYardMaxZ) * 0.22f; }

float lakeValue(float x, float z) {
    const float dx = (x - kLakeX) / kLakeRx, dz = (z - kLakeZ) / kLakeRz;
    return dx * dx + dz * dz;  // < 1 dentro del lago
}

bool insideWarehouse(float x, float z) {
    if (z < kWarehouseZ0 || z > kWarehouseZ1) return false;
    return (x > -30 && x < -14) || (x > -8 && x < 8) || (x > 14 && x < 30);
}

enum class GroundType { Grass, Concrete, Dirt, Water, Sand };

GroundType groundAt(float x, float z) {
    if (x >= kYardMinX && x <= kYardMaxX && z >= kYardMinZ && z <= kYardMaxZ) {
        return GroundType::Concrete;
    }
    const float lake = lakeValue(x, z);
    if (lake < 1.0f) return GroundType::Water;
    if (lake < 1.4f) return GroundType::Sand;
    if (z > kYardMaxZ && fabsf(x - southRoadX(z)) < kRoadHalfWidth) return GroundType::Dirt;
    if (x > kYardMaxX && fabsf(z + 6.0f) < kRoadHalfWidth) return GroundType::Dirt;
    return GroundType::Grass;
}

float hash01(int x, int z, u32 seed) {
    u32 h = static_cast<u32>(x) * 73856093u ^ static_cast<u32>(z) * 19349663u ^ seed * 83492791u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return static_cast<float>((h ^ (h >> 16)) & 0xFFFF) / 65536.0f;
}

u32 darken(u32 c, float s) {
    return rgba(static_cast<u32>((c & 0xFF) * s), static_cast<u32>(((c >> 8) & 0xFF) * s),
                static_cast<u32>(((c >> 16) & 0xFF) * s));
}

// --- Materiales ----------------------------------------------------------------------------
Material textured(int tex, u32 color, float metersPerRepeat) { return {tex, color, metersPerRepeat}; }
Material flat(u32 color) { return {kTexNone, color, 1.0f}; }

const u32 kWhite = rgba(255, 255, 255);
const Material kConcrete = textured(kTexConcrete, rgba(205, 205, 200), 4.0f);
const Material kBlockWall = textured(kTexBlockWall, kWhite, 4.0f);
const Material kShutter = textured(kTexShutter, rgba(205, 205, 200), 3.0f);
const Material kWood = textured(kTexWood, kWhite, 1.2f);
const Material kRock = textured(kTexRock, kWhite, 6.0f);
const Material kTrim = flat(rgba(196, 196, 190));
const Material kDarkTrim = flat(rgba(70, 72, 78));
const Material kSteel = flat(rgba(120, 120, 116));
const Material kGlass = flat(rgba(44, 52, 66));
const Material kTrunk = flat(rgba(96, 70, 46));

Material siding(u32 color) { return textured(kTexMetalSiding, color, 2.0f); }
Material roofPanels(u32 color) { return textured(kTexRoofPanel, color, 4.0f); }
Material containerSkin(u32 color) { return textured(kTexContainer, color, 2.6f); }

// --- Registro visual + colision ---------------------------------------------------------------

struct Builders {
    SceneBuilder& visual;
    CollisionWorld& collision;
};

// Caja con un material por cara. Orden: sur(+Z), norte(-Z), este(+X), oeste(-X), techo,
// suelo (nullptr = sin esa cara). Mismos vertices que SceneBuilder::box().
void faceBox(SceneBuilder& v, const Material* f[6], const Vec3& mn, const Vec3& mx) {
    if (f[0]) v.quad(*f[0], {mn.x, mn.y, mx.z}, {mx.x, mn.y, mx.z}, {mx.x, mx.y, mx.z}, {mn.x, mx.y, mx.z});
    if (f[1]) v.quad(*f[1], {mx.x, mn.y, mn.z}, {mn.x, mn.y, mn.z}, {mn.x, mx.y, mn.z}, {mx.x, mx.y, mn.z});
    if (f[2]) v.quad(*f[2], {mx.x, mn.y, mx.z}, {mx.x, mn.y, mn.z}, {mx.x, mx.y, mn.z}, {mx.x, mx.y, mx.z});
    if (f[3]) v.quad(*f[3], {mn.x, mn.y, mn.z}, {mn.x, mn.y, mx.z}, {mn.x, mx.y, mx.z}, {mn.x, mx.y, mn.z});
    if (f[4]) v.quad(*f[4], {mn.x, mx.y, mx.z}, {mx.x, mx.y, mx.z}, {mx.x, mx.y, mn.z}, {mn.x, mx.y, mn.z});
    if (f[5]) v.quad(*f[5], {mn.x, mn.y, mn.z}, {mx.x, mn.y, mn.z}, {mx.x, mn.y, mx.z}, {mn.x, mn.y, mx.z});
}

// Caja visible y con colision (por defecto solida para todo).
void solid(Builders& b, const Material& sides, const Material& top, const Vec3& mn, const Vec3& mx,
           u8 flags = kSolid) {
    b.visual.box(sides, top, mn, mx);
    b.collision.add({mn, mx}, flags);
}

// Hueco en una pared: de a0 a a1 a lo largo de la pared, desde el suelo hasta `height`.
struct Opening {
    float a0, a1, height;
};

// Pared a lo largo de X (grosor en Z [z0, z1]) con huecos. `insideNegZ`: la cara
// interior (mas oscura) es la que mira a -Z.
void wallAlongX(Builders& b, const Material& outer, const Material& inner, bool insideNegZ,
                float x0, float x1, float z0, float z1, float h, std::initializer_list<Opening> ops) {
    const Material* f[6] = {insideNegZ ? &outer : &inner, insideNegZ ? &inner : &outer, &outer,
                            &outer, &outer, nullptr};
    auto piece = [&](float a, float c, float y0, float y1) {
        if (c - a < 0.01f || y1 - y0 < 0.01f) return;
        faceBox(b.visual, f, {a, y0, z0}, {c, y1, z1});
        b.collision.add({{a, y0, z0}, {c, y1, z1}}, kSolid);
    };
    float cursor = x0;
    for (const Opening& op : ops) {
        piece(cursor, op.a0, 0.0f, h);
        piece(op.a0, op.a1, op.height, h);  // dintel
        cursor = op.a1;
    }
    piece(cursor, x1, 0.0f, h);
}

// Pared a lo largo de Z (grosor en X [x0, x1]). `insideNegX`: la cara interior mira a -X.
void wallAlongZ(Builders& b, const Material& outer, const Material& inner, bool insideNegX,
                float z0, float z1, float x0, float x1, float h, std::initializer_list<Opening> ops) {
    const Material* f[6] = {&outer, &outer, insideNegX ? &outer : &inner,
                            insideNegX ? &inner : &outer, &outer, nullptr};
    auto piece = [&](float a, float c, float y0, float y1) {
        if (c - a < 0.01f || y1 - y0 < 0.01f) return;
        faceBox(b.visual, f, {x0, y0, a}, {x1, y1, c});
        b.collision.add({{x0, y0, a}, {x1, y1, c}}, kSolid);
    };
    float cursor = z0;
    for (const Opening& op : ops) {
        piece(cursor, op.a0, 0.0f, h);
        piece(op.a0, op.a1, op.height, h);
        cursor = op.a1;
    }
    piece(cursor, z1, 0.0f, h);
}

// --- Naves --------------------------------------------------------------------------------

// Punto a una fraccion t de A->B, desplazado `lift` metros perpendicular hacia arriba.
void alongSlope(float ax, float ay, float bx, float by, float t, float lift, float& ox, float& oy) {
    const float dx = bx - ax, dy = by - ay, len = sqrtf(dx * dx + dy * dy);
    const float side = bx > ax ? 1.0f : -1.0f;
    ox = ax + dx * t - dy / len * lift * side;
    oy = ay + dy * t + dx / len * lift * side;
}

// Cubierta abuhardillada (como en la referencia) + hastiales + lucernarios. Solo visual:
// nadie puede llegar al tejado.
void gambrelRoof(SceneBuilder& v, float x0, float x1, float z0, float z1, float wallH,
                 const Material& roof, const Material& gable) {
    const float xm = (x0 + x1) * 0.5f;
    const float o = 0.5f;
    const float zs = z0 - o, ze = z1 + o;
    const float eave = wallH - 0.3f, knee = wallH + 3.2f, ridge = wallH + 4.8f;
    const float kx0 = x0 + 3.0f, kx1 = x1 - 3.0f;
    // Faldones izquierdos (suben hacia +X) y derechos (suben hacia -X).
    v.quad(roof, {x0 - o, eave, zs}, {x0 - o, eave, ze}, {kx0, knee, ze}, {kx0, knee, zs});
    v.quad(roof, {kx0, knee, zs}, {kx0, knee, ze}, {xm, ridge, ze}, {xm, ridge, zs});
    v.quad(roof, {x1 + o, eave, ze}, {x1 + o, eave, zs}, {kx1, knee, zs}, {kx1, knee, ze});
    v.quad(roof, {kx1, knee, ze}, {kx1, knee, zs}, {xm, ridge, zs}, {xm, ridge, ze});
    // Hastiales por encima de las paredes.
    v.quad(gable, {x0, wallH, z1}, {x1, wallH, z1}, {kx1, knee, z1}, {kx0, knee, z1});
    v.triangle(gable, {kx0, knee, z1}, {kx1, knee, z1}, {xm, ridge, z1});
    v.quad(gable, {x1, wallH, z0}, {x0, wallH, z0}, {kx0, knee, z0}, {kx1, knee, z0});
    v.triangle(gable, {kx1, knee, z0}, {kx0, knee, z0}, {xm, ridge, z0});
    // Lucernarios: rectangulos oscuros sobre el faldon superior, separados 6 cm.
    for (float zc = z0 + 4.0f; zc < z1 - 3.0f; zc += 8.0f) {
        float ax, ay, bx, by;
        alongSlope(kx0, knee, xm, ridge, 0.2f, 0.06f, ax, ay);
        alongSlope(kx0, knee, xm, ridge, 0.7f, 0.06f, bx, by);
        v.quad(kGlass, {ax, ay, zc}, {ax, ay, zc + 2.0f}, {bx, by, zc + 2.0f}, {bx, by, zc});
        alongSlope(kx1, knee, xm, ridge, 0.2f, 0.06f, ax, ay);
        alongSlope(kx1, knee, xm, ridge, 0.7f, 0.06f, bx, by);
        v.quad(kGlass, {ax, ay, zc + 2.0f}, {ax, ay, zc}, {bx, by, zc}, {bx, by, zc + 2.0f});
    }
}

// Escalera de 10 peldanos (0.36 m de alto, 0.5 m de huella) que sube hacia -Z hasta
// `topZ` a la altura `topY`. Cada peldano es un bloque solido: el personaje los sube
// solo (kStepHeight = 0.45 m).
void stairs(Builders& b, float x0, float x1, float topZ, float topY) {
    constexpr int kSteps = 10;
    const float rise = topY / kSteps;
    for (int i = 0; i < kSteps; ++i) {
        const float zBack = topZ + (kSteps - 1 - i) * 0.5f;
        solid(b, kSteel, kConcrete, {x0, 0.0f, zBack}, {x1, rise * (i + 1), zBack + 0.5f});
    }
}

void crate(Builders& b, float x, float z, float y = 0.0f) {
    solid(b, kWood, kWood, {x - 0.6f, y, z - 0.6f}, {x + 0.6f, y + 1.2f, z + 0.6f});
}

void shippingContainer(Builders& b, float x, float z, int level, u32 color, bool alongZ) {
    const float len = 6.1f, h = 2.6f, w = 2.5f, y = level * h;
    const Material skin = containerSkin(color);
    if (alongZ) solid(b, skin, skin, {x, y, z}, {x + w, y + h, z + len});
    else solid(b, skin, skin, {x, y, z}, {x + len, y + h, z + w});
}

// Estanteria industrial: postes de acero y baldas con cajas (cobertura dentro de la nave).
// Visual detallada, colision de una sola caja.
void rack(Builders& b, float x0, float z0, float x1, float z1) {
    b.collision.add({{x0, 0.0f, z0}, {x1, 2.6f, z1}}, kSolid);
    for (float y : {0.1f, 1.3f, 2.5f}) b.visual.box(kSteel, kSteel, {x0, y, z0}, {x1, y + 0.1f, z1});
    for (float px : {x0, x1 - 0.12f})
        for (float pz : {z0, z1 - 0.12f}) b.visual.box(kSteel, kSteel, {px, 0, pz}, {px + 0.12f, 2.6f, pz + 0.12f});
    const float midX = (x0 + x1) * 0.5f;
    b.visual.box(kWood, kWood, {x0 + 0.1f, 0.2f, z0 + 0.1f}, {midX - 0.1f, 1.2f, z1 - 0.1f});
    b.visual.box(kWood, kWood, {midX + 0.1f, 1.4f, z0 + 0.1f}, {x1 - 0.1f, 2.3f, z1 - 0.1f});
}

// Nave con portalon delantero (sur), puerta trasera, puertas laterales, altillo con
// escalera y barandilla, y obstaculos interiores segun `variant`.
void warehouse(Builders& b, float x0, float x1, u32 color, float backDoorX, int variant,
               std::vector<Vec3>& pois) {
    const float z0 = kWarehouseZ0, z1 = kWarehouseZ1;
    const float t = 0.4f, h = 8.0f, xm = (x0 + x1) * 0.5f;
    const Material outer = siding(color);
    const Material inner = siding(darken(color, 0.6f));

    // Paredes: frontal con portalon de 8 x 6.5 m, trasera con puerta de 4 x 2.8 m y
    // laterales con puerta de 4 x 2.8 m centrada en z = -10 (pasillos entre naves).
    wallAlongX(b, outer, inner, true, x0, x1, z1 - t, z1, h, {{xm - 4.0f, xm + 4.0f, 6.5f}});
    wallAlongX(b, outer, inner, false, x0, x1, z0, z0 + t, h, {{backDoorX - 2.0f, backDoorX + 2.0f, 2.8f}});
    wallAlongZ(b, outer, inner, false, z0 + t, z1 - t, x0, x0 + t, h, {{-12.0f, -8.0f, 2.8f}});
    wallAlongZ(b, outer, inner, true, z0 + t, z1 - t, x1 - t, x1, h, {{-12.0f, -8.0f, 2.8f}});

    // Persiana medio subida (por encima de la cabeza) y marco claro del portalon.
    solid(b, kShutter, kShutter, {xm - 4.0f, 4.6f, z1 - 0.3f}, {xm + 4.0f, 6.5f, z1 - 0.15f});
    solid(b, kTrim, kTrim, {xm - 4.4f, 0.0f, z1}, {xm - 4.0f, 6.9f, z1 + 0.3f});
    solid(b, kTrim, kTrim, {xm + 4.0f, 0.0f, z1}, {xm + 4.4f, 6.9f, z1 + 0.3f});
    b.visual.box(kTrim, kTrim, {xm - 4.4f, 6.5f, z1}, {xm + 4.4f, 6.9f, z1 + 0.3f});
    // Zocalo de hormigon exterior en los laterales (solo visual, 5 cm por fuera).
    b.visual.quad(kConcrete, {x0 - 0.05f, 0, z0 + 0.2f}, {x0 - 0.05f, 0, z1 - 0.2f},
                  {x0 - 0.05f, 1.0f, z1 - 0.2f}, {x0 - 0.05f, 1.0f, z0 + 0.2f});
    b.visual.quad(kConcrete, {x1 + 0.05f, 0, z1 - 0.2f}, {x1 + 0.05f, 0, z0 + 0.2f},
                  {x1 + 0.05f, 1.0f, z0 + 0.2f}, {x1 + 0.05f, 1.0f, z1 - 0.2f});

    gambrelRoof(b.visual, x0, x1, z0, z1, h, roofPanels(rgba(96, 100, 106)), outer);

    // Altillo al fondo: suelo a 3.6 m, apoyado en pilares, con barandilla y escalera.
    const float mezzFront = z0 + 6.0f, mezzY = 3.6f;
    const float stairX0 = x0 + t, stairX1 = x0 + t + 1.6f;
    solid(b, kSteel, kConcrete, {x0 + t, mezzY - 0.3f, z0 + t}, {x1 - t, mezzY, mezzFront});
    for (float px : {x0 + 4.0f, xm, x1 - 3.0f}) {
        solid(b, kSteel, kSteel, {px - 0.15f, 0.0f, mezzFront - 0.35f}, {px + 0.15f, mezzY - 0.3f, mezzFront - 0.05f});
    }
    // Barandilla: bloquea el paso (no caerse) pero no los disparos.
    b.collision.add({{stairX1, mezzY, mezzFront - 0.08f}, {x1 - t, mezzY + 1.0f, mezzFront}}, kBlocksMovement);
    b.visual.box(kDarkTrim, kDarkTrim, {stairX1, mezzY + 0.9f, mezzFront - 0.08f}, {x1 - t, mezzY + 1.0f, mezzFront});
    b.visual.box(kDarkTrim, kDarkTrim, {stairX1, mezzY + 0.45f, mezzFront - 0.06f}, {x1 - t, mezzY + 0.52f, mezzFront - 0.02f});
    for (float px = stairX1; px < x1 - t; px += 2.0f) {
        b.visual.box(kDarkTrim, kDarkTrim, {px, mezzY, mezzFront - 0.08f}, {px + 0.08f, mezzY + 1.0f, mezzFront});
    }
    stairs(b, stairX0, stairX1, mezzFront, mezzY);
    // Cajas en el altillo: cobertura para quien suba.
    crate(b, x1 - 2.0f, z0 + 1.8f, mezzY);
    crate(b, xm, z0 + 1.6f, mezzY);

    // Obstaculos interiores (sin cortar el pasillo de las puertas laterales, z -12..-8).
    switch (variant) {
        case 0:
            rack(b, x1 - 2.6f, -6.0f, x1 - 1.4f, 1.0f);
            crate(b, xm - 3.5f, -3.0f);
            crate(b, xm - 3.5f, -1.8f);
            crate(b, xm - 3.5f, -2.4f, 1.2f);
            break;
        case 1:
            shippingContainer(b, xm - 3.0f, z0 + 1.5f, 0, rgba(176, 62, 48), false);  // bajo el altillo
            rack(b, xm - 5.5f, -5.0f, xm - 4.3f, 1.0f);
            crate(b, xm + 4.0f, -4.0f);
            crate(b, xm + 4.0f, -2.8f);
            break;
        default:
            solid(b, flat(rgba(214, 170, 40)), flat(rgba(214, 170, 40)), {xm + 2.5f, 0, -5.0f}, {xm + 3.7f, 2.2f, -3.0f});
            rack(b, x0 + 2.4f, -6.0f, x0 + 3.6f, 0.0f);
            crate(b, xm - 1.0f, -15.0f);
            crate(b, xm + 0.4f, -15.4f);
            break;
    }

    pois.push_back({xm, 0.0f, -10.0f});
    pois.push_back({xm, 0.0f, -2.0f});
}

// --- Recinto ------------------------------------------------------------------------------

// Valla metalica: postes y largueros visuales; bloquea el paso pero no los disparos.
void chainFenceAlongX(Builders& b, float z, float x0, float x1) {
    if (x1 - x0 < 0.1f) return;
    b.collision.add({{x0, 0.0f, z - 0.05f}, {x1, 2.4f, z + 0.05f}}, kBlocksMovement);
    for (float x = x0; x <= x1 + 0.01f; x += 3.0f) {
        b.visual.box(kSteel, kSteel, {x - 0.06f, 0, z - 0.06f}, {x + 0.06f, 2.4f, z + 0.06f});
    }
    for (float y : {0.3f, 1.2f, 2.3f}) {
        b.visual.quad(kSteel, {x0, y, z}, {x1, y, z}, {x1, y + 0.08f, z}, {x0, y + 0.08f, z});
    }
}

void lampPost(Builders& b, float x, float z, float armDirX) {
    solid(b, kDarkTrim, kDarkTrim, {x - 0.12f, 0, z - 0.12f}, {x + 0.12f, 6.5f, z + 0.12f});
    b.visual.box(kDarkTrim, kDarkTrim, {fminf(x, x + armDirX * 1.6f), 6.3f, z - 0.08f},
                 {fmaxf(x, x + armDirX * 1.6f), 6.45f, z + 0.08f});
    b.visual.box(flat(rgba(240, 236, 200)), kDarkTrim, {x + armDirX * 1.6f - 0.25f, 6.0f, z - 0.15f},
                 {x + armDirX * 1.6f + 0.25f, 6.3f, z + 0.15f});
}

void boxTrailer(Builders& b, float x0, float z0, float x1, float z1) {
    b.collision.add({{x0, 0.0f, z0}, {x1, 3.6f, z1}}, kSolid);
    const Material skin = containerSkin(rgba(210, 210, 204));
    b.visual.box(skin, skin, {x0, 1.0f, z0}, {x1, 3.6f, z1}, true);
    b.visual.box(kDarkTrim, kDarkTrim, {x0 + 0.5f, 0.6f, z0 + 0.3f}, {x1 - 0.5f, 1.0f, z1 - 0.3f});
    for (float wx : {x0 + 1.2f, x0 + 2.4f, x1 - 1.6f}) {
        b.visual.box(kDarkTrim, kDarkTrim, {wx - 0.45f, 0, z0 + 0.1f}, {wx + 0.45f, 0.9f, z0 + 0.4f});
        b.visual.box(kDarkTrim, kDarkTrim, {wx - 0.45f, 0, z1 - 0.4f}, {wx + 0.45f, 0.9f, z1 - 0.1f});
    }
}

void buildDepot(Builders& b, std::vector<StaticBatch>& out, std::vector<Vec3>& pois) {
    // Puertas traseras centradas en x = -22, -2 y 22: coinciden con nodos de la rejilla
    // de navegacion de 4 m, asi los bots pueden usarlas.
    warehouse(b, -30.0f, -14.0f, rgba(58, 82, 168), -22.0f, 0, pois);   // azul
    b.visual.flushGroup(out);
    warehouse(b, -8.0f, 8.0f, rgba(128, 132, 130), -2.0f, 1, pois);     // gris
    b.visual.flushGroup(out);
    warehouse(b, 14.0f, 30.0f, rgba(156, 72, 52), 22.0f, 2, pois);      // roja
    b.visual.flushGroup(out);

    // Muro de bloques (2.4 m: no se salta) al sur con dos puertas y un tramo de valla.
    const float wallH = 2.4f;
    auto wallX = [&](float x0, float x1, float z) {
        if (x1 - x0 > 0.01f) solid(b, kBlockWall, kConcrete, {x0, 0, z}, {x1, wallH, z + 0.4f});
    };
    auto wallZ = [&](float z0, float z1, float x) {
        if (z1 - z0 > 0.01f) solid(b, kBlockWall, kConcrete, {x, 0, z0}, {x + 0.4f, wallH, z1});
    };
    wallX(-36.4f, -25.0f, 14.0f);
    wallX(-19.0f, -6.0f, 14.0f);
    chainFenceAlongX(b, 14.2f, -6.0f, 6.0f);
    wallX(6.0f, 19.0f, 14.0f);
    wallX(25.0f, 36.4f, 14.0f);
    b.visual.flushGroup(out);
    wallZ(-32.0f, -9.0f, -36.4f);
    wallZ(-3.0f, 14.0f, -36.4f);
    wallZ(-32.0f, -9.0f, 36.0f);
    wallZ(-3.0f, 14.0f, 36.0f);
    chainFenceAlongX(b, -32.0f, -36.4f, -5.0f);
    chainFenceAlongX(b, -32.0f, 5.0f, 36.4f);
    b.visual.flushGroup(out);

    // Patio: remolque junto a la nave roja, contenedores, cajas, farolas, contenedor de basura.
    boxTrailer(b, 26.0f, 6.5f, 35.6f, 9.0f);
    shippingContainer(b, -35.5f, 7.0f, 0, rgba(52, 92, 152), false);
    shippingContainer(b, -35.5f, 7.0f, 1, rgba(62, 124, 72), false);
    shippingContainer(b, -27.0f, -31.5f, 0, rgba(206, 126, 46), false);
    crate(b, -12.0f, 9.0f);
    crate(b, -10.8f, 9.4f);
    crate(b, -11.4f, 9.2f, 1.2f);
    crate(b, 10.5f, 10.0f);
    crate(b, 3.0f, 8.5f);
    lampPost(b, -11.0f, 5.5f, 1.0f);
    lampPost(b, 11.0f, 5.5f, -1.0f);
    lampPost(b, 33.0f, -16.0f, -1.0f);
    solid(b, flat(rgba(60, 110, 70)), flat(rgba(50, 90, 60)), {6.0f, 0, -30.0f}, {8.5f, 1.4f, -28.6f});
    b.visual.flushGroup(out);

    pois.push_back({0.0f, 0.0f, 9.0f});
    pois.push_back({-10.0f, 0.0f, -14.0f});
    pois.push_back({10.0f, 0.0f, -14.0f});
    pois.push_back({0.0f, 0.0f, -29.0f});
}

// --- Naturaleza ----------------------------------------------------------------------------

void pineTree(Builders& b, float x, float z, float s) {
    solid(b, kTrunk, kTrunk, {x - 0.3f * s, 0, z - 0.3f * s}, {x + 0.3f * s, 2.2f * s, z + 0.3f * s});
    b.visual.cone(flat(rgba(50, 104, 52)), {x, 1.8f * s, z}, 2.6f * s, 4.2f * s, 6);
    b.visual.cone(flat(rgba(62, 124, 60)), {x, 4.4f * s, z}, 1.8f * s, 3.2f * s, 6);
}

// Arbol de copa cubica, como los de Recursos/Loadscreen.png. La copa no colisiona.
void blockTree(Builders& b, float x, float z, float s) {
    solid(b, kTrunk, kTrunk, {x - 0.3f * s, 0, z - 0.3f * s}, {x + 0.3f * s, 3.0f * s, z + 0.3f * s});
    const Material leaves = flat(rgba(72, 132, 52));
    const Material leavesTop = flat(rgba(92, 156, 62));
    b.visual.box(leaves, leavesTop, {x - 2.2f * s, 2.6f * s, z - 2.2f * s}, {x + 2.2f * s, 4.8f * s, z + 2.2f * s}, true);
    b.visual.box(leaves, leavesTop, {x - 1.4f * s, 4.8f * s, z - 1.4f * s}, {x + 1.4f * s, 6.0f * s, z + 1.4f * s});
}

void boulder(Builders& b, float x, float z, float w, float d, float h) {
    solid(b, kRock, kRock, {x - w * 0.5f, 0, z - d * 0.5f}, {x + w * 0.5f, h, z + d * 0.5f});
    b.visual.box(kRock, kRock, {x - w * 0.3f, h, z - d * 0.25f}, {x + w * 0.2f, h + 0.5f, z + d * 0.3f});
}

// Meseta de roca: dos troncos de cono de 12 lados con radio irregular, cima de hierba.
void mesa(Builders& b, std::vector<StaticBatch>& out) {
    constexpr int kSides = 12;
    const float levels[3] = {0.0f, 13.0f, 26.0f};
    const float radii[3] = {22.0f, 18.5f, 15.0f};
    Vec3 ring[3][kSides];
    for (int l = 0; l < 3; ++l) {
        for (int i = 0; i < kSides; ++i) {
            const float a = 2.0f * math3d::kPi * i / kSides;
            const float r = radii[l] * (0.86f + 0.14f * hash01(i, l, 501));
            ring[l][i] = {kMesaX + cosf(a) * r, levels[l], kMesaZ - sinf(a) * r};
        }
    }
    for (int l = 0; l < 2; ++l) {
        for (int i = 0; i < kSides; ++i) {
            const int j = (i + 1) % kSides;
            b.visual.quad(kRock, ring[l][i], ring[l][j], ring[l + 1][j], ring[l + 1][i]);
        }
    }
    const Material topGrass = textured(kTexGrass, kWhite, 4.0f);
    const Vec3 top{kMesaX, levels[2], kMesaZ};
    for (int i = 0; i < kSides; ++i) b.visual.triangle(topGrass, top, ring[2][i], ring[2][(i + 1) % kSides]);
    b.visual.flushGroup(out);

    // Colision: union de tres cajas que aproxima el circulo inscrito de la base (r 18.5).
    const float r = 18.5f;
    for (float a : {0.26f, 0.785f, 1.31f}) {
        const float hx = r * cosf(a), hz = r * sinf(a);
        b.collision.add({{kMesaX - hx, 0.0f, kMesaZ - hz}, {kMesaX + hx, 26.0f, kMesaZ + hz}}, kSolid);
    }

    // Pinos y deposito de agua en la cima (solo visual: no se puede subir).
    for (int i = 0; i < 4; ++i) {
        const float a = 1.3f * i + 0.4f;
        const float x = kMesaX + cosf(a) * 9.0f, z = kMesaZ - sinf(a) * 9.0f;
        b.visual.box(kTrunk, kTrunk, {x - 0.3f, 26.0f, z - 0.3f}, {x + 0.3f, 28.2f, z + 0.3f});
        b.visual.cone(flat(rgba(50, 104, 52)), {x, 27.8f, z}, 2.6f, 4.2f, 6);
        b.visual.cone(flat(rgba(62, 124, 60)), {x, 30.4f, z}, 1.8f, 3.2f, 6);
    }
    const float tx = kMesaX - 6.0f, tz = kMesaZ + 6.0f;
    for (float dx : {-1.2f, 1.2f})
        for (float dz : {-1.2f, 1.2f})
            b.visual.box(flat(rgba(110, 80, 56)), kTrunk, {tx + dx - 0.12f, 26.0f, tz + dz - 0.12f},
                         {tx + dx + 0.12f, 31.0f, tz + dz + 0.12f});
    b.visual.cylinder(flat(rgba(150, 104, 70)), flat(rgba(120, 84, 56)), {tx, 31.0f, tz}, 2.0f, 2.6f, 8);
    b.visual.cone(flat(rgba(120, 84, 56)), {tx, 33.6f, tz}, 2.2f, 1.0f, 8);
    b.visual.flushGroup(out);
}

void lake(Builders& b, std::vector<StaticBatch>& out) {
    // El agua se dibuja en buildGround(); aqui solo su colision: bloquea el paso (no se
    // nada) pero no los disparos. Una caja por fila de losas de agua.
    for (float z = kLakeZ - kLakeRz - 2.0f; z <= kLakeZ + kLakeRz + 2.0f; z += kTileSize) {
        float runStart = 0.0f;
        bool inRun = false;
        for (float x = kLakeX - kLakeRx - 2.0f; x <= kLakeX + kLakeRx + 2.0f + kTileSize; x += kTileSize) {
            const bool water = lakeValue(x + kTileSize * 0.5f, z + kTileSize * 0.5f) < 1.0f;
            if (water && !inRun) {
                runStart = x;
                inRun = true;
            } else if (!water && inRun) {
                b.collision.add({{runStart, -1.0f, z}, {x, 2.5f, z + kTileSize}}, kBlocksMovement);
                inRun = false;
            }
        }
    }
    // Rocas en la orilla como cobertura.
    boulder(b, kLakeX + 22.0f, kLakeZ - 4.0f, 2.6f, 2.0f, 1.6f);
    boulder(b, kLakeX - 8.0f, kLakeZ - 15.5f, 3.2f, 2.2f, 1.8f);
    boulder(b, kLakeX + 6.0f, kLakeZ + 15.0f, 2.4f, 2.4f, 1.4f);
    b.visual.flushGroup(out);
}

void forest(Builders& b, std::vector<StaticBatch>& out) {
    // Rejilla de 7 m con jitter al oeste del deposito; un lote cada dos filas.
    int row = 0;
    for (float z = -90.0f; z <= kForestMaxZ; z += 7.0f, ++row) {
        int col = 0;
        for (float x = -91.0f; x <= kForestMaxX; x += 7.0f, ++col) {
            if (hash01(col, row, 601) < 0.18f) continue;  // claros
            const float px = x + (hash01(col, row, 602) - 0.5f) * 4.0f;
            const float pz = z + (hash01(col, row, 603) - 0.5f) * 4.0f;
            const float s = 0.8f + 0.45f * hash01(col, row, 604);
            if (hash01(col, row, 605) < 0.6f) pineTree(b, px, pz, s);
            else blockTree(b, px, pz, s);
        }
        if (row % 2 == 1) b.visual.flushGroup(out);
    }
    b.visual.flushGroup(out);
}

void scatteredTrees(Builders& b, std::vector<StaticBatch>& out) {
    const float spots[][2] = {{-20, 34}, {-4, 40}, {12, 52}, {44, 34}, {58, 22}, {74, 6},
                              {84, -24}, {30, -44}, {-6, -48}, {-26, -46}, {50, 64}, {72, 74},
                              {-6, 84}, {30, 86}, {84, 44}, {-30, 82}, {-60, 76}, {-80, 56}};
    int i = 0;
    for (const auto& p : spots) {
        if (i % 2) blockTree(b, p[0], p[1], 0.9f + 0.1f * (i % 3));
        else pineTree(b, p[0], p[1], 1.0f + 0.1f * (i % 4));
        ++i;
        if (i % 6 == 0) b.visual.flushGroup(out);
    }
    // Rocas sueltas en los prados: cobertura entre el deposito y los bordes.
    boulder(b, 6.0f, 30.0f, 3.0f, 2.0f, 1.7f);
    boulder(b, -14.0f, 26.0f, 2.2f, 2.8f, 1.5f);
    boulder(b, 46.0f, 12.0f, 2.8f, 2.4f, 1.8f);
    boulder(b, 62.0f, -18.0f, 3.4f, 2.0f, 2.0f);
    boulder(b, 14.0f, -46.0f, 2.6f, 2.6f, 1.6f);
    boulder(b, -18.0f, -56.0f, 3.0f, 2.2f, 1.9f);
    boulder(b, 66.0f, 48.0f, 2.4f, 3.0f, 1.6f);
    boulder(b, 0.0f, 64.0f, 3.2f, 2.4f, 1.8f);
    boulder(b, 34.0f, 70.0f, 2.6f, 2.2f, 1.5f);
    boulder(b, 80.0f, 70.0f, 3.0f, 3.0f, 2.2f);
    b.visual.flushGroup(out);
}

}  // namespace

void buildGround(SceneBuilder& b, CollisionWorld& /*collision*/, std::vector<StaticBatch>& out) {
    const Material grass = textured(kTexGrass, kWhite, 4.0f);
    const Material concrete = textured(kTexConcrete, kWhite, 4.0f);
    const Material dirt = textured(kTexDirt, kWhite, 3.0f);
    const Material water = textured(kTexWater, kWhite, 6.0f);
    const Material sand = textured(kTexSand, kWhite, 4.0f);

    for (int cz = 0; cz < kChunksPerSide; ++cz) {
        for (int cx = 0; cx < kChunksPerSide; ++cx) {
            // Los bloques que son solo hierba usan losas del doble de lado.
            bool onlyGrass = true;
            for (int tz = 0; tz < kTilesPerChunk && onlyGrass; ++tz) {
                for (int tx = 0; tx < kTilesPerChunk && onlyGrass; ++tx) {
                    const float x = -kMapHalfSize + (cx * kTilesPerChunk + tx + 0.5f) * kTileSize;
                    const float z = -kMapHalfSize + (cz * kTilesPerChunk + tz + 0.5f) * kTileSize;
                    onlyGrass = groundAt(x, z) == GroundType::Grass;
                }
            }
            if (onlyGrass) {
                const float big = kTileSize * 2.0f;
                for (int tz = 0; tz < kTilesPerChunk / 2; ++tz) {
                    for (int tx = 0; tx < kTilesPerChunk / 2; ++tx) {
                        const int ix = cx * kTilesPerChunk / 2 + tx;
                        const int iz = cz * kTilesPerChunk / 2 + tz;
                        b.groundTile(grass, -kMapHalfSize + ix * big, -kMapHalfSize + iz * big, big,
                                     0.88f + 0.16f * hash01(ix, iz, 8));
                    }
                }
                b.flushGroup(out);
                continue;
            }
            for (int tz = 0; tz < kTilesPerChunk; ++tz) {
                for (int tx = 0; tx < kTilesPerChunk; ++tx) {
                    const int ix = cx * kTilesPerChunk + tx;
                    const int iz = cz * kTilesPerChunk + tz;
                    const float x0 = -kMapHalfSize + ix * kTileSize;
                    const float z0 = -kMapHalfSize + iz * kTileSize;
                    const float mx = x0 + kTileSize * 0.5f, mz = z0 + kTileSize * 0.5f;
                    const float jitter = hash01(ix, iz, 7);
                    switch (groundAt(mx, mz)) {
                        case GroundType::Grass:
                            b.groundTile(grass, x0, z0, kTileSize, 0.88f + 0.16f * jitter);
                            break;
                        case GroundType::Concrete: {
                            // Dentro de las naves el suelo es mas oscuro: sombra "horneada".
                            const float shade = insideWarehouse(mx, mz) ? 0.6f : 1.0f;
                            b.groundTile(concrete, x0, z0, kTileSize, (0.94f + 0.08f * jitter) * shade);
                            break;
                        }
                        case GroundType::Dirt:
                            b.groundTile(dirt, x0, z0, kTileSize, 0.90f + 0.12f * jitter);
                            break;
                        case GroundType::Water:
                            b.groundTile(water, x0, z0, kTileSize, 0.95f + 0.05f * jitter);
                            break;
                        case GroundType::Sand:
                            b.groundTile(sand, x0, z0, kTileSize, 0.92f + 0.08f * jitter);
                            break;
                    }
                }
            }
            b.flushGroup(out);
        }
    }
}

void buildStructures(SceneBuilder& visual, CollisionWorld& collision, std::vector<StaticBatch>& out,
                     std::vector<Vec3>& pois) {
    Builders b{visual, collision};
    buildDepot(b, out, pois);
    mesa(b, out);
    lake(b, out);
    forest(b, out);
    scatteredTrees(b, out);

    pois.push_back({kLakeX + 4.0f, 0.0f, kLakeZ - kLakeRz - 5.0f});   // orilla norte del lago
    pois.push_back({-62.0f, 0.0f, -20.0f});                            // bosque
    pois.push_back({kMesaX - 26.0f, 0.0f, kMesaZ + 16.0f});           // pie de la meseta
    pois.push_back({southRoadX(50.0f), 0.0f, 50.0f});                  // camino sur
    pois.push_back({62.0f, 0.0f, -6.0f});                              // camino este
}

}  // namespace world
