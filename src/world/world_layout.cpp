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
#include "world/storm.h"
#include "world/water.h"
#include "world/world.h"
#include "world/world_units.h"
#include "world/zombie_sites.h"

namespace world {
namespace {

using gfx::rgba;
using math3d::Vec3;

// --- Zonas -------------------------------------------------------------------------------
constexpr float kYardMinX = -38.0f, kYardMaxX = 38.0f;
constexpr float kYardMinZ = -34.0f, kYardMaxZ = 16.0f;
constexpr float kRoadHalfWidth = 3.5f;
constexpr float kMesaX = 58.0f, kMesaZ = -60.0f;
constexpr float kForestMaxX = -44.0f, kForestMaxZ = 36.0f;
constexpr float kWarehouseZ0 = -26.0f, kWarehouseZ1 = 4.0f;

float southRoadX(float z) { return 22.0f + (z - kYardMaxZ) * 0.22f; }


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
    std::vector<Harvestable>* harvest = nullptr;
    std::vector<StaticBatch>* out = nullptr;  // lotes de dibujo (los de cada objeto, aparte)
    int objectCollider = 0;                    // primera caja / lote del objeto en curso
    int objectBatch = 0;
    bool zombieOnly = false;                   // lo que se registra solo existe en el modo zombi
};

// Lo que da cada objeto al picarlo (por golpe) y su vida (el pico quita 50 por golpe).
// `ruin`: en el modo zombi, % de objetos de ese tipo que ya estan derruidos (v0.14).
struct Yield {
    Resource resource;
    int perHit;
    float health;
    u8 ruin;
};
constexpr Yield kTreeYield{Resource::Wood, 8, 300.0f, 0};
constexpr Yield kCrateYield{Resource::Wood, 6, 150.0f, 35};
constexpr Yield kRockYield{Resource::Stone, 7, 300.0f, 0};
constexpr Yield kMetalYield{Resource::Metal, 8, 400.0f, 15};
constexpr Yield kSmallMetalYield{Resource::Metal, 6, 200.0f, 30};
constexpr Yield kPanelYield{Resource::Metal, 6, 300.0f, 38};      // paneles de las naves y tejados
constexpr Yield kBlockWallYield{Resource::Stone, 6, 250.0f, 25};  // muro de bloques y peldanos
constexpr Yield kFenceYield{Resource::Metal, 4, 150.0f, 45};      // vallas, farolas, barandillas
// Objetos del modo zombi (base, campamentos, centros de crafteo): nunca derruidos.
constexpr Yield kCampWoodYield{Resource::Wood, 6, 200.0f, 0};
constexpr Yield kCampMetalYield{Resource::Metal, 6, 260.0f, 0};

// Objeto del mapa que se puede picar y destruir: todo lo que se dibuje y se anada a la
// colision entre beginObject() y endObject() forma un solo objeto (sus lotes de dibujo
// van aparte para poder dejar de dibujarlos).
void beginObject(Builders& b) {
    if (b.out) b.visual.flushGroup(*b.out);  // lo anterior, en sus propios lotes
    b.objectCollider = b.collision.count();
    b.objectBatch = b.out ? static_cast<int>(b.out->size()) : 0;
}

void endObject(Builders& b, const Yield& y) {
    if (b.out) b.visual.flushGroup(*b.out);
    const int colliders = b.collision.count() - b.objectCollider;
    if (!b.harvest || colliders <= 0) return;
    Harvestable h;
    h.collider = b.objectCollider;
    h.colliderCount = colliders;
    h.batchBegin = b.objectBatch;
    h.batchEnd = b.out ? static_cast<int>(b.out->size()) : b.objectBatch;
    h.resource = y.resource;
    h.box = b.collision.collider(h.collider).box;
    for (int i = 1; i < colliders; ++i) {
        const Aabb& c = b.collision.collider(h.collider + i).box;
        h.box.min = {fminf(h.box.min.x, c.min.x), fminf(h.box.min.y, c.min.y), fminf(h.box.min.z, c.min.z)};
        h.box.max = {fmaxf(h.box.max.x, c.max.x), fmaxf(h.box.max.y, c.max.y), fmaxf(h.box.max.z, c.max.z)};
    }
    h.perHit = y.perHit;
    h.health = y.health;
    h.ruinChance = y.ruin;
    h.zombieOnly = b.zombieOnly;
    b.harvest->push_back(h);
}

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

// Tramos de pared de mas de 4 m: paneles de 4 m como mucho, cada uno un objeto que se
// puede picar y destruir por separado (como las piezas de Fortnite).
template <typename Fn>
void panels(float a, float c, Fn&& panel) {
    const int n = static_cast<int>(ceilf((c - a) / 4.0f - 0.01f));
    for (int i = 0; i < n; ++i) panel(a + (c - a) * i / n, a + (c - a) * (i + 1) / n);
}

// Pared a lo largo de X (grosor en Z [z0, z1]) con huecos. `insideNegZ`: la cara
// interior (mas oscura) es la que mira a -Z.
void wallAlongX(Builders& b, const Material& outer, const Material& inner, bool insideNegZ,
                float x0, float x1, float z0, float z1, float h, std::initializer_list<Opening> ops) {
    const Material* f[6] = {insideNegZ ? &outer : &inner, insideNegZ ? &inner : &outer, &outer,
                            &outer, &outer, nullptr};
    auto piece = [&](float a, float c, float y0, float y1) {
        if (c - a < 0.01f || y1 - y0 < 0.01f) return;
        panels(a, c, [&](float pa, float pc) {
            beginObject(b);
            faceBox(b.visual, f, {pa, y0, z0}, {pc, y1, z1});
            b.collision.add({{pa, y0, z0}, {pc, y1, z1}}, kSolid);
            endObject(b, kPanelYield);
        });
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
        panels(a, c, [&](float pa, float pc) {
            beginObject(b);
            faceBox(b.visual, f, {x0, y0, pa}, {x1, y1, pc});
            b.collision.add({{x0, y0, pa}, {x1, y1, pc}}, kSolid);
            endObject(b, kPanelYield);
        });
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

// Cubierta abuhardillada (como en la referencia) + hastiales + lucernarios, en cuatro
// tramos a lo largo de Z: cada tramo es un objeto que se puede picar y destruir.
// Colision: columnas de 1 m de ancho que siguen el perfil del tejado (losas de 0.5 m), para
// que quien cae del autobus o planea se pose encima en lugar de atravesarlo.
void gambrelRoof(Builders& b, float x0, float x1, float z0, float z1, float wallH,
                 const Material& roof, const Material& gable) {
    SceneBuilder& v = b.visual;
    const float xm = (x0 + x1) * 0.5f;
    const float o = 0.5f;
    const float zs = z0 - o, ze = z1 + o;
    const float eave = wallH - 0.3f, knee = wallH + 3.2f, ridge = wallH + 4.8f;
    const float kx0 = x0 + 3.0f, kx1 = x1 - 3.0f;
    auto profile = [&](float x) {
        const float d = fabsf(x - xm), half = xm - (x0 - o), kneeD = xm - kx0;
        if (d >= kneeD) return eave + (knee - eave) * (half - d) / (half - kneeD);
        return knee + (ridge - knee) * (kneeD - d) / kneeD;
    };
    constexpr int kSegments = 4;
    for (int seg = 0; seg < kSegments; ++seg) {
        const float za = zs + (ze - zs) * seg / kSegments, zb = zs + (ze - zs) * (seg + 1) / kSegments;
        beginObject(b);
        // Faldones izquierdos (suben hacia +X) y derechos (suben hacia -X).
        v.quad(roof, {x0 - o, eave, za}, {x0 - o, eave, zb}, {kx0, knee, zb}, {kx0, knee, za});
        v.quad(roof, {kx0, knee, za}, {kx0, knee, zb}, {xm, ridge, zb}, {xm, ridge, za});
        v.quad(roof, {x1 + o, eave, zb}, {x1 + o, eave, za}, {kx1, knee, za}, {kx1, knee, zb});
        v.quad(roof, {kx1, knee, zb}, {kx1, knee, za}, {xm, ridge, za}, {xm, ridge, zb});
        // Hastiales por encima de las paredes (con el primer y el ultimo tramo).
        if (seg == kSegments - 1) {
            v.quad(gable, {x0, wallH, z1}, {x1, wallH, z1}, {kx1, knee, z1}, {kx0, knee, z1});
            v.triangle(gable, {kx0, knee, z1}, {kx1, knee, z1}, {xm, ridge, z1});
        }
        if (seg == 0) {
            v.quad(gable, {x1, wallH, z0}, {x0, wallH, z0}, {kx0, knee, z0}, {kx1, knee, z0});
            v.triangle(gable, {kx1, knee, z0}, {kx0, knee, z0}, {xm, ridge, z0});
        }
        // Lucernarios: rectangulos oscuros sobre el faldon superior, separados 6 cm.
        for (float zc = z0 + 4.0f; zc < z1 - 3.0f; zc += 8.0f) {
            if (zc < za || zc >= zb) continue;
            float ax, ay, bx, by;
            alongSlope(kx0, knee, xm, ridge, 0.2f, 0.06f, ax, ay);
            alongSlope(kx0, knee, xm, ridge, 0.7f, 0.06f, bx, by);
            v.quad(kGlass, {ax, ay, zc}, {ax, ay, zc + 2.0f}, {bx, by, zc + 2.0f}, {bx, by, zc});
            alongSlope(kx1, knee, xm, ridge, 0.2f, 0.06f, ax, ay);
            alongSlope(kx1, knee, xm, ridge, 0.7f, 0.06f, bx, by);
            v.quad(kGlass, {ax, ay, zc + 2.0f}, {ax, ay, zc}, {bx, by, zc}, {bx, by, zc + 2.0f});
        }
        const int columns = static_cast<int>(ceilf(x1 - x0 + 2.0f * o));
        const float w = (x1 - x0 + 2.0f * o) / columns;
        for (int c = 0; c < columns; ++c) {
            const float cx0 = x0 - o + c * w, cx1 = cx0 + w;
            const float top = profile((cx0 + cx1) * 0.5f);
            b.collision.add({{cx0, top - 0.5f, za}, {cx1, top, zb}}, kSolid);
        }
        endObject(b, kPanelYield);
    }
}

// Escalera de 10 peldanos (0.36 m de alto, 0.5 m de huella) que sube hacia -Z hasta
// `topZ` a la altura `topY`. Cada peldano es un bloque solido: el personaje los sube
// solo (kStepHeight = 0.45 m).
void stairs(Builders& b, float x0, float x1, float topZ, float topY) {
    constexpr int kSteps = 10;
    const float rise = topY / kSteps;
    beginObject(b);  // toda la escalera es un objeto
    for (int i = 0; i < kSteps; ++i) {
        const float zBack = topZ + (kSteps - 1 - i) * 0.5f;
        solid(b, kSteel, kConcrete, {x0, 0.0f, zBack}, {x1, rise * (i + 1), zBack + 0.5f});
    }
    endObject(b, kSmallMetalYield);
}

void crate(Builders& b, float x, float z, float y = 0.0f) {
    beginObject(b);
    solid(b, kWood, kWood, {x - 0.6f, y, z - 0.6f}, {x + 0.6f, y + 1.2f, z + 0.6f});
    endObject(b, kCrateYield);
}

void shippingContainer(Builders& b, float x, float z, int level, u32 color, bool alongZ) {
    const float len = 6.1f, h = 2.6f, w = 2.5f, y = level * h;
    const Material skin = containerSkin(color);
    beginObject(b);
    if (alongZ) solid(b, skin, skin, {x, y, z}, {x + w, y + h, z + len});
    else solid(b, skin, skin, {x, y, z}, {x + len, y + h, z + w});
    endObject(b, kMetalYield);
}

// Estanteria industrial: postes de acero y baldas con cajas (cobertura dentro de la nave).
// Visual detallada, colision de una sola caja.
void rack(Builders& b, float x0, float z0, float x1, float z1) {
    beginObject(b);
    b.collision.add({{x0, 0.0f, z0}, {x1, 2.6f, z1}}, kSolid);
    for (float y : {0.1f, 1.3f, 2.5f}) b.visual.box(kSteel, kSteel, {x0, y, z0}, {x1, y + 0.1f, z1});
    for (float px : {x0, x1 - 0.12f})
        for (float pz : {z0, z1 - 0.12f}) b.visual.box(kSteel, kSteel, {px, 0, pz}, {px + 0.12f, 2.6f, pz + 0.12f});
    const float midX = (x0 + x1) * 0.5f;
    b.visual.box(kWood, kWood, {x0 + 0.1f, 0.2f, z0 + 0.1f}, {midX - 0.1f, 1.2f, z1 - 0.1f});
    b.visual.box(kWood, kWood, {midX + 0.1f, 1.4f, z0 + 0.1f}, {x1 - 0.1f, 2.3f, z1 - 0.1f});
    endObject(b, kSmallMetalYield);
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
    beginObject(b);
    solid(b, kShutter, kShutter, {xm - 4.0f, 4.6f, z1 - 0.3f}, {xm + 4.0f, 6.5f, z1 - 0.15f});
    endObject(b, kFenceYield);
    beginObject(b);
    solid(b, kTrim, kTrim, {xm - 4.4f, 0.0f, z1}, {xm - 4.0f, 6.9f, z1 + 0.3f});
    solid(b, kTrim, kTrim, {xm + 4.0f, 0.0f, z1}, {xm + 4.4f, 6.9f, z1 + 0.3f});
    b.visual.box(kTrim, kTrim, {xm - 4.4f, 6.5f, z1}, {xm + 4.4f, 6.9f, z1 + 0.3f});
    endObject(b, kFenceYield);
    // Zocalo de hormigon exterior en los laterales (solo visual, 5 cm por fuera).
    b.visual.quad(kConcrete, {x0 - 0.05f, 0, z0 + 0.2f}, {x0 - 0.05f, 0, z1 - 0.2f},
                  {x0 - 0.05f, 1.0f, z1 - 0.2f}, {x0 - 0.05f, 1.0f, z0 + 0.2f});
    b.visual.quad(kConcrete, {x1 + 0.05f, 0, z1 - 0.2f}, {x1 + 0.05f, 0, z0 + 0.2f},
                  {x1 + 0.05f, 1.0f, z0 + 0.2f}, {x1 + 0.05f, 1.0f, z1 - 0.2f});

    gambrelRoof(b, x0, x1, z0, z1, h, roofPanels(rgba(96, 100, 106)), outer);

    // Altillo al fondo: suelo a 3.6 m, apoyado en pilares, con barandilla y escalera.
    const float mezzFront = z0 + 6.0f, mezzY = 3.6f;
    const float stairX0 = x0 + t, stairX1 = x0 + t + 1.6f;
    beginObject(b);
    solid(b, kSteel, kConcrete, {x0 + t, mezzY - 0.3f, z0 + t}, {x1 - t, mezzY, mezzFront});
    endObject(b, kPanelYield);
    for (float px : {x0 + 4.0f, xm, x1 - 3.0f}) {
        beginObject(b);
        solid(b, kSteel, kSteel, {px - 0.15f, 0.0f, mezzFront - 0.35f}, {px + 0.15f, mezzY - 0.3f, mezzFront - 0.05f});
        endObject(b, kFenceYield);
    }
    // Barandilla: bloquea el paso (no caerse) pero no los disparos.
    beginObject(b);
    b.collision.add({{stairX1, mezzY, mezzFront - 0.08f}, {x1 - t, mezzY + 1.0f, mezzFront}}, kBlocksMovement);
    b.visual.box(kDarkTrim, kDarkTrim, {stairX1, mezzY + 0.9f, mezzFront - 0.08f}, {x1 - t, mezzY + 1.0f, mezzFront});
    b.visual.box(kDarkTrim, kDarkTrim, {stairX1, mezzY + 0.45f, mezzFront - 0.06f}, {x1 - t, mezzY + 0.52f, mezzFront - 0.02f});
    for (float px = stairX1; px < x1 - t; px += 2.0f) {
        b.visual.box(kDarkTrim, kDarkTrim, {px, mezzY, mezzFront - 0.08f}, {px + 0.08f, mezzY + 1.0f, mezzFront});
    }
    endObject(b, kFenceYield);
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
            beginObject(b);
            solid(b, flat(rgba(214, 170, 40)), flat(rgba(214, 170, 40)), {xm + 2.5f, 0, -5.0f}, {xm + 3.7f, 2.2f, -3.0f});
            endObject(b, kSmallMetalYield);
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
// En tramos de 4 m como mucho, cada uno un objeto.
void chainFenceAlongX(Builders& b, float z, float x0, float x1) {
    if (x1 - x0 < 0.1f) return;
    panels(x0, x1, [&](float a, float c) {
        beginObject(b);
        b.collision.add({{a, 0.0f, z - 0.05f}, {c, 2.4f, z + 0.05f}}, kBlocksMovement);
        for (float x : {a, c}) b.visual.box(kSteel, kSteel, {x - 0.06f, 0, z - 0.06f}, {x + 0.06f, 2.4f, z + 0.06f});
        for (float y : {0.3f, 1.2f, 2.3f}) {
            b.visual.quad(kSteel, {a, y, z}, {c, y, z}, {c, y + 0.08f, z}, {a, y + 0.08f, z});
        }
        endObject(b, kFenceYield);
    });
}

void lampPost(Builders& b, float x, float z, float armDirX) {
    beginObject(b);
    solid(b, kDarkTrim, kDarkTrim, {x - 0.12f, 0, z - 0.12f}, {x + 0.12f, 6.5f, z + 0.12f});
    b.visual.box(kDarkTrim, kDarkTrim, {fminf(x, x + armDirX * 1.6f), 6.3f, z - 0.08f},
                 {fmaxf(x, x + armDirX * 1.6f), 6.45f, z + 0.08f});
    b.visual.box(flat(rgba(240, 236, 200)), kDarkTrim, {x + armDirX * 1.6f - 0.25f, 6.0f, z - 0.15f},
                 {x + armDirX * 1.6f + 0.25f, 6.3f, z + 0.15f});
    endObject(b, kFenceYield);
}

void boxTrailer(Builders& b, float x0, float z0, float x1, float z1) {
    beginObject(b);
    b.collision.add({{x0, 0.0f, z0}, {x1, 3.6f, z1}}, kSolid);
    const Material skin = containerSkin(rgba(210, 210, 204));
    b.visual.box(skin, skin, {x0, 1.0f, z0}, {x1, 3.6f, z1}, true);
    b.visual.box(kDarkTrim, kDarkTrim, {x0 + 0.5f, 0.6f, z0 + 0.3f}, {x1 - 0.5f, 1.0f, z1 - 0.3f});
    for (float wx : {x0 + 1.2f, x0 + 2.4f, x1 - 1.6f}) {
        b.visual.box(kDarkTrim, kDarkTrim, {wx - 0.45f, 0, z0 + 0.1f}, {wx + 0.45f, 0.9f, z0 + 0.4f});
        b.visual.box(kDarkTrim, kDarkTrim, {wx - 0.45f, 0, z1 - 0.4f}, {wx + 0.45f, 0.9f, z1 - 0.1f});
    }
    endObject(b, kMetalYield);
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
        if (x1 - x0 <= 0.01f) return;
        panels(x0, x1, [&](float a, float c) {
            beginObject(b);
            solid(b, kBlockWall, kConcrete, {a, 0, z}, {c, wallH, z + 0.4f});
            endObject(b, kBlockWallYield);
        });
    };
    auto wallZ = [&](float z0, float z1, float x) {
        if (z1 - z0 <= 0.01f) return;
        panels(z0, z1, [&](float a, float c) {
            beginObject(b);
            solid(b, kBlockWall, kConcrete, {x, 0, a}, {x + 0.4f, wallH, c});
            endObject(b, kBlockWallYield);
        });
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
    beginObject(b);  // contenedor de basura
    solid(b, flat(rgba(60, 110, 70)), flat(rgba(50, 90, 60)), {6.0f, 0, -30.0f}, {8.5f, 1.4f, -28.6f});
    endObject(b, kSmallMetalYield);
    b.visual.flushGroup(out);

    pois.push_back({0.0f, 0.0f, 9.0f});
    pois.push_back({-10.0f, 0.0f, -14.0f});
    pois.push_back({10.0f, 0.0f, -14.0f});
    pois.push_back({0.0f, 0.0f, -29.0f});
}

// --- Naturaleza ----------------------------------------------------------------------------

void pineTree(Builders& b, float x, float z, float s) {
    beginObject(b);
    solid(b, kTrunk, kTrunk, {x - 0.3f * s, 0, z - 0.3f * s}, {x + 0.3f * s, 2.2f * s, z + 0.3f * s});
    b.visual.cone(flat(rgba(50, 104, 52)), {x, 1.8f * s, z}, 2.6f * s, 4.2f * s, 6);
    b.visual.cone(flat(rgba(62, 124, 60)), {x, 4.4f * s, z}, 1.8f * s, 3.2f * s, 6);
    endObject(b, kTreeYield);
}

// Arbol de copa cubica, como los de Recursos/Loadscreen.png. La copa no colisiona.
void blockTree(Builders& b, float x, float z, float s) {
    beginObject(b);
    solid(b, kTrunk, kTrunk, {x - 0.3f * s, 0, z - 0.3f * s}, {x + 0.3f * s, 3.0f * s, z + 0.3f * s});
    const Material leaves = flat(rgba(72, 132, 52));
    const Material leavesTop = flat(rgba(92, 156, 62));
    b.visual.box(leaves, leavesTop, {x - 2.2f * s, 2.6f * s, z - 2.2f * s}, {x + 2.2f * s, 4.8f * s, z + 2.2f * s}, true);
    b.visual.box(leaves, leavesTop, {x - 1.4f * s, 4.8f * s, z - 1.4f * s}, {x + 1.4f * s, 6.0f * s, z + 1.4f * s});
    endObject(b, kTreeYield);
}

void boulder(Builders& b, float x, float z, float w, float d, float h) {
    beginObject(b);
    solid(b, kRock, kRock, {x - w * 0.5f, 0, z - d * 0.5f}, {x + w * 0.5f, h, z + d * 0.5f});
    b.visual.box(kRock, kRock, {x - w * 0.3f, h, z - d * 0.25f}, {x + w * 0.2f, h + 0.5f, z + d * 0.3f});
    endObject(b, kRockYield);
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
    // El agua se dibuja en buildGround() y no tiene colision (v0.14): se cruza andando, mas
    // despacio (world/water.h). Antes bloqueaba el paso.
    // Rocas en la orilla como cobertura.
    boulder(b, kLakeX + 22.0f, kLakeZ - 4.0f, 2.6f, 2.0f, 1.6f);
    boulder(b, kLakeX - 8.0f, kLakeZ - 15.5f, 3.2f, 2.2f, 1.8f);
    boulder(b, kLakeX + 6.0f, kLakeZ + 15.0f, 2.4f, 2.4f, 1.4f);
    b.visual.flushGroup(out);
}

bool nearZombieSite(float x, float z, float radius);

void forest(Builders& b, std::vector<StaticBatch>& out) {
    // Rejilla de 7 m con jitter al oeste del deposito; un lote cada dos filas.
    int row = 0;
    for (float z = -90.0f; z <= kForestMaxZ; z += 7.0f, ++row) {
        int col = 0;
        for (float x = -91.0f; x <= kForestMaxX; x += 7.0f, ++col) {
            if (hash01(col, row, 601) < 0.18f) continue;  // claros
            const float px = x + (hash01(col, row, 602) - 0.5f) * 4.0f;
            const float pz = z + (hash01(col, row, 603) - 0.5f) * 4.0f;
            if (nearZombieSite(px, pz, 7.0f)) continue;  // claro de un campamento (v0.14)
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

// --- Modo zombi (v0.14, docs/ZOMBIS.md) ---------------------------------------------------
// La base abandonada donde despierta el jugador, los campamentos de los supervivientes y los
// centros de crafteo. Son objetos del mapa como los demas (se pueden picar), pero marcados
// `zombieOnly`: la partida los quita en el resto de modos (no colisionan ni se dibujan).

// Cerca de un lugar del modo zombi (alli no crecen arboles: es un claro).
bool nearZombieSite(float x, float z, float radius) {
    auto near = [&](const Vec3& p) { return (p.x - x) * (p.x - x) + (p.z - z) * (p.z - z) < radius * radius; };
    for (const Vec3& p : zsites::kCamps) if (near(p)) return true;
    for (const Vec3& p : zsites::kWorkbenches) if (near(p)) return true;
    return zsites::insideBase(x, z, radius);
}

const Material kRustSheet = textured(kTexRustSiding, rgba(196, 150, 120), 2.0f);
const Material kCanvas = textured(kTexCanvas, rgba(168, 156, 118), 2.0f);
const Material kSandbag = textured(kTexCanvas, rgba(176, 156, 112), 1.0f);
const Material kRotten = textured(kTexRottenWood, kWhite, 1.2f);
const Material kAsh = flat(rgba(52, 46, 42));
const Material kEmber = flat(rgba(236, 110, 40));

// Tienda de campana: lona a dos aguas sobre un suelo, con la entrada oscura.
void tent(Builders& b, float x, float z, bool alongX) {
    beginObject(b);
    const float hl = 1.3f, hw = 1.1f, h = 1.6f;
    const float ax = alongX ? hl : hw, az = alongX ? hw : hl;
    b.collision.add({{x - ax, 0.0f, z - az}, {x + ax, h, z + az}}, kSolid);
    if (alongX) {
        b.visual.quad(kCanvas, {x - hl, 0, z + hw}, {x + hl, 0, z + hw}, {x + hl, h, z}, {x - hl, h, z});
        b.visual.quad(kCanvas, {x + hl, 0, z - hw}, {x - hl, 0, z - hw}, {x - hl, h, z}, {x + hl, h, z});
        b.visual.triangle(kCanvas, {x + hl, 0, z + hw}, {x + hl, 0, z - hw}, {x + hl, h, z});
        b.visual.triangle(flat(rgba(30, 26, 22)), {x - hl, 0, z - hw}, {x - hl, 0, z + hw}, {x - hl, h, z});
    } else {
        b.visual.quad(kCanvas, {x + hw, 0, z + hl}, {x + hw, 0, z - hl}, {x, h, z - hl}, {x, h, z + hl});
        b.visual.quad(kCanvas, {x - hw, 0, z - hl}, {x - hw, 0, z + hl}, {x, h, z + hl}, {x, h, z - hl});
        b.visual.triangle(kCanvas, {x - hw, 0, z - hl}, {x + hw, 0, z - hl}, {x, h, z - hl});
        b.visual.triangle(flat(rgba(30, 26, 22)), {x + hw, 0, z + hl}, {x - hw, 0, z + hl}, {x, h, z + hl});
    }
    b.visual.box(kTrunk, kTrunk, {x - 0.05f, 0, z - 0.05f}, {x + 0.05f, h + 0.15f, z + 0.05f});
    endObject(b, kCampWoodYield);
}

// Hoguera apagada: piedras en corro, troncos y brasas (se pisa por encima: 0.25 m).
void campfire(Builders& b, float x, float z) {
    beginObject(b);
    b.collision.add({{x - 0.5f, 0.0f, z - 0.5f}, {x + 0.5f, 0.25f, z + 0.5f}}, kBlocksMovement);
    for (int i = 0; i < 6; ++i) {
        const float a = i * (math3d::kPi / 3.0f);
        const float sx = x + cosf(a) * 0.45f, sz = z + sinf(a) * 0.45f;
        b.visual.box(kRock, kRock, {sx - 0.12f, 0, sz - 0.1f}, {sx + 0.12f, 0.18f, sz + 0.1f});
    }
    b.visual.box(kAsh, kAsh, {x - 0.3f, 0, z - 0.3f}, {x + 0.3f, 0.04f, z + 0.3f});
    b.visual.box(kTrunk, kTrunk, {x - 0.32f, 0.04f, z - 0.06f}, {x + 0.32f, 0.16f, z + 0.06f});
    b.visual.box(kTrunk, kTrunk, {x - 0.06f, 0.10f, z - 0.30f}, {x + 0.06f, 0.22f, z + 0.30f});
    b.visual.box(kEmber, kEmber, {x - 0.1f, 0.05f, z - 0.1f}, {x + 0.1f, 0.09f, z + 0.1f});
    endObject(b, kCampWoodYield);
}

// Catre de madera podrida con una manta.
void cot(Builders& b, float x, float z, bool alongX) {
    beginObject(b);
    const float ax = alongX ? 1.0f : 0.45f, az = alongX ? 0.45f : 1.0f;
    b.collision.add({{x - ax, 0.0f, z - az}, {x + ax, 0.5f, z + az}}, kSolid);
    b.visual.box(kRotten, kRotten, {x - ax, 0.32f, z - az}, {x + ax, 0.42f, z + az});
    b.visual.box(kCanvas, kCanvas, {x - ax * 0.85f, 0.42f, z - az * 0.85f}, {x + ax * 0.85f, 0.5f, z + az * 0.85f});
    for (float sx : {-1.0f, 1.0f})
        for (float sz : {-1.0f, 1.0f})
            b.visual.box(kRotten, kRotten, {x + sx * (ax - 0.05f) - 0.05f, 0, z + sz * (az - 0.05f) - 0.05f},
                         {x + sx * (ax - 0.05f) + 0.05f, 0.32f, z + sz * (az - 0.05f) + 0.05f});
    endObject(b, kCampWoodYield);
}

// Bidon oxidado.
void barrel(Builders& b, float x, float z) {
    beginObject(b);
    b.collision.add({{x - 0.35f, 0.0f, z - 0.35f}, {x + 0.35f, 1.0f, z + 0.35f}}, kSolid);
    b.visual.cylinder(kRustSheet, flat(rgba(92, 60, 44)), {x, 0.0f, z}, 0.35f, 1.0f, 8);
    b.visual.cylinder(flat(rgba(70, 44, 32)), flat(rgba(70, 44, 32)), {x, 0.3f, z}, 0.37f, 0.06f, 8, false);
    b.visual.cylinder(flat(rgba(70, 44, 32)), flat(rgba(70, 44, 32)), {x, 0.7f, z}, 0.37f, 0.06f, 8, false);
    endObject(b, kCampMetalYield);
}

// Muro bajo de sacos terreros (de x0 a x1 a lo largo de X, o de Z).
void sandbags(Builders& b, float x0, float z0, float x1, float z1) {
    beginObject(b);
    b.collision.add({{x0, 0.0f, z0}, {x1, 0.95f, z1}}, kSolid);
    const bool alongX = x1 - x0 > z1 - z0;
    const float len = alongX ? x1 - x0 : z1 - z0;
    const int n = static_cast<int>(len / 0.6f);
    for (int row = 0; row < 3; ++row) {
        const float y0 = row * 0.31f, off = (row % 2) * 0.3f;
        for (int i = 0; i < n; ++i) {
            const float a = i * 0.6f + off, c = fminf(a + 0.58f, len);
            if (a >= len - 0.05f) break;
            if (alongX) b.visual.box(kSandbag, kSandbag, {x0 + a, y0, z0 + 0.03f}, {x0 + c, y0 + 0.3f, z1 - 0.03f});
            else b.visual.box(kSandbag, kSandbag, {x0 + 0.03f, y0, z0 + a}, {x1 - 0.03f, y0 + 0.3f, z0 + c});
        }
    }
    endObject(b, kCampWoodYield);
}

// Centro de crafteo: banco de trabajo con tornillo y herramientas bajo un tejadillo de
// chapa oxidada, y una caja de chatarra al lado.
void workbench(Builders& b, float x, float z) {
    beginObject(b);
    b.collision.add({{x - 0.95f, 0.0f, z - 0.5f}, {x + 0.95f, 0.9f, z + 0.5f}}, kSolid);
    b.visual.box(kRotten, kRotten, {x - 0.95f, 0.78f, z - 0.5f}, {x + 0.95f, 0.9f, z + 0.5f});
    for (float sx : {-0.85f, 0.85f})
        for (float sz : {-0.4f, 0.4f})
            b.visual.box(kSteel, kSteel, {x + sx - 0.05f, 0, z + sz - 0.05f}, {x + sx + 0.05f, 0.78f, z + sz + 0.05f});
    b.visual.box(kRotten, kRotten, {x - 0.85f, 0.25f, z - 0.4f}, {x + 0.85f, 0.31f, z + 0.4f});
    // Tornillo de banco, sierra y llave.
    b.visual.box(flat(rgba(60, 90, 140)), kDarkTrim, {x + 0.45f, 0.9f, z - 0.25f}, {x + 0.75f, 1.08f, z + 0.05f});
    b.visual.box(kSteel, kSteel, {x - 0.6f, 0.9f, z - 0.1f}, {x - 0.05f, 0.92f, z + 0.12f});
    b.visual.box(kTrunk, kTrunk, {x - 0.75f, 0.9f, z - 0.05f}, {x - 0.6f, 0.95f, z + 0.08f});
    b.visual.box(flat(rgba(200, 60, 40)), kDarkTrim, {x - 0.2f, 0.9f, z + 0.2f}, {x + 0.15f, 0.96f, z + 0.3f});
    // Postes y tejadillo de chapa.
    for (float sx : {-1.1f, 1.1f}) {
        b.visual.box(kRustSheet, kRustSheet, {x + sx - 0.06f, 0, z + 0.55f}, {x + sx + 0.06f, 2.5f, z + 0.67f});
    }
    b.visual.quad(kRustSheet, {x - 1.4f, 2.25f, z - 1.0f}, {x + 1.4f, 2.25f, z - 1.0f}, {x + 1.4f, 2.6f, z + 0.8f},
                  {x - 1.4f, 2.6f, z + 0.8f});
    // Panel de herramientas detras del banco.
    b.visual.quad(kRotten, {x - 1.0f, 1.0f, z + 0.62f}, {x + 1.0f, 1.0f, z + 0.62f}, {x + 1.0f, 2.0f, z + 0.62f},
                  {x - 1.0f, 2.0f, z + 0.62f});
    endObject(b, kCampMetalYield);
    beginObject(b);
    solid(b, kRustSheet, flat(rgba(80, 70, 60)), {x + 1.25f, 0, z - 0.4f}, {x + 1.95f, 0.7f, z + 0.4f});
    endObject(b, kCampMetalYield);
}

void zombieSites(Builders& b, std::vector<StaticBatch>& out) {
    b.zombieOnly = true;
    // Base abandonada (x 56..68, z 28..40). Libres: las lineas del perimetro (las paredes de
    // los planos) y la celda de la rampa de la torre (x 64..68, z 32..36).
    cot(b, 58.4f, 29.6f, true);
    tent(b, 58.3f, 37.6f, true);
    campfire(b, 62.0f, 34.2f);
    barrel(b, 66.9f, 29.0f);
    barrel(b, 66.9f, 30.1f);
    barrel(b, 65.8f, 29.0f);
    beginObject(b);  // mesa con el mapa de los planos
    solid(b, kRotten, kRotten, {60.6f, 0, 29.1f}, {62.4f, 0.85f, 30.1f});
    b.visual.box(flat(rgba(70, 120, 190)), flat(rgba(70, 120, 190)), {60.9f, 0.85f, 29.3f}, {61.9f, 0.87f, 29.9f});
    endObject(b, kCampWoodYield);
    crate(b, 57.2f, 33.6f);
    crate(b, 57.2f, 34.8f);
    sandbags(b, 69.0f, 30.0f, 69.6f, 36.0f);  // fuera, junto al muro este
    b.visual.flushGroup(out);
    // Campamentos: tienda, hoguera, sacos terreros y una caja (el superviviente espera en
    // el centro, junto a la hoguera).
    for (const Vec3& c : zsites::kCamps) {
        tent(b, c.x - 2.0f, c.z - 1.5f, true);
        campfire(b, c.x + 1.8f, c.z - 1.2f);
        sandbags(b, c.x - 3.0f, c.z + 2.3f, c.x + 1.0f, c.z + 2.9f);
        crate(b, c.x + 2.6f, c.z + 1.6f);
        b.visual.flushGroup(out);
    }
    for (const Vec3& w : zsites::kWorkbenches) {
        workbench(b, w.x, w.z);
        b.visual.flushGroup(out);
    }
    b.zombieOnly = false;
}

}  // namespace

namespace {
u32 mapColorBase(float x, float z) {
    // Tejados de las naves con su color (como se verian desde el aire).
    if (insideWarehouse(x, z)) {
        if (x < -10.0f) return rgba(58, 82, 168);
        if (x < 10.0f) return rgba(128, 132, 130);
        return rgba(156, 72, 52);
    }
    const float mx = x - kMesaX, mz = z - kMesaZ;
    if (mx * mx + mz * mz < 20.0f * 20.0f) return rgba(150, 128, 108);
    if (x < kForestMaxX && z < kForestMaxZ) return rgba(44, 92, 40);
    switch (groundAt(x, z)) {
        case GroundType::Concrete: return rgba(170, 168, 160);
        case GroundType::Dirt: return rgba(146, 112, 74);
        case GroundType::Water: return rgba(56, 110, 168);
        case GroundType::Sand: return rgba(214, 196, 150);
        case GroundType::Grass: break;
    }
    return rgba(78, 130, 54);
}
}  // namespace

u32 mapColorAt(float x, float z) {
    const u32 c = mapColorBase(x, z);
    if (stormDepth(x, z) <= 0.0f) return c;
    // Tormenta (limite del mapa): morado sobre el terreno.
    auto mix = [&](int shift, u32 storm) {
        return (((c >> shift) & 0xFF) * 2 + storm) / 3;
    };
    return rgba(mix(0, 140), mix(8, 60), mix(16, 190));
}

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
                        case GroundType::Water: {
                            // Superficie algo por encima del fondo (los pies quedan bajo el
                            // agua) y, en la orilla, el canto del agua hasta el suelo.
                            b.groundTile(water, x0, z0, kTileSize, 0.95f + 0.05f * jitter, kWaterLevel);
                            const float x1 = x0 + kTileSize, z1 = z0 + kTileSize, h = kWaterLevel;
                            if (groundAt(mx, mz - kTileSize) != GroundType::Water)
                                b.quad(water, {x1, 0, z0}, {x0, 0, z0}, {x0, h, z0}, {x1, h, z0});
                            if (groundAt(mx, mz + kTileSize) != GroundType::Water)
                                b.quad(water, {x0, 0, z1}, {x1, 0, z1}, {x1, h, z1}, {x0, h, z1});
                            if (groundAt(mx - kTileSize, mz) != GroundType::Water)
                                b.quad(water, {x0, 0, z0}, {x0, 0, z1}, {x0, h, z1}, {x0, h, z0});
                            if (groundAt(mx + kTileSize, mz) != GroundType::Water)
                                b.quad(water, {x1, 0, z1}, {x1, 0, z0}, {x1, h, z0}, {x1, h, z1});
                            break;
                        }
                        case GroundType::Sand:
                            b.groundTile(sand, x0, z0, kTileSize, 0.92f + 0.08f * jitter);
                            break;
                    }
                }
            }
            b.flushGroup(out);
        }
    }

