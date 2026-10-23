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

// Armas tumbadas a lo largo de X (como en el suelo de Fortnite).
void buildWeapon(SceneBuilder& b, int type) {
    const Material tan = flat(rgba(204, 172, 108));
    const Material dark = flat(rgba(44, 44, 42));
    const Material wood = flat(rgba(120, 70, 36));
    switch (type) {
        case 0:  // fusil de asalto / SCAR
            b.box(tan, tan, {-0.30f, 0.05f, -0.04f}, {0.22f, 0.15f, 0.04f});
            b.box(dark, dark, {0.22f, 0.08f, -0.02f}, {0.45f, 0.12f, 0.02f});
            b.box(dark, dark, {-0.04f, -0.05f, -0.03f}, {0.04f, 0.05f, 0.03f});
            b.box(dark, dark, {-0.10f, 0.15f, -0.025f}, {0.08f, 0.19f, 0.025f});
            break;
        case 1:  // escopeta de corredera
            b.box(wood, wood, {-0.40f, 0.04f, -0.035f}, {-0.05f, 0.13f, 0.035f});
            b.box(dark, dark, {-0.05f, 0.07f, -0.03f}, {0.42f, 0.13f, 0.03f});
            b.box(wood, wood, {0.12f, 0.03f, -0.04f}, {0.28f, 0.08f, 0.04f});
            break;
        case 2:  // subfusil con silenciador
            b.box(dark, dark, {-0.22f, 0.05f, -0.035f}, {0.12f, 0.15f, 0.035f});
            b.box(flat(rgba(70, 70, 68)), dark, {0.12f, 0.08f, -0.03f}, {0.36f, 0.13f, 0.03f});
            b.box(dark, dark, {-0.02f, -0.06f, -0.025f}, {0.04f, 0.05f, 0.025f});
            break;
        default:  // francotirador de cerrojo
            b.box(tan, tan, {-0.48f, 0.04f, -0.035f}, {0.18f, 0.13f, 0.035f});
            b.box(dark, dark, {0.18f, 0.08f, -0.018f}, {0.62f, 0.11f, 0.018f});
            b.box(dark, dark, {-0.14f, 0.15f, -0.03f}, {0.12f, 0.21f, 0.03f});
            break;
    }
}

void buildConsumable(SceneBuilder& b, int type) {
    switch (type) {
        case 0: {  // vendas: dos rollos blancos con banda roja
            const Material white = flat(rgba(236, 232, 222));
            b.cylinder(white, white, {-0.08f, 0.0f, 0.0f}, 0.07f, 0.16f, 8, true);
            b.cylinder(white, white, {0.09f, 0.0f, 0.02f}, 0.07f, 0.14f, 8, true);
            b.cylinder(flat(rgba(196, 50, 40)), white, {-0.08f, 0.06f, 0.0f}, 0.072f, 0.04f, 8, false);
            break;
        }
        case 1: {  // botiquin blanco con cruz roja
            const Material white = flat(rgba(240, 240, 240));
            const Material red = flat(rgba(214, 40, 40));
            b.box(white, white, {-0.18f, 0.0f, -0.12f}, {0.18f, 0.24f, 0.12f});
            b.box(red, red, {-0.04f, 0.04f, -0.13f}, {0.04f, 0.20f, -0.12f});
            b.box(red, red, {-0.10f, 0.08f, -0.13f}, {0.10f, 0.16f, -0.12f});
            break;
        }
        case 2:  // minipocion: frasco pequeno azul
            b.cylinder(flat(rgba(70, 150, 235)), flat(rgba(120, 190, 250)), {0.0f, 0.0f, 0.0f}, 0.07f, 0.13f, 8, true);
            b.cylinder(flat(rgba(214, 214, 214)), flat(rgba(214, 214, 214)), {0.0f, 0.13f, 0.0f}, 0.03f, 0.05f, 6, true);
            break;
        default:  // pocion de escudo: tarro azul con tapa
            b.cylinder(flat(rgba(60, 130, 230)), flat(rgba(110, 180, 250)), {0.0f, 0.0f, 0.0f}, 0.12f, 0.22f, 8, true);
            b.cylinder(flat(rgba(150, 150, 150)), flat(rgba(150, 150, 150)), {0.0f, 0.22f, 0.0f}, 0.07f, 0.05f, 8, true);
            break;
    }
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
    for (int i = 0; i < 4; ++i) {
        buildWeapon(b, i);
        finish(static_cast<Mesh>(kRifle + i));
        buildConsumable(b, i);
        finish(static_cast<Mesh>(kBandage + i));
    }
    const u32 ammoColors[4] = {rgba(120, 140, 170), rgba(70, 130, 60), rgba(150, 40, 36), rgba(214, 90, 40)};
    for (int i = 0; i < 4; ++i) {
        buildAmmo(b, ammoColors[i]);
        finish(static_cast<Mesh>(kAmmoLight + i));
    }
    for (int i = 0; i < 5; ++i) {
        buildBeam(b, kRarityBeam[i]);
        finish(static_cast<Mesh>(kBeam0 + i));
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

PropVisuals::Mesh PropVisuals::pickupMesh(const game::Pickup& p) {
    if (p.kind == game::PickupKind::Ammo) return static_cast<Mesh>(kAmmoLight + static_cast<int>(p.ammo));
    switch (p.item) {
        case game::ItemType::AssaultRifle: return kRifle;
        case game::ItemType::Shotgun: return kShotgun;
        case game::ItemType::Smg: return kSmg;
        case game::ItemType::Sniper: return kSniper;
        case game::ItemType::Bandage: return kBandage;
        case game::ItemType::Medkit: return kMedkit;
        case game::ItemType::SmallShield: return kSmallShield;
        default: return kShieldPotion;
    }
}

void PropVisuals::drawLoot(const game::Match& match, const math3d::Frustum& frustum, const Vec3& eye,
                           float maxDistance, float time, const world::TextureLibrary& textures) const {
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
        draw(pickupMesh(p), Mat4::translation(at) * Mat4::rotationY(time * 1.3f + phase), &textures);
    }
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
