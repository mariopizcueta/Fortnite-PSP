#include "app/prop_visuals.h"

#include <math.h>

#include <initializer_list>

#include "gfx/graphics.h"

namespace app {

using gfx::rgba;
using math3d::Mat4;
using math3d::Vec3;
using world::Material;
using world::SceneBuilder;

namespace {

constexpr float kLootDrawDistance = 45.0f;       // objetos del suelo
constexpr float kContainerDrawDistance = 80.0f;  // cofres, cajas, airdrops
constexpr float kOpenedVisibleSeconds = 0.4f;     // un cofre abierto se ve asi y desaparece

Material flat(u32 c) { return {world::kTexNone, c, 1.0f}; }
Material textured(int tex, u32 c, float meters) { return {tex, c, meters}; }

// Cuadrilatero con la normal hacia fuera de `center` (la luz horneada depende de ella).
void outwardQuad(SceneBuilder& b, const Material& m, const Vec3& p0, const Vec3& p1, const Vec3& p2,
                 const Vec3& p3, const Vec3& center) {
    const Vec3 n = math3d::cross(p1 - p0, p3 - p0);
    if (math3d::dot(n, (p0 + p2) * 0.5f - center) >= 0.0f) b.quad(m, p0, p1, p2, p3);
    else b.quad(m, p3, p2, p1, p0);
}

// Esfera de meridianos y paralelos (globos). `stripe`: color de una franja central.
void sphere(SceneBuilder& b, const Material& m, const Material& stripe, const Vec3& c, float r, int rings,
            int segments, float squashY = 1.0f) {
    for (int i = 0; i < rings; ++i) {
        const float a0 = math3d::kPi * (static_cast<float>(i) / rings - 0.5f);
        const float a1 = math3d::kPi * (static_cast<float>(i + 1) / rings - 0.5f);
        const Material& mat = (i == rings / 2 || i == rings / 2 - 1) ? stripe : m;
        for (int k = 0; k < segments; ++k) {
            const float o0 = 2.0f * math3d::kPi * k / segments, o1 = 2.0f * math3d::kPi * (k + 1) / segments;
            auto p = [&](float lat, float lon) {
                return c + Vec3{cosf(lat) * cosf(lon) * r, sinf(lat) * r * squashY, cosf(lat) * sinf(lon) * r};
            };
            outwardQuad(b, mat, p(a0, o0), p(a0, o1), p(a1, o1), p(a1, o0), c);
        }
    }
}

void buildChest(SceneBuilder& b, bool open) {
    // Cofre de madera dorada con bandas metalicas y cerradura (Referencias/Chest...).
    const Material wood = textured(world::kTexWood, rgba(236, 178, 72), 0.8f);
    const Material band = flat(rgba(122, 88, 40));
    const Material lock = flat(rgba(214, 214, 204));
    b.box(wood, wood, {-0.45f, 0.0f, -0.30f}, {0.45f, 0.42f, 0.30f});
    b.box(band, band, {-0.47f, 0.0f, -0.32f}, {-0.39f, 0.43f, 0.32f});
    b.box(band, band, {0.39f, 0.0f, -0.32f}, {0.47f, 0.43f, 0.32f});
    if (open) {
        // Tapa abierta hacia atras, girando sobre la bisagra trasera.
        b.setTransform(Mat4::translation({0.0f, 0.42f, 0.32f}) * Mat4::rotationX(-1.75f) *
                       Mat4::translation({0.0f, -0.42f, -0.32f}));
        // Brillo dorado del interior.
        b.box(flat(rgba(255, 226, 120)), flat(rgba(255, 226, 120)), {-0.40f, 0.38f, -0.25f}, {0.40f, 0.41f, 0.25f});
    } else {
        b.box(lock, lock, {-0.06f, 0.26f, -0.34f}, {0.06f, 0.40f, -0.30f});
    }
    b.box(wood, wood, {-0.47f, 0.42f, -0.32f}, {0.47f, 0.62f, 0.32f});
    b.box(band, band, {-0.49f, 0.42f, -0.34f}, {-0.39f, 0.64f, 0.34f});
    b.box(band, band, {0.39f, 0.42f, -0.34f}, {0.49f, 0.64f, 0.34f});
    b.resetTransform();
}

void buildAmmoBox(SceneBuilder& b) {
    // Caja de municion verde oliva con franja amarilla y asa (Referencias/caja de balas).
    const Material olive = flat(rgba(92, 110, 54));
    b.box(olive, olive, {-0.38f, 0.0f, -0.25f}, {0.38f, 0.50f, 0.25f});
    b.box(flat(rgba(230, 200, 40)), flat(rgba(230, 200, 40)), {-0.39f, 0.38f, -0.26f}, {0.39f, 0.42f, 0.26f});
    b.box(flat(rgba(40, 40, 36)), flat(rgba(40, 40, 36)), {-0.16f, 0.50f, -0.03f}, {0.16f, 0.56f, 0.03f});
}

void buildAirdrop(SceneBuilder& b) {
    // Caja azul de suministros con aristas claras (Referencias/airdrop).
    const Material blue = flat(rgba(52, 86, 172));
    const Material edge = flat(rgba(220, 220, 214));
    b.box(blue, blue, {-0.65f, 0.0f, -0.65f}, {0.65f, 1.3f, 0.65f});
    for (float x : {-0.66f, 0.56f}) {
        for (float z : {-0.66f, 0.56f}) b.box(edge, edge, {x, 0.0f, z}, {x + 0.1f, 1.32f, z + 0.1f});
    }
    b.box(edge, edge, {-0.66f, 1.22f, -0.66f}, {0.66f, 1.32f, 0.66f});
}

void buildBalloon(SceneBuilder& b, float height, float radius, u32 color, u32 stripe) {
    sphere(b, flat(color), flat(stripe), {0.0f, height, 0.0f}, radius, 6, 10, 1.15f);
    // Cuerdas hasta la caja.
    const Material rope = flat(rgba(70, 64, 56));
    for (float x : {-0.5f, 0.45f}) {
        for (float z : {-0.5f, 0.45f}) b.box(rope, rope, {x, 1.3f, z}, {x + 0.05f, height - radius * 0.9f, z + 0.05f});
    }
}

void buildBus(SceneBuilder& b) {
    // Autobus de batalla azul con su globo (Referencias/fortnite-battle-bus...).
    const Material body = flat(rgba(44, 92, 196));
    const Material roof = flat(rgba(214, 222, 236));
    const Material window = flat(rgba(36, 46, 68));
    const Material dark = flat(rgba(30, 30, 34));
    b.box(body, roof, {-1.4f, 0.4f, -4.6f}, {1.4f, 3.0f, 4.6f}, true);
    for (float z = -3.6f; z < 3.8f; z += 1.5f) {
        b.box(window, window, {-1.45f, 1.7f, z}, {-1.38f, 2.6f, z + 1.1f});
        b.box(window, window, {1.38f, 1.7f, z}, {1.45f, 2.6f, z + 1.1f});
    }
    b.box(window, window, {-1.2f, 1.6f, -4.66f}, {1.2f, 2.7f, -4.58f});  // parabrisas
    b.box(flat(rgba(200, 200, 196)), dark, {-1.3f, 0.5f, -4.75f}, {1.3f, 1.2f, -4.6f});  // parachoques
    for (float x : {-1.45f, 1.05f}) {
        for (float z : {-3.4f, 2.6f}) b.box(dark, dark, {x, 0.0f, z}, {x + 0.4f, 0.9f, z + 0.9f});
    }
    sphere(b, flat(rgba(180, 214, 238)), flat(rgba(200, 60, 52)), {0.0f, 10.5f, 0.0f}, 4.2f, 8, 12, 1.2f);
    const Material rope = flat(rgba(60, 56, 50));
    for (float x : {-1.2f, 1.1f}) {
        for (float z : {-3.0f, 2.9f}) b.box(rope, rope, {x, 3.0f, z}, {x + 0.08f, 7.0f, z + 0.08f});
    }
}

void buildGlider(SceneBuilder& b) {
    // Ala delta azul (Referencias/glider): lona en V invertida y barra de agarre.
    const Material canvas = flat(rgba(76, 156, 214));
    const Material dark = flat(rgba(40, 52, 64));
    const Vec3 nose{0.0f, 0.15f, -0.9f}, tailL{-1.4f, -0.15f, 0.5f}, tailR{1.4f, -0.15f, 0.5f}, mid{0.0f, 0.25f, 0.5f};
    b.quad(canvas, nose, tailL, Vec3{-1.4f, -0.1f, 0.6f}, mid);
    b.quad(canvas, nose, mid, Vec3{1.4f, -0.1f, 0.6f}, tailR);
    b.box(dark, dark, {-0.5f, -0.9f, -0.05f}, {0.5f, -0.84f, 0.05f});  // barra
    b.box(dark, dark, {-0.04f, -0.9f, -0.04f}, {0.04f, 0.15f, 0.04f});  // mastil
}

void buildAmmo(SceneBuilder& b, u32 color) {
    const Material m = flat(color);
    b.box(m, m, {-0.13f, 0.0f, -0.09f}, {0.13f, 0.12f, 0.09f});
    b.box(flat(rgba(230, 200, 60)), flat(rgba(230, 200, 60)), {-0.135f, 0.05f, -0.095f}, {0.135f, 0.07f, 0.095f});
}

void buildBeam(SceneBuilder& b, u32 color) {
    // Dos planos cruzados: se ven igual desde cualquier lado.
    const Material m = flat(color);
    b.quad(m, {-0.08f, 0.0f, 0.0f}, {0.08f, 0.0f, 0.0f}, {0.08f, 1.7f, 0.0f}, {-0.08f, 1.7f, 0.0f});
    b.quad(m, {0.0f, 0.0f, -0.08f}, {0.0f, 0.0f, 0.08f}, {0.0f, 1.7f, 0.08f}, {0.0f, 1.7f, -0.08f});
}

void buildMaterialPile(SceneBuilder& b, int resource) {
    // Montones de materiales (como los iconos de Recursos/HUD: tablones, ladrillos, chapas).
    switch (resource) {
        case 0: {  // madera: tres tablones cruzados
            const Material wood = textured(world::kTexWood, rgba(196, 140, 86), 0.8f);
            b.box(wood, wood, {-0.22f, 0.0f, -0.07f}, {0.22f, 0.06f, 0.0f});
            b.box(wood, wood, {-0.20f, 0.0f, 0.02f}, {0.24f, 0.06f, 0.09f});
            b.setTransform(Mat4::rotationY(0.7f));
            b.box(wood, wood, {-0.2f, 0.06f, -0.04f}, {0.2f, 0.12f, 0.04f});
            b.resetTransform();
            break;
        }
        case 1: {  // piedra: ladrillos
            const Material brick = flat(rgba(170, 96, 70));
            b.box(brick, brick, {-0.18f, 0.0f, -0.08f}, {0.0f, 0.08f, 0.08f});
            b.box(brick, brick, {0.02f, 0.0f, -0.08f}, {0.20f, 0.08f, 0.08f});
            b.box(brick, brick, {-0.08f, 0.08f, -0.08f}, {0.10f, 0.16f, 0.08f});
            break;
        }
        default: {  // metal: chapas onduladas
            const Material metal = flat(rgba(150, 156, 166));
            b.box(metal, metal, {-0.2f, 0.0f, -0.14f}, {0.2f, 0.03f, 0.14f});
            b.box(flat(rgba(120, 126, 136)), metal, {-0.16f, 0.03f, -0.10f}, {0.18f, 0.06f, 0.12f});
            break;
        }
    }
}

void buildLlama(SceneBuilder& b) {
    // Llama de suministros (Referencias/Supply_Llama): pinata morada con franjas azules y
    // rosas, cuello largo, orejas, y la bolsa con la caja en el costado. Mira hacia -Z.
    const Material purple = flat(rgba(150, 84, 206));
    const Material blue = flat(rgba(76, 128, 232));
    const Material pink = flat(rgba(232, 112, 196));
    const Material teal = flat(rgba(90, 200, 210));
    const Material dark = flat(rgba(40, 34, 50));
    const Material brown = flat(rgba(120, 86, 56));
    // Patas.
    for (float x : {-0.17f, 0.11f}) {
        for (float z : {-0.30f, 0.24f}) b.box(blue, blue, {x, 0.0f, z}, {x + 0.07f, 0.55f, z + 0.07f});
    }
    // Cuerpo en franjas (de abajo arriba).
    b.box(pink, pink, {-0.24f, 0.52f, -0.42f}, {0.24f, 0.66f, 0.42f}, true);
    b.box(blue, blue, {-0.25f, 0.66f, -0.44f}, {0.25f, 0.80f, 0.44f});
    b.box(purple, purple, {-0.24f, 0.80f, -0.42f}, {0.24f, 0.98f, 0.42f});
    b.box(teal, teal, {-0.06f, 0.88f, 0.42f}, {0.06f, 1.0f, 0.52f});  // cola
    // Cuello y cabeza.
    b.box(purple, purple, {-0.11f, 0.90f, -0.46f}, {0.11f, 1.38f, -0.24f});
    b.box(pink, pink, {-0.115f, 1.05f, -0.47f}, {0.115f, 1.14f, -0.23f});
    b.box(purple, purple, {-0.14f, 1.36f, -0.56f}, {0.14f, 1.58f, -0.22f});
    b.box(blue, blue, {-0.09f, 1.36f, -0.70f}, {0.09f, 1.50f, -0.54f});  // hocico
    b.box(dark, dark, {-0.145f, 1.47f, -0.48f}, {-0.135f, 1.52f, -0.42f});  // ojos
    b.box(dark, dark, {0.135f, 1.47f, -0.48f}, {0.145f, 1.52f, -0.42f});
    b.box(purple, pink, {-0.12f, 1.58f, -0.34f}, {-0.05f, 1.75f, -0.28f});  // orejas
    b.box(purple, pink, {0.05f, 1.58f, -0.34f}, {0.12f, 1.75f, -0.28f});
    // Bolsa en el costado derecho con la caja de suministros (azul claro, con dibujo).
    b.box(brown, brown, {0.25f, 0.62f, -0.18f}, {0.32f, 0.96f, 0.22f});
    b.box(flat(rgba(214, 222, 236)), flat(rgba(214, 222, 236)), {0.32f, 0.68f, -0.12f}, {0.36f, 0.92f, 0.16f});
    b.box(dark, dark, {0.36f, 0.74f, -0.04f}, {0.365f, 0.86f, 0.08f});
}

// Trampa de pinchos de 2 x 2 (de -1 a 1 en X y Z, normal +Y): se escala a cada superficie.
// Marco oscuro con franjas amarillas, placa clara con sus agujeros y 9 pinchos (Referencias/
// FortniteFloorTrap).
void buildTrapPlate(SceneBuilder& b) {
    const Material frame = flat(rgba(70, 66, 60));
    const Material yellow = flat(rgba(230, 186, 40));
    const Material plate = flat(rgba(214, 204, 180));
    const Material hole = flat(rgba(120, 100, 80));
    b.box(plate, plate, {-0.92f, 0.0f, -0.92f}, {0.92f, 0.05f, 0.92f}, true);
    b.box(frame, frame, {-1.0f, 0.0f, -1.0f}, {1.0f, 0.08f, -0.9f}, true);
    b.box(frame, frame, {-1.0f, 0.0f, 0.9f}, {1.0f, 0.08f, 1.0f}, true);
    b.box(frame, frame, {-1.0f, 0.0f, -0.9f}, {-0.9f, 0.08f, 0.9f}, true);
    b.box(frame, frame, {0.9f, 0.0f, -0.9f}, {1.0f, 0.08f, 0.9f}, true);
    b.box(yellow, yellow, {-0.6f, 0.08f, -0.98f}, {0.6f, 0.09f, -0.92f});
    b.box(yellow, yellow, {-0.6f, 0.08f, 0.92f}, {0.6f, 0.09f, 0.98f});
    b.box(yellow, yellow, {-0.98f, 0.08f, -0.6f}, {-0.92f, 0.09f, 0.6f});
    b.box(yellow, yellow, {0.92f, 0.08f, -0.6f}, {0.98f, 0.09f, 0.6f});
    for (int i = 0; i < 9; ++i) {
        const float x = -0.6f + 0.6f * (i % 3), z = -0.6f + 0.6f * (i / 3);
        b.box(hole, hole, {x - 0.08f, 0.05f, z - 0.08f}, {x + 0.08f, 0.06f, z + 0.08f});
    }
}

void buildTrapSpikes(SceneBuilder& b) {
    // Pinchos de altura 1 (se escalan con lo que han salido).
    const Material steel = flat(rgba(196, 200, 210));
    for (int i = 0; i < 9; ++i) {
        const float x = -0.6f + 0.6f * (i % 3), z = -0.6f + 0.6f * (i / 3);
        b.cone(steel, {x, 0.0f, z}, 0.06f, 1.0f, 4);
    }
}

const u32 kRarityBeam[5] = {rgba(200, 200, 200, 70), rgba(90, 220, 60, 80), rgba(70, 160, 255, 90),
                            rgba(190, 90, 255, 95), rgba(255, 170, 40, 105)};

}  // namespace

bool PropVisuals::build() {
    SceneBuilder b(3.0f);
    auto finish = [&](Mesh m) { b.flushGroup(m_meshes[m]); };
    buildChest(b, false);
    finish(kChest);
    buildChest(b, true);
    finish(kChestOpen);
    buildAmmoBox(b);
    finish(kAmmoBox);
    buildAirdrop(b);
    finish(kAirdrop);
    buildBalloon(b, 4.4f, 1.5f, rgba(196, 190, 136), rgba(214, 214, 206));
    finish(kBalloon);
    buildBus(b);
    finish(kBus);
    buildGlider(b);
    finish(kGlider);
    buildLlama(b);
    finish(kLlama);
    // Municion: ligera, media, pesada, cartuchos y cohetes (verde con franja).
    const u32 ammoColors[5] = {rgba(120, 140, 170), rgba(70, 130, 60), rgba(150, 40, 36), rgba(214, 90, 40),
                               rgba(84, 102, 62)};
    for (int i = 0; i < 5; ++i) {
        buildAmmo(b, ammoColors[i]);
        finish(static_cast<Mesh>(kAmmoLight + i));
    }
    for (int i = 0; i < 3; ++i) {
        buildMaterialPile(b, i);
        finish(static_cast<Mesh>(kMatWood + i));
    }
    for (int i = 0; i < 5; ++i) {
        buildBeam(b, kRarityBeam[i]);
        finish(static_cast<Mesh>(kBeam0 + i));
    }
    buildTrapPlate(b);
    finish(kTrapPlate);
    buildTrapSpikes(b);
    finish(kTrapSpikes);
    b.box(flat(rgba(90, 200, 255, 120)), flat(rgba(90, 200, 255, 120)), {-1.0f, 0.0f, -1.0f}, {1.0f, 0.1f, 1.0f}, true);
    finish(kTrapGhostValid);
    b.box(flat(rgba(255, 70, 60, 120)), flat(rgba(255, 70, 60, 120)), {-1.0f, 0.0f, -1.0f}, {1.0f, 0.1f, 1.0f}, true);
    finish(kTrapGhostInvalid);
    // Explosion: bola naranja con nucleo amarillo, en tres transparencias (se apaga).
    const u32 alphas[3] = {190, 120, 60};
    for (int i = 0; i < 3; ++i) {
        sphere(b, flat(rgba(255, 140, 40, alphas[i])), flat(rgba(255, 210, 90, alphas[i])), {0.0f, 0.0f, 0.0f}, 1.0f, 5, 8);
        finish(static_cast<Mesh>(kBlast0 + i));
    }
    m_triangles = 0;
    for (const Batches& list : m_meshes) {
        for (const world::StaticBatch& batch : list) m_triangles += batch.triangles;
    }
    return !b.failed();
}

void PropVisuals::destroy() {
    for (Batches& list : m_meshes) {
        for (world::StaticBatch& batch : list) gfx::destroyMesh(batch.mesh);
        list.clear();
    }
}

void PropVisuals::draw(Mesh mesh, const Mat4& model, const world::TextureLibrary* textures) const {
    gfx::setModelMatrix(model);
    for (const world::StaticBatch& batch : m_meshes[mesh]) {
        gfx::bindTexture(textures ? textures->get(batch.texture) : nullptr);
        gfx::drawMesh(batch.mesh);
    }
}

void PropVisuals::drawLoot(const game::Match& match, const math3d::Frustum& frustum, const Vec3& eye,
                           float maxDistance, float time, const world::TextureLibrary& textures,
                           const game::ItemModels& items) const {
    const float containerRange = fminf(maxDistance, kContainerDrawDistance);
    for (const game::Container& c : match.containers()) {
        // Abierto: se ve un instante con la tapa levantada y desaparece (como en Fortnite).
        if (c.opened && match.elapsed() - c.openedAt > kOpenedVisibleSeconds) continue;
        if (math3d::length(c.position - eye) > containerRange + 6.0f) continue;
        if (!frustum.sphereVisible(c.position + Vec3{0.0f, 1.5f, 0.0f}, c.kind == game::ContainerKind::Airdrop ? 6.0f : 1.0f)) continue;
        const Mat4 model = Mat4::translation(c.position) * Mat4::rotationY(c.yaw);
        switch (c.kind) {
            case game::ContainerKind::Chest: draw(c.opened ? kChestOpen : kChest, model, &textures); break;
            case game::ContainerKind::AmmoBox: draw(kAmmoBox, model, &textures); break;
            case game::ContainerKind::Airdrop:
                draw(kAirdrop, model, &textures);
                if (!c.landed) draw(kBalloon, model, &textures);
                break;
            case game::ContainerKind::Llama: {
                // Se balancea un poco (como la de Fortnite) y salta al recibir un golpe.
                const float sway = 0.04f * sinf(time * 2.2f);
                draw(kLlama, model * Mat4::rotationZ(sway), &textures);
                break;
            }
        }
    }
    const float lootRange = fminf(maxDistance, kLootDrawDistance);
    const std::vector<game::Pickup>& pickups = match.pickups();
    for (size_t i = 0; i < pickups.size(); ++i) {
        const game::Pickup& p = pickups[i];
        if (!p.active || math3d::length(p.position - eye) > lootRange) continue;
        if (!frustum.sphereVisible(p.position + Vec3{0.0f, 0.3f, 0.0f}, 0.6f)) continue;
        // Gira y sube y baja un poco (desfasado por objeto).
        const float phase = static_cast<float>(i) * 1.7f;
        const Vec3 at = p.position + Vec3{0.0f, 0.25f + 0.06f * sinf(time * 2.0f + phase), 0.0f};
        const Mat4 spin = Mat4::translation(at) * Mat4::rotationY(time * 1.3f + phase);
        if (p.kind == game::PickupKind::Ammo) {
            draw(static_cast<Mesh>(kAmmoLight + static_cast<int>(p.ammo)), spin, &textures);
        } else if (p.kind == game::PickupKind::Materials) {
            draw(static_cast<Mesh>(kMatWood + static_cast<int>(p.resource)), spin, &textures);
        } else {
            // Armas y consumibles: su modelo, girando sobre su centro.
            const game::ItemModel model = game::itemModelFor(p.item, p.rarity);
            items.draw(model, spin * Mat4::translation(items.info(model).center * -1.0f));
        }
    }
    gfx::setModelMatrix(Mat4::identity());
}

void PropVisuals::drawProjectiles(const game::Match& match, const math3d::Frustum& frustum,
                                  const game::ItemModels& items) const {
    // Orientados segun su velocidad (el modelo mira hacia -Z).
    for (int i = 0; i < game::Match::kMaxProjectiles; ++i) {
        const game::Projectile& p = match.projectiles()[i];
        if (!p.active || !frustum.sphereVisible(p.position, 0.5f)) continue;
        const Vec3& v = p.velocity;
        const float flat = sqrtf(v.x * v.x + v.z * v.z);
        const float yaw = atan2f(-v.x, -v.z), pitch = atan2f(v.y, flat);
        const game::ItemModel model = p.kind == game::ProjectileKind::Rocket            ? game::ItemModel::Rocket
                                      : p.kind == game::ProjectileKind::LauncherGrenade ? game::ItemModel::LauncherGrenade
                                                                                         : game::ItemModel::Grenade;
        // La granada de mano da vueltas en el aire.
        const Mat4 spin = p.kind == game::ProjectileKind::HandGrenade ? Mat4::rotationX(p.age * 12.0f) : Mat4::identity();
        items.draw(model, Mat4::translation(p.position) * Mat4::rotationY(yaw) * Mat4::rotationX(pitch) * spin);
    }
    gfx::setModelMatrix(Mat4::identity());
}

void PropVisuals::drawExplosions(const game::Match& match, const math3d::Frustum& frustum) const {
    // Bola de fuego que crece hasta el radio de la explosion en 0.35 s y se apaga.
    constexpr float kLife = 0.45f;
    gfx::setBlend(true);
    for (int i = 0; i < game::Match::kMaxExplosions; ++i) {
        const game::Explosion& e = match.explosions()[i];
        if (e.age >= kLife || !frustum.sphereVisible(e.position, e.radius)) continue;
        const float t = e.age / kLife;
        const float r = e.radius * (0.35f + 0.65f * fminf(1.0f, e.age / 0.3f));
        const Mesh mesh = t < 0.4f ? kBlast0 : (t < 0.75f ? kBlast1 : kBlast2);
        draw(mesh, Mat4::translation(e.position) * Mat4::scale({r, r * 0.8f, r}), nullptr);
    }
    gfx::setBlend(false);
    gfx::setModelMatrix(Mat4::identity());
}

namespace {

// Matriz de una placa de trampa: X = eje U de la superficie, Y = su normal, Z = U x N,
// escalada a sus semianchos (la malla va de -1 a 1).
Mat4 trapMatrix(const game::TrapPlacement& p, float height) {
    const Vec3 u = p.axisU, n = p.normal, v = math3d::cross(p.axisU, p.normal);
    Mat4 m = Mat4::identity();
    m.m[0] = u.x * p.halfU;
    m.m[1] = u.y * p.halfU;
    m.m[2] = u.z * p.halfU;
    m.m[4] = n.x * height;
    m.m[5] = n.y * height;
    m.m[6] = n.z * height;
    m.m[8] = v.x * p.halfV;
    m.m[9] = v.y * p.halfV;
    m.m[10] = v.z * p.halfV;
    m.m[12] = p.center.x + n.x * 0.01f;
    m.m[13] = p.center.y + n.y * 0.01f;
    m.m[14] = p.center.z + n.z * 0.01f;
    return m;
}

}  // namespace

void PropVisuals::drawTraps(const game::Match& match, const math3d::Frustum& frustum, const Vec3& eye,
                            float maxDistance) const {
    for (int i = 0; i < game::Match::kMaxTraps; ++i) {
        const game::Trap& t = match.traps()[i];
        if (!t.active || math3d::length(t.place.center - eye) > maxDistance) continue;
        if (!frustum.sphereVisible(t.place.center, fmaxf(t.place.halfU, t.place.halfV) + 0.5f)) continue;
        draw(kTrapPlate, trapMatrix(t.place, 1.0f), nullptr);
        // Los pinchos asoman un poco y, al saltar, salen 0.45 m en 0.06 s; vuelven al
        // terminar la espera.
        const float s = t.sinceTrigger;
        const float out = s < 0.06f ? s / 0.06f : (s < 1.2f ? 1.0f : (s < 1.5f ? 1.0f - (s - 1.2f) / 0.3f : 0.0f));
        draw(kTrapSpikes, trapMatrix(t.place, 0.06f + 0.42f * out), nullptr);
    }
    gfx::setModelMatrix(Mat4::identity());
}

void PropVisuals::drawTrapGhost(const game::TrapPlacement& placement) const {
    if (placement.error == game::TrapError::NoSurface) return;  // no se apunta a nada
    gfx::setBlend(true);
    draw(placement.valid ? kTrapGhostValid : kTrapGhostInvalid, trapMatrix(placement, 1.0f), nullptr);
    gfx::setBlend(false);
    gfx::setModelMatrix(Mat4::identity());
}

void PropVisuals::drawBeams(const game::Match& match, const math3d::Frustum& frustum, const Vec3& eye,
                            float time) const {
    gfx::setBlend(true);
    const std::vector<game::Pickup>& pickups = match.pickups();
    for (size_t i = 0; i < pickups.size(); ++i) {
        const game::Pickup& p = pickups[i];
        if (!p.active || p.kind != game::PickupKind::Item) continue;
        if (math3d::length(p.position - eye) > kLootDrawDistance) continue;
        if (!frustum.sphereVisible(p.position + Vec3{0.0f, 0.8f, 0.0f}, 1.0f)) continue;
        draw(static_cast<Mesh>(kBeam0 + static_cast<int>(p.rarity)),
             Mat4::translation(p.position) * Mat4::rotationY(time * 0.5f), nullptr);
    }
    gfx::setBlend(false);
    gfx::setModelMatrix(Mat4::identity());
}

void PropVisuals::drawBus(const Vec3& position, const world::TextureLibrary& textures) const {
    draw(kBus, Mat4::translation(position), &textures);
    gfx::setModelMatrix(Mat4::identity());
}

void PropVisuals::drawGlider(const Vec3& feet, float yaw) const {
    draw(kGlider, Mat4::translation(feet + Vec3{0.0f, 2.75f, 0.0f}) * Mat4::rotationY(yaw), nullptr);
    gfx::setModelMatrix(Mat4::identity());
}

}  // namespace app