    // Suelo mas alla del mapa (hasta kWorldExtent): queda dentro de la tormenta y de su
    // niebla, asi nunca se ve donde acaba el mundo. Losas de 4 m en bloques de 16 m.
    const int outerChunks = static_cast<int>(2.0f * kWorldExtent / kChunkSize);
    for (int cz = 0; cz < outerChunks; ++cz) {
        for (int cx = 0; cx < outerChunks; ++cx) {
            const float x0 = -kWorldExtent + cx * kChunkSize, z0 = -kWorldExtent + cz * kChunkSize;
            const bool insideMap = x0 >= -kMapHalfSize && x0 + kChunkSize <= kMapHalfSize && z0 >= -kMapHalfSize &&
                                   z0 + kChunkSize <= kMapHalfSize;
            if (insideMap) continue;
            constexpr float tile = 4.0f;
            for (int tz = 0; tz < static_cast<int>(kChunkSize / tile); ++tz) {
                for (int tx = 0; tx < static_cast<int>(kChunkSize / tile); ++tx) {
                    const float x = x0 + tx * tile, z = z0 + tz * tile;
                    if (x >= -kMapHalfSize && x + tile <= kMapHalfSize && z >= -kMapHalfSize && z + tile <= kMapHalfSize) {
                        continue;
                    }
                    const int ix = cx * 4 + tx, iz = cz * 4 + tz;
                    b.groundTile(grass, x, z, tile, 0.82f + 0.16f * hash01(ix, iz, 9));
                }
            }
            b.flushGroup(out);
        }
    }
}

