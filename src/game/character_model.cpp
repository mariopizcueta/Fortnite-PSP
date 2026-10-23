#include "game/character_model.h"

#include <math.h>

#include "gfx/graphics.h"
#include "world/terrain.h"

namespace game {

using gfx::rgba;
using math3d::Mat4;
using math3d::Vec3;
using world::Material;

namespace palettes {
const CharacterPalette kPlayer = {
    rgba(226, 170, 128), rgba(226, 196, 96), rgba(84, 96, 60),  rgba(178, 186, 152),
    rgba(72, 54, 36),    rgba(255, 255, 255), rgba(48, 40, 32), rgba(44, 46, 40),
    rgba(112, 122, 62),  rgba(46, 50, 40),   rgba(186, 58, 40), true, false};
const CharacterPalette kCommando = {
    rgba(196, 146, 110), rgba(70, 50, 34),   rgba(40, 42, 44),  rgba(90, 90, 86),
    rgba(30, 30, 30),    rgba(150, 150, 150), rgba(30, 30, 30), rgba(30, 30, 32),
    rgba(70, 72, 74),    rgba(30, 30, 30),   rgba(40, 42, 44),  false, false};
const CharacterPalette kOrange = {
    rgba(212, 160, 120), rgba(30, 28, 26),   rgba(220, 120, 40), rgba(236, 208, 120),
    rgba(60, 50, 40),    rgba(240, 150, 70), rgba(60, 50, 40),  rgba(60, 46, 34),
    rgba(220, 120, 40),  rgba(60, 50, 40),   rgba(240, 220, 200), true, false};
const CharacterPalette kRanger = {
    rgba(170, 120, 90),  rgba(60, 40, 30),   rgba(170, 50, 46),  rgba(200, 190, 170),
    rgba(50, 40, 30),    rgba(110, 130, 190), rgba(40, 40, 60), rgba(40, 40, 50),
    rgba(90, 100, 140),  rgba(40, 40, 50),   rgba(230, 210, 80), false, true};
}  // namespace palettes

namespace {

const Material kEyes{world::kTexNone, rgba(30, 30, 34), 1.0f};
const Material kBuckle{world::kTexNone, rgba(170, 172, 168), 1.0f};
const Material kGun{world::kTexNone, rgba(200, 168, 104), 1.0f};      // tostado del SCAR
const Material kGunDark{world::kTexNone, rgba(54, 52, 48), 1.0f};
const Material kHandle{world::kTexNone, rgba(110, 76, 44), 1.0f};       // mango del pico
const Material kPickHead{world::kTexNone, rgba(120, 150, 200), 1.0f};   // cabeza azulada (Recursos/HUD/Pico.png)
const Material kShadow{world::kTexNone, rgba(0, 0, 0, 110), 1.0f};

constexpr float kHipY = 0.95f;
constexpr float kHipX = 0.12f;
constexpr float kShoulderY = 1.45f;
constexpr float kShoulderX = 0.31f;
constexpr float kStrideLength = 1.6f;  // metros por ciclo completo de pasos

Material flat(u32 c) { return {world::kTexNone, c, 1.0f}; }

void box(world::SceneBuilder& b, const Material& m, Vec3 mn, Vec3 mx) { b.box(m, m, mn, mx, true); }

// Pierna en coordenadas de la cadera (Y = 0 en la articulacion, el pie en -0.95).
void buildLeg(world::SceneBuilder& b, const CharacterPalette& p, bool strap) {
    box(b, {world::kTexCamo, p.pantsTint, 0.6f}, {-0.11f, -0.62f, -0.12f}, {0.11f, 0.03f, 0.12f});
    if (strap) box(b, flat(p.strap), {-0.12f, -0.30f, -0.13f}, {0.12f, -0.25f, 0.13f});
    box(b, flat(p.strap), {-0.12f, -0.44f, -0.13f}, {0.12f, -0.40f, 0.13f});
    box(b, flat(p.boot), {-0.11f, -0.95f, -0.18f}, {0.11f, -0.62f, 0.12f});
    box(b, flat(p.kneePad), {-0.09f, -0.66f, -0.17f}, {0.09f, -0.46f, -0.11f});
}

// Brazo en coordenadas del hombro (la mano queda en -0.58). El derecho lleva el rifle,
// alineado con el brazo: al levantar el brazo 90 grados apunta hacia delante. El pico va
// tambien en la prolongacion del brazo, con la cabeza perpendicular.
void buildArm(world::SceneBuilder& b, const CharacterPalette& p, bool armband, HeldItem held) {
    box(b, flat(p.skin), {-0.07f, -0.46f, -0.07f}, {0.07f, 0.03f, 0.07f});
    if (armband) box(b, flat(p.armband), {-0.08f, -0.14f, -0.08f}, {0.08f, -0.08f, 0.08f});
    box(b, flat(p.glove), {-0.08f, -0.60f, -0.08f}, {0.08f, -0.44f, 0.08f});
    if (held == HeldItem::Pickaxe) {
        box(b, kHandle, {-0.025f, -1.25f, -0.025f}, {0.025f, -0.45f, 0.025f});
        box(b, kPickHead, {-0.035f, -1.30f, -0.34f}, {0.035f, -1.18f, 0.14f});
        box(b, kPickHead, {-0.025f, -1.27f, -0.42f}, {0.025f, -1.21f, -0.34f});  // punta
    }
    if (held == HeldItem::Rifle) {
        box(b, kGun, {-0.045f, -1.10f, -0.06f}, {0.045f, -0.40f, 0.05f});      // cuerpo
        box(b, kGunDark, {-0.025f, -1.32f, -0.03f}, {0.025f, -1.10f, 0.02f});  // canon
        box(b, kGunDark, {-0.035f, -0.78f, 0.05f}, {0.035f, -0.68f, 0.18f});   // cargador
    }
}

void buildBody(world::SceneBuilder& b, const CharacterPalette& p) {
    box(b, flat(p.shirt), {-0.24f, 0.98f, -0.13f}, {0.24f, 1.48f, 0.13f});
    box(b, flat(p.belt), {-0.25f, 0.92f, -0.14f}, {0.25f, 1.03f, 0.14f});
    box(b, kBuckle, {-0.06f, 0.94f, -0.155f}, {0.06f, 1.01f, -0.14f});
    box(b, flat(p.shirt), {0.24f, 0.78f, -0.08f}, {0.32f, 1.00f, 0.08f});  // bolsa en la cadera
    if (p.hasScarf) {
        box(b, flat(p.scarf), {-0.20f, 1.36f, -0.16f}, {0.20f, 1.52f, 0.12f});
        box(b, flat(p.scarf), {-0.10f, 1.22f, -0.15f}, {0.10f, 1.36f, -0.13f});
    }
    box(b, flat(p.skin), {-0.07f, 1.48f, -0.06f}, {0.07f, 1.57f, 0.06f});  // cuello
    box(b, flat(p.skin), {-0.12f, 1.56f, -0.13f}, {0.12f, 1.82f, 0.12f});  // cabeza
    if (p.hasHelmet) {
        box(b, flat(p.kneePad), {-0.14f, 1.76f, -0.15f}, {0.14f, 1.92f, 0.14f});
    } else {
        box(b, flat(p.hair), {-0.13f, 1.78f, -0.15f}, {0.13f, 1.90f, 0.13f});
        box(b, flat(p.hair), {-0.13f, 1.64f, 0.10f}, {0.13f, 1.80f, 0.14f});
    }
    box(b, kEyes, {-0.075f, 1.70f, -0.14f}, {-0.03f, 1.73f, -0.125f});  // los ojos marcan
    box(b, kEyes, {0.03f, 1.70f, -0.14f}, {0.075f, 1.73f, -0.125f});    // el frente
}

void buildShadow(world::SceneBuilder& b) {
    constexpr int kSides = 8;
    for (int i = 0; i < kSides; ++i) {
        const float a0 = 2.0f * math3d::kPi * i / kSides;
        const float a1 = 2.0f * math3d::kPi * (i + 1) / kSides;
        b.triangle(kShadow, {0, 0, 0}, {cosf(a0) * 0.45f, 0, -sinf(a0) * 0.45f},
                   {cosf(a1) * 0.45f, 0, -sinf(a1) * 0.45f});
    }
}

// Suavizado exponencial independiente del frame rate.
float smoothToward(float current, float target, float rate, float dt) {
    return current + (target - current) * (1.0f - expf(-rate * dt));
}

int countTriangles(const std::vector<world::StaticBatch>& batches) {
    int n = 0;
    for (const world::StaticBatch& b : batches) n += b.triangles;
    return n;
}

}  // namespace

void CharacterAnim::update(const CharacterState& state, bool aiming, bool alive, float dt) {
    idleTime += dt;
    recoil = smoothToward(recoil, 0.0f, 14.0f, dt);
    swing = smoothToward(swing, 0.0f, 6.0f, dt);
    flinch = smoothToward(flinch, 0.0f, 8.0f, dt);
    if (!alive) {
        deathSeconds += dt;
        return;
    }
    deathSeconds = 0.0f;
    const float speed = state.horizontalSpeed();
    float legL, legR, armL, armR;
    if (state.grounded()) {
        // La fase avanza con la distancia recorrida: los pies no "patinan".
        walkPhase += speed * dt * (2.0f * math3d::kPi / kStrideLength);
        if (walkPhase > 2.0f * math3d::kPi) walkPhase -= 2.0f * math3d::kPi;
        const float amplitude = 0.75f * math3d::clampf(speed / tuning::kRunSpeed, 0.0f, 1.0f);
        const float s = sinf(walkPhase) * amplitude;
        legL = s;
        legR = -s;
        armL = -s * 0.8f;
        armR = s * 0.8f;
    } else {
        legL = 0.55f;
        legR = -0.25f;
        armL = 0.7f;
        armR = 0.5f;
    }
    if (aiming) {
        // Rifle al frente: brazo derecho horizontal, el izquierdo sujetando.
        armR = 1.52f;
        armL = 1.30f;
    }
    // Retroceso: el rifle sube un poco con cada disparo.
    armR += 0.30f * recoil;
    armL += 0.20f * recoil;
    float rate = 14.0f;
    if (swing > 0.03f) {
        // Golpe de pico: el brazo baja desde encima de la cabeza hasta delante del pecho.
        armR = 0.8f + 1.9f * swing;
        rate = 30.0f;
    }
    leftLeg = smoothToward(leftLeg, legL, 14.0f, dt);
    rightLeg = smoothToward(rightLeg, legR, 14.0f, dt);
    leftArm = smoothToward(leftArm, armL, 14.0f, dt);
    rightArm = smoothToward(rightArm, armR, rate, dt);
}

bool CharacterModel::build(const CharacterPalette& palette) {
    world::SceneBuilder b(1.0f);
    buildBody(b, palette);
    b.flushGroup(m_body.batches);
    buildLeg(b, palette, true);
    b.flushGroup(m_leftLeg.batches);
    buildLeg(b, palette, false);
    b.flushGroup(m_rightLeg.batches);
    buildArm(b, palette, false, HeldItem::Empty);
    b.flushGroup(m_leftArm.batches);
    buildArm(b, palette, true, HeldItem::Rifle);  // brazalete y rifle en el brazo derecho
    b.flushGroup(m_rightArm.batches);
    buildArm(b, palette, true, HeldItem::Pickaxe);
    b.flushGroup(m_rightArmPickaxe.batches);
    buildArm(b, palette, true, HeldItem::Empty);  // con un consumible en la mano
    b.flushGroup(m_rightArmEmpty.batches);
    buildShadow(b);
    b.flushGroup(m_shadow);

    // El modelo mira a -Z: su derecha es +X.
    m_leftLeg.pivot = {-kHipX, kHipY, 0.0f};
    m_rightLeg.pivot = {kHipX, kHipY, 0.0f};
    m_leftArm.pivot = {-kShoulderX, kShoulderY, 0.0f};
    m_rightArm.pivot = {kShoulderX, kShoulderY, 0.0f};
    m_rightArmPickaxe.pivot = m_rightArm.pivot;
    m_rightArmEmpty.pivot = m_rightArm.pivot;

    m_triangles = 0;
    for (const Part* p : {&m_body, &m_leftLeg, &m_rightLeg, &m_leftArm, &m_rightArm}) {
        m_triangles += countTriangles(p->batches);
    }
    return !b.failed();
}

void CharacterModel::destroy() {
    for (Part* p : {&m_body, &m_leftLeg, &m_rightLeg, &m_leftArm, &m_rightArm, &m_rightArmPickaxe, &m_rightArmEmpty}) {
        for (world::StaticBatch& batch : p->batches) gfx::destroyMesh(batch.mesh);
        p->batches.clear();
    }
    for (world::StaticBatch& batch : m_shadow) gfx::destroyMesh(batch.mesh);
    m_shadow.clear();
}

void CharacterModel::drawPart(const Part& part, const Mat4& model,
                              const world::TextureLibrary& textures) const {
    gfx::setModelMatrix(model);
    for (const world::StaticBatch& b : part.batches) {
        gfx::bindTexture(textures.get(b.texture));
        gfx::drawMesh(b.mesh);
    }
}

void CharacterModel::draw(const CharacterState& state, const CharacterAnim& anim,
                          const world::TextureLibrary& textures, bool eliminated, HeldItem held) const {
    const Vec3& pos = state.position;
    if (!eliminated) {
        // Sombra circular bajo los pies: encoge al subir, para leer la altura del salto.
        // Se apoya en la altura real de los pies al tocar suelo (cajas, altillo).
        const float groundY = state.grounded() ? pos.y : world::groundHeightAt(pos.x, pos.z);
        const float shrink = math3d::clampf(1.0f - (pos.y - groundY) / 6.0f, 0.4f, 1.0f);
        gfx::setBlend(true);
        gfx::setModelMatrix(Mat4::translation({pos.x, groundY + 0.04f, pos.z}) *
                            Mat4::scale({shrink, 1.0f, shrink}));
        for (const world::StaticBatch& b : m_shadow) {
            gfx::bindTexture(nullptr);
            gfx::drawMesh(b.mesh);
        }
        gfx::setBlend(false);
    }

    Mat4 root = Mat4::translation(pos) * Mat4::rotationY(state.yaw);
    if (eliminated) {
        // Cae de espaldas en 0.45 s (acelerando, como con gravedad) y queda tumbado:
        // rotationX(+90) lleva la cabeza (+Y) hacia +Z (detras).
        const float t = anim.deathSeconds > 0.0f ? math3d::clampf(anim.deathSeconds / 0.45f, 0.0f, 1.0f) : 1.0f;
        const float fall = t * t;
        root = root * Mat4::translation({0.0f, 0.15f * fall, 0.0f}) *
               Mat4::rotationX(math3d::kPi * 0.5f * fall);
    } else {
        // Respiracion (muy leve) y sacudida hacia atras al recibir un impacto.
        const float breath = 0.012f * sinf(anim.idleTime * 2.4f) * (state.horizontalSpeed() < 0.5f ? 1.0f : 0.0f);
        root = root * Mat4::translation({0.0f, breath, 0.0f}) * Mat4::rotationX(-0.22f * anim.flinch);
    }
    drawPart(m_body, root, textures);
    // rotationX(+a) lleva el extremo de la pieza hacia -Z (hacia delante).
    auto joint = [&root](const Part& p, float angle) {
        return root * Mat4::translation(p.pivot) * Mat4::rotationX(angle);
    };
    const bool still = eliminated && (anim.deathSeconds <= 0.0f || anim.deathSeconds > 0.45f);
    drawPart(m_leftLeg, joint(m_leftLeg, still ? 0.0f : anim.leftLeg), textures);
    drawPart(m_rightLeg, joint(m_rightLeg, still ? 0.0f : anim.rightLeg), textures);
    drawPart(m_leftArm, joint(m_leftArm, still ? 0.2f : anim.leftArm), textures);
    const Part& rightArm = held == HeldItem::Pickaxe ? m_rightArmPickaxe
                           : held == HeldItem::Empty ? m_rightArmEmpty
                                                     : m_rightArm;
    drawPart(rightArm, joint(rightArm, still ? -0.2f : anim.rightArm), textures);
    gfx::setModelMatrix(Mat4::identity());
}

}  // namespace game