void buildStormWall(SceneBuilder& b, std::vector<StaticBatch>& out) {
    // Cortina morada en el borde de la zona segura (cuadrado de esquinas redondeadas):
    // mas opaca abajo y mas transparente arriba. Puntos cada ~4 m del contorno.
    std::vector<Vec3> contour;
    const float inner = kStormEdge - kStormCorner;
    const int straight = static_cast<int>(2.0f * inner / 4.0f);
    const int arc = 6;
    const float cornerX[4] = {inner, -inner, -inner, inner}, cornerZ[4] = {-inner, -inner, inner, inner};
    for (int side = 0; side < 4; ++side) {
        // Arco de la esquina y tramo recto hasta la siguiente (sentido antihorario visto
        // desde arriba: este -> norte -> oeste -> sur).
        const float a0 = side * 0.5f * math3d::kPi;
        for (int k = 0; k < arc; ++k) {
            const float a = a0 + 0.5f * math3d::kPi * k / arc;
            contour.push_back({cornerX[side] + cosf(a) * kStormCorner, 0.0f, cornerZ[side] - sinf(a) * kStormCorner});
        }
        const int next = (side + 1) % 4;
        const float a1 = a0 + 0.5f * math3d::kPi;
        const Vec3 from{cornerX[side] + cosf(a1) * kStormCorner, 0.0f, cornerZ[side] - sinf(a1) * kStormCorner};
        const Vec3 to{cornerX[next] + cosf(a1) * kStormCorner, 0.0f, cornerZ[next] - sinf(a1) * kStormCorner};
        for (int k = 0; k < straight; ++k) contour.push_back(from + (to - from) * (static_cast<float>(k) / straight));
    }
    const float bands[] = {0.0f, 3.0f, 8.0f, 15.0f, kStormWallHeight};
    const u32 alpha[] = {150, 128, 100, 70};
    const int n = static_cast<int>(contour.size());
    for (int i = 0; i < n; ++i) {
        const Vec3& p0 = contour[i];
        const Vec3& p1 = contour[(i + 1) % n];
        for (int band = 0; band < 4; ++band) {
            const Material m = flat(rgba(146, 72, 208, alpha[band]));
            const Vec3 lo{0.0f, bands[band], 0.0f}, hi{0.0f, bands[band + 1], 0.0f};
            b.quad(m, p0 + lo, p1 + lo, p1 + hi, p0 + hi);
        }
        if (i % 6 == 5) b.flushGroup(out);  // trozos de ~24 m: se descartan por separado
    }
    b.flushGroup(out);
}

void buildStormCircle(SceneBuilder& b, std::vector<StaticBatch>& out) {
    // Mismo color y franjas que el muro del borde, en un cilindro de radio 1 visto desde
    // dentro y desde fuera (sin descarte de caras). Lotes de 8 lados: se descartan por separado.
    constexpr int kSides = 128;
    const float bands[] = {0.0f, 1.5f, 3.0f, 5.0f, 8.0f, 11.5f, 15.0f, 21.0f, kStormWallHeight};
    const u32 alpha[] = {150, 140, 128, 115, 100, 85, 70, 55};
    for (int i = 0; i < kSides; ++i) {
        const float a0 = 2.0f * math3d::kPi * i / kSides, a1 = 2.0f * math3d::kPi * (i + 1) / kSides;
        const Vec3 p0{cosf(a0), 0.0f, sinf(a0)}, p1{cosf(a1), 0.0f, sinf(a1)};
        for (int band = 0; band < 8; ++band) {
            const Material m = flat(rgba(146, 72, 208, alpha[band]));
            const Vec3 lo{0.0f, bands[band], 0.0f}, hi{0.0f, bands[band + 1], 0.0f};
            b.quad(m, p0 + lo, p1 + lo, p1 + hi, p0 + hi);
        }
        if (i % 8 == 7) b.flushGroup(out);
    }
    b.flushGroup(out);
}

void buildStructures(SceneBuilder& visual, CollisionWorld& collision, std::vector<StaticBatch>& out,
                     std::vector<Vec3>& pois, std::vector<Harvestable>& harvestables) {
    Builders b{visual, collision, &harvestables, &out};
    buildDepot(b, out, pois);
    mesa(b, out);
    lake(b, out);
    forest(b, out);
    scatteredTrees(b, out);
    zombieSites(b, out);

    pois.push_back({kLakeX + 4.0f, 0.0f, kLakeZ - kLakeRz - 5.0f});   // orilla norte del lago
    pois.push_back({-62.0f, 0.0f, -20.0f});                            // bosque
    pois.push_back({kMesaX - 26.0f, 0.0f, kMesaZ + 16.0f});           // pie de la meseta
    pois.push_back({southRoadX(50.0f), 0.0f, 50.0f});                  // camino sur
    pois.push_back({62.0f, 0.0f, -6.0f});                              // camino este
}

}  // namespace world
