#include "game/item_models.h"

#include <math.h>

#include <vector>

#include "gfx/graphics.h"

namespace game {

using gfx::rgba;
using math3d::Mat4;
using math3d::Vec3;

namespace {

// --- Colores (muestreados de Recursos/Weapons y Referencias/Armas) -----------------------------
const u32 kBlack = rgba(34, 34, 36);
const u32 kDark = rgba(58, 58, 62);
const u32 kGunmetal = rgba(84, 88, 94);
const u32 kGrey = rgba(118, 122, 128);
const u32 kSilver = rgba(176, 180, 186);
const u32 kScarTan = rgba(204, 170, 104);
const u32 kScarDark = rgba(72, 66, 54);
const u32 kWood = rgba(132, 84, 44);
const u32 kWoodDark = rgba(96, 60, 32);
const u32 kBurstOrange = rgba(198, 124, 58);
const u32 kTacticalRed = rgba(168, 42, 38);
const u32 kBrass = rgba(188, 150, 74);
const u32 kBrassDark = rgba(138, 104, 48);
const u32 kOlive = rgba(84, 102, 62);
const u32 kSharkRed = rgba(200, 56, 48);
const u32 kWhite = rgba(236, 234, 228);
const u32 kLensBlue = rgba(70, 150, 230);
const u32 kLensRed = rgba(220, 50, 40);
const u32 kGlowBlue = rgba(90, 180, 255);
const u32 kHandle = rgba(122, 80, 46);
const u32 kPickHead = rgba(120, 150, 200);

// Luz horneada como la de los personajes (principal arriba-delante-derecha y relleno detras).
u32 lit(u32 color, const Vec3& n) {
    static const Vec3 kKey = math3d::normalize({0.35f, 0.85f, -0.40f});
    static const Vec3 kFill = math3d::normalize({-0.45f, 0.35f, 0.82f});
    float b = 0.52f + 0.45f * fmaxf(0.0f, math3d::dot(n, kKey)) + 0.22f * fmaxf(0.0f, math3d::dot(n, kFill));
    if (n.y < 0.0f) b += 0.12f * n.y;
    auto ch = [&](int shift) {
        const float v = static_cast<float>((color >> shift) & 0xFF) * b;
        return static_cast<u32>(v > 255.0f ? 255.0f : v);
    };
    return rgba(ch(0), ch(8), ch(16), color >> 24);
}

// Constructor de mallas con primitivas de caras planas (cajas, prismas, conos).
class Builder {
public:
    explicit Builder(bool details) : m_details(details) {}
    bool details() const { return m_details; }

    void setTransform(const Mat4& t) {
        m_transform = t;
        m_hasTransform = true;
    }
    void resetTransform() { m_hasTransform = false; }

    // Caja por centro y tamano.
    void box(float cx, float cy, float cz, float sx, float sy, float sz, u32 color) {
        const Vec3 mn{cx - sx * 0.5f, cy - sy * 0.5f, cz - sz * 0.5f}, mx{cx + sx * 0.5f, cy + sy * 0.5f, cz + sz * 0.5f};
        const Vec3 c[8] = {{mn.x, mn.y, mn.z}, {mx.x, mn.y, mn.z}, {mx.x, mx.y, mn.z}, {mn.x, mx.y, mn.z},
                           {mn.x, mn.y, mx.z}, {mx.x, mn.y, mx.z}, {mx.x, mx.y, mx.z}, {mn.x, mx.y, mx.z}};
        const int f[6][4] = {{0, 3, 2, 1}, {4, 5, 6, 7}, {1, 2, 6, 5}, {0, 4, 7, 3}, {3, 7, 6, 2}, {0, 1, 5, 4}};
        const Vec3 normals[6] = {{0, 0, -1}, {0, 0, 1}, {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}};
        for (int i = 0; i < 6; ++i) quad(c[f[i][0]], c[f[i][1]], c[f[i][2]], c[f[i][3]], normals[i], color);
    }
    // Caja solo si hay detalle (piezas pequenas: miras, gatillos...).
    void detail(float cx, float cy, float cz, float sx, float sy, float sz, u32 color) {
        if (m_details) box(cx, cy, cz, sx, sy, sz, color);
    }
    // Prisma de `sides` lados a lo largo de Z (canones, tubos, mirillas), de z0 a z1.
    void tubeZ(float cx, float cy, float z0, float z1, float r, int sides, u32 color, bool caps = true) {
        if (!m_details && sides > 6) sides = 6;
        if (!m_details && sides > 4 && r < 0.02f) sides = 4;
        for (int i = 0; i < sides; ++i) {
            const float a0 = 2.0f * math3d::kPi * i / sides + math3d::kPi / sides;
            const float a1 = 2.0f * math3d::kPi * (i + 1) / sides + math3d::kPi / sides;
            const Vec3 p0{cx + cosf(a0) * r, cy + sinf(a0) * r, z0}, p1{cx + cosf(a1) * r, cy + sinf(a1) * r, z0};
            const Vec3 p2{p1.x, p1.y, z1}, p3{p0.x, p0.y, z1};
            const float am = 0.5f * (a0 + a1);
            quad(p0, p3, p2, p1, {cosf(am), sinf(am), 0.0f}, color);
        }
        if (!caps) return;
        for (int end = 0; end < 2; ++end) {
            const float z = end == 0 ? z0 : z1;
            const Vec3 n{0.0f, 0.0f, end == 0 ? -1.0f : 1.0f};
            const Vec3 c{cx, cy, z};
            for (int i = 0; i < sides; ++i) {
                const float a0 = 2.0f * math3d::kPi * i / sides + math3d::kPi / sides;
                const float a1 = 2.0f * math3d::kPi * (i + 1) / sides + math3d::kPi / sides;
                const Vec3 p0{cx + cosf(a0) * r, cy + sinf(a0) * r, z}, p1{cx + cosf(a1) * r, cy + sinf(a1) * r, z};
                if (end == 0) tri(c, p1, p0, n, color);
                else tri(c, p0, p1, n, color);
            }
        }
    }
    // Prisma vertical (frascos, botes): base en (cx, y0, cz).
    void tubeY(float cx, float y0, float cz, float y1, float r, int sides, u32 color) {
        // rotationX(-90) lleva +Z local a +Y: el prisma sube de y0 a y1.
        setLocal(Mat4::translation({cx, y0, cz}) * Mat4::rotationX(-0.5f * math3d::kPi));
        tubeZ(0.0f, 0.0f, 0.0f, y1 - y0, r, sides, color);
        endLocal();
    }
    // Pinchos (piramide de 4 caras) de la base a la punta.
    void spike(const Vec3& base, float r, const Vec3& tip, u32 color) {
        const Vec3 p[4] = {base + Vec3{-r, 0, -r}, base + Vec3{r, 0, -r}, base + Vec3{r, 0, r}, base + Vec3{-r, 0, r}};
        for (int i = 0; i < 4; ++i) {
            const Vec3& a = p[i];
            const Vec3& b = p[(i + 1) % 4];
            tri(a, tip, b, math3d::normalize(math3d::cross(tip - a, b - a)), color);
        }
    }

    // Transformacion local temporal (piezas inclinadas), combinada con la actual. Se
    // pueden anidar: endLocal() vuelve a la anterior.
    void setLocal(const Mat4& t) {
        m_stack.push_back(m_transform);
        m_stackHas.push_back(m_hasTransform);
        m_transform = m_hasTransform ? m_transform * t : t;
        m_hasTransform = true;
    }
    void endLocal() {
        if (m_stack.empty()) return;
        m_transform = m_stack.back();
        m_hasTransform = m_stackHas.back() != 0;
        m_stack.pop_back();
        m_stackHas.pop_back();
    }

    bool finish(gfx::Mesh& out, int* triangles, Vec3* center) {
        if (center && !m_vertices.empty()) {
            Vec3 mn{1e9f, 1e9f, 1e9f}, mx{-1e9f, -1e9f, -1e9f};
            for (const gfx::Vertex& v : m_vertices) {
                mn = {fminf(mn.x, v.x), fminf(mn.y, v.y), fminf(mn.z, v.z)};
                mx = {fmaxf(mx.x, v.x), fmaxf(mx.y, v.y), fmaxf(mx.z, v.z)};
            }
            *center = (mn + mx) * 0.5f;
        }
        *triangles = static_cast<int>(m_indices.size() / 3);
        const bool ok = !m_vertices.empty() && gfx::createMesh(out, m_vertices.data(), static_cast<int>(m_vertices.size()),
                                                               m_indices.data(), static_cast<int>(m_indices.size()));
        m_vertices.clear();
        m_indices.clear();
        m_hasTransform = false;
        m_stack.clear();
        m_stackHas.clear();
        return ok;
    }

private:
    void emit(const Vec3& p, const Vec3& n, u32 color) {
        Vec3 wp = p, wn = n;
        if (m_hasTransform) {
            wp = m_transform.transformPoint(p);
            wn = math3d::normalize(m_transform.transformPoint(n) - m_transform.transformPoint({0, 0, 0}));
        }
        m_vertices.push_back({0.0f, 0.0f, lit(color, wn), wp.x, wp.y, wp.z});
    }
    void quad(const Vec3& a, const Vec3& b, const Vec3& c, const Vec3& d, const Vec3& n, u32 color) {
        const u16 base = static_cast<u16>(m_vertices.size());
        emit(a, n, color);
        emit(b, n, color);
        emit(c, n, color);
        emit(d, n, color);
        const u16 idx[6] = {0, 1, 2, 0, 2, 3};
        for (u16 i : idx) m_indices.push_back(base + i);
    }
    void tri(const Vec3& a, const Vec3& b, const Vec3& c, const Vec3& n, u32 color) {
        const u16 base = static_cast<u16>(m_vertices.size());
        emit(a, n, color);
        emit(b, n, color);
        emit(c, n, color);
        for (u16 i = 0; i < 3; ++i) m_indices.push_back(base + i);
    }

    bool m_details;
    Mat4 m_transform = Mat4::identity();
    bool m_hasTransform = false;
    std::vector<Mat4> m_stack;
    std::vector<u8> m_stackHas;
    std::vector<gfx::Vertex> m_vertices;
    std::vector<u16> m_indices;
};

// Empunadura inclinada hacia atras (como en casi todas las armas), bajo el origen.
void pistolGrip(Builder& b, u32 color, float length = 0.10f) {
    b.setLocal(Mat4::translation({0.0f, 0.0f, 0.01f}) * Mat4::rotationX(-0.28f));
    b.box(0.0f, -length * 0.5f + 0.01f, 0.0f, 0.032f, length, 0.045f, color);
    b.endLocal();
    b.detail(0.0f, -0.012f, -0.035f, 0.008f, 0.03f, 0.035f, kBlack);  // guardamonte
}

// Cargador curvo (dos tramos) o recto.
void magazine(Builder& b, float z, float length, u32 color, bool curved) {
    b.setLocal(Mat4::translation({0.0f, 0.0f, z}) * Mat4::rotationX(curved ? 0.18f : 0.08f));
    b.box(0.0f, -length * 0.35f, 0.0f, 0.034f, length * 0.7f, 0.055f, color);
    if (curved) {
        b.setLocal(Mat4::translation({0.0f, -length * 0.7f, 0.0f}) * Mat4::rotationX(0.3f));
        b.box(0.0f, -length * 0.15f, 0.0f, 0.034f, length * 0.3f, 0.05f, color);
        b.endLocal();
    } else {
        b.box(0.0f, -length * 0.85f, 0.0f, 0.034f, length * 0.3f, 0.055f, color);
    }
    b.endLocal();
}

void buildPickaxe(Builder& b) {
    // Pico por defecto (Recursos/HUD/Pico.png): mango de madera y cabeza azulada curva. El
    // mango sale de la mano hacia delante y la cabeza queda arriba (golpea al bajar el brazo).
    b.tubeZ(0.05f, 0.0f, -0.56f, 0.16f, 0.022f, 6, kHandle);
    b.detail(0.05f, 0.0f, 0.12f, 0.05f, 0.05f, 0.06f, kWoodDark);  // pomo
    b.box(0.05f, 0.05f, -0.55f, 0.066f, 0.20f, 0.10f, kPickHead);   // ojo de la cabeza
    b.setLocal(Mat4::translation({0.05f, 0.14f, -0.55f}) * Mat4::rotationX(0.25f));
    b.box(0.0f, 0.12f, 0.0f, 0.05f, 0.26f, 0.07f, kPickHead);  // pico superior
    b.endLocal();
    b.setLocal(Mat4::translation({0.05f, -0.04f, -0.55f}) * Mat4::rotationX(-0.35f));
    b.box(0.0f, -0.07f, 0.0f, 0.052f, 0.14f, 0.07f, kPickHead);  // contrapunta
    b.endLocal();
    b.detail(0.05f, 0.05f, -0.55f, 0.07f, 0.05f, 0.11f, rgba(170, 70, 60));  // remache rojo
}

void buildAssaultRifle(Builder& b) {
    // Fusil de asalto generico (Recursos/Weapons/Assault_rifle_icon.png): negro, asa de
    // transporte, guardamanos redondo y cargador curvo.
    b.box(0.0f, 0.055f, -0.04f, 0.05f, 0.08f, 0.30f, kDark);           // cajon
    b.box(0.0f, 0.012f, -0.10f, 0.046f, 0.035f, 0.12f, kBlack);        // brocal
    b.tubeZ(0.0f, 0.058f, -0.46f, -0.19f, 0.031f, 8, kDark);           // guardamanos
    b.tubeZ(0.0f, 0.06f, -0.66f, -0.46f, 0.010f, 5, kBlack);           // canon
    b.detail(0.0f, 0.10f, -0.43f, 0.012f, 0.07f, 0.018f, kBlack);      // punto de mira
    b.box(0.0f, 0.122f, -0.03f, 0.022f, 0.024f, 0.16f, kDark);         // asa de transporte
    b.detail(0.0f, 0.10f, -0.10f, 0.02f, 0.03f, 0.02f, kDark);
    b.detail(0.0f, 0.10f, 0.04f, 0.02f, 0.03f, 0.02f, kDark);
    magazine(b, -0.10f, 0.16f, kBlack, true);
    pistolGrip(b, kBlack);
    b.box(0.0f, 0.052f, 0.24f, 0.044f, 0.07f, 0.26f, kBlack);          // culata
    b.box(0.0f, 0.032f, 0.355f, 0.048f, 0.11f, 0.04f, kDark);          // cantonera
}

void buildScar(Builder& b) {
    // SCAR (Recursos/Weapons/AssaultRiffleLegendary.png y Referencias/Armas): tostada, con
    // rail y mira holografica.
    b.box(0.0f, 0.062f, -0.06f, 0.056f, 0.09f, 0.38f, kScarTan);       // cajon
    b.box(0.0f, 0.112f, -0.06f, 0.03f, 0.012f, 0.36f, kScarDark);      // rail
    b.box(0.0f, 0.052f, -0.36f, 0.05f, 0.07f, 0.22f, kScarTan);        // guardamanos
    b.tubeZ(0.0f, 0.065f, -0.66f, -0.47f, 0.012f, 5, kScarDark);       // canon
    b.tubeZ(0.0f, 0.065f, -0.71f, -0.66f, 0.019f, 6, kBlack);          // bocacha
    b.box(0.0f, 0.142f, -0.04f, 0.032f, 0.048f, 0.06f, kScarDark);     // mira holografica
    b.detail(0.0f, 0.150f, -0.072f, 0.02f, 0.026f, 0.004f, kLensRed);
    magazine(b, -0.13f, 0.15f, kScarDark, false);
    pistolGrip(b, kScarDark);
    b.box(0.0f, 0.07f, 0.21f, 0.04f, 0.055f, 0.18f, kScarTan);         // culata plegable
    b.box(0.0f, 0.04f, 0.31f, 0.046f, 0.13f, 0.04f, kScarDark);
}

void buildBurstRifle(Builder& b) {
    // Fusil de rafagas (Burst_Assault_Rifle): metal oscuro, guardamanos y culata naranjas.
    b.box(0.0f, 0.058f, -0.06f, 0.052f, 0.085f, 0.32f, kGunmetal);
    b.box(0.0f, 0.05f, -0.33f, 0.056f, 0.075f, 0.22f, kBurstOrange);
    b.tubeZ(0.0f, 0.062f, -0.68f, -0.44f, 0.012f, 5, kBlack);
    b.tubeZ(0.0f, 0.062f, -0.74f, -0.68f, 0.022f, 6, kDark);           // freno de boca
    b.box(0.0f, 0.115f, -0.08f, 0.024f, 0.03f, 0.10f, kBlack);         // mira
    magazine(b, -0.12f, 0.16f, kDark, true);
    pistolGrip(b, kBurstOrange);
    b.box(0.0f, 0.085f, 0.20f, 0.03f, 0.03f, 0.20f, kBurstOrange);     // culata de esqueleto
    b.box(0.0f, 0.0f, 0.22f, 0.03f, 0.03f, 0.18f, kBurstOrange);
    b.box(0.0f, 0.042f, 0.31f, 0.04f, 0.13f, 0.035f, kBurstOrange);
}

void buildScopedRifle(Builder& b) {
    // Fusil con mira (Scoped_Assault_Rifle): gris oscuro con mira telescopica.
    b.box(0.0f, 0.058f, -0.07f, 0.052f, 0.085f, 0.34f, kGunmetal);
    b.box(0.0f, 0.052f, -0.36f, 0.05f, 0.07f, 0.24f, kDark);
    b.tubeZ(0.0f, 0.06f, -0.72f, -0.48f, 0.011f, 5, kBlack);
    b.tubeZ(0.0f, 0.148f, -0.21f, 0.03f, 0.026f, 8, kBlack);           // mira telescopica
    b.tubeZ(0.0f, 0.148f, -0.25f, -0.21f, 0.034f, 8, kBlack);
    b.detail(0.0f, 0.148f, -0.252f, 0.03f, 0.03f, 0.004f, kLensRed);
    b.detail(0.0f, 0.112f, -0.15f, 0.018f, 0.03f, 0.02f, kDark);       // monturas
    b.detail(0.0f, 0.112f, -0.02f, 0.018f, 0.03f, 0.02f, kDark);
    magazine(b, -0.12f, 0.14f, kBlack, false);
    pistolGrip(b, kBlack);
    b.box(0.0f, 0.055f, 0.23f, 0.046f, 0.08f, 0.24f, kGunmetal);
    b.box(0.0f, 0.035f, 0.34f, 0.05f, 0.12f, 0.04f, kBlack);
}

void buildSuppressedSmg(Builder& b) {
    // Subfusil con silenciador (Referencias/Armas/fortnite-suppressed-smg).
    b.box(0.0f, 0.05f, -0.08f, 0.05f, 0.09f, 0.26f, kGrey);
    b.tubeZ(0.0f, 0.062f, -0.56f, -0.21f, 0.022f, 6, kBlack);          // silenciador
    magazine(b, -0.06f, 0.17f, kBlack, false);
    pistolGrip(b, kBlack, 0.09f);
    b.box(0.0f, 0.06f, 0.12f, 0.02f, 0.04f, 0.14f, kBlack);            // culata plegada
    b.detail(0.0f, 0.105f, -0.10f, 0.022f, 0.022f, 0.10f, kBlack);     // mira
}

void buildSmg(Builder& b) {
    // Subfusil (Submachine_Gun v1.6): gris oscuro, camisa perforada y cargador largo.
    b.box(0.0f, 0.05f, -0.05f, 0.056f, 0.10f, 0.24f, kGunmetal);
    b.tubeZ(0.0f, 0.065f, -0.42f, -0.17f, 0.03f, 6, kDark);            // camisa
    b.tubeZ(0.0f, 0.065f, -0.46f, -0.42f, 0.012f, 5, kBlack);
    if (b.details()) {
        for (int i = 0; i < 4; ++i) b.box(0.0f, 0.065f, -0.22f - i * 0.055f, 0.064f, 0.016f, 0.022f, kBlack);  // agujeros
    }
    magazine(b, -0.11f, 0.22f, kBlack, false);
    pistolGrip(b, kBlack, 0.09f);
    b.box(0.0f, 0.04f, 0.09f, 0.04f, 0.07f, 0.04f, kDark);             // tapa trasera
    b.detail(0.0f, 0.11f, -0.04f, 0.016f, 0.024f, 0.06f, kBlack);
}

void buildPumpShotgun(Builder& b) {
    // Escopeta de corredera (Referencias/Armas/pump-shotgun): metal, corredera y culata de madera.
    b.box(0.0f, 0.05f, -0.04f, 0.05f, 0.08f, 0.28f, kGunmetal);
    b.tubeZ(0.0f, 0.072f, -0.76f, -0.18f, 0.019f, 6, kDark);           // canon
    b.tubeZ(0.0f, 0.035f, -0.66f, -0.18f, 0.016f, 6, kBlack);          // deposito
    b.tubeZ(0.0f, 0.035f, -0.54f, -0.32f, 0.032f, 6, kWood);           // corredera
    b.detail(0.0f, 0.095f, -0.74f, 0.008f, 0.012f, 0.01f, kSilver);    // punto de mira
    pistolGrip(b, kWood);
    b.setLocal(Mat4::translation({0.0f, 0.04f, 0.10f}) * Mat4::rotationX(0.12f));
    b.box(0.0f, -0.01f, 0.15f, 0.046f, 0.085f, 0.30f, kWood);          // culata
    b.endLocal();
    b.box(0.0f, -0.005f, 0.38f, 0.048f, 0.12f, 0.03f, kWoodDark);
}

void buildTacticalShotgun(Builder& b) {
    // Escopeta tactica (Tactical_Shotgun): cuerpo rojo, canon corto y culata negra.
    b.box(0.0f, 0.056f, -0.07f, 0.06f, 0.10f, 0.34f, kTacticalRed);
    b.box(0.0f, 0.06f, -0.36f, 0.058f, 0.08f, 0.18f, kTacticalRed);
    b.tubeZ(0.0f, 0.07f, -0.58f, -0.27f, 0.021f, 6, kBlack);
    b.box(0.0f, 0.115f, -0.12f, 0.03f, 0.014f, 0.30f, kBlack);         // rail
    if (b.details()) {
        for (int i = 0; i < 3; ++i) b.box(0.034f, 0.045f, -0.04f - i * 0.05f, 0.012f, 0.03f, 0.03f, kBrass);  // cartuchos
    }
    pistolGrip(b, kBlack);
    b.box(0.0f, 0.07f, 0.19f, 0.03f, 0.04f, 0.18f, kBlack);
    b.box(0.0f, 0.03f, 0.28f, 0.04f, 0.12f, 0.035f, kBlack);
}

void buildPistol(Builder& b) {
    // Pistola (Recursos/Weapons/pistola.png): corredera plateada y cachas de madera.
    b.box(0.0f, 0.068f, -0.075f, 0.034f, 0.042f, 0.20f, kSilver);
    b.box(0.0f, 0.036f, -0.065f, 0.03f, 0.028f, 0.17f, kGrey);
    b.tubeZ(0.0f, 0.068f, -0.185f, -0.17f, 0.011f, 5, kDark);
    pistolGrip(b, kWood, 0.11f);
    b.detail(0.0f, 0.094f, -0.165f, 0.006f, 0.01f, 0.01f, kDark);
}

void buildRevolver(Builder& b) {
    // Revolver (Recursos/Weapons/revolver.jpeg): laton, tambor y empunadura de madera.
    b.box(0.0f, 0.058f, -0.05f, 0.032f, 0.06f, 0.11f, kBrass);         // armazon
    b.tubeZ(0.0f, 0.052f, -0.10f, -0.02f, 0.032f, 7, kBrassDark);      // tambor
    b.tubeZ(0.0f, 0.072f, -0.33f, -0.10f, 0.015f, 6, kBrass);          // canon
    b.detail(0.0f, 0.09f, -0.32f, 0.006f, 0.014f, 0.012f, kBrassDark);
    b.detail(0.0f, 0.095f, 0.02f, 0.012f, 0.025f, 0.02f, kBrassDark);  // martillo
    pistolGrip(b, kWood, 0.11f);
}

void buildBoltSniper(Builder& b) {
    // Francotirador de cerrojo (Referencias/Armas/Icon_Weapons_Sniper_01): madera y mira grande.
    b.box(0.0f, 0.03f, -0.06f, 0.05f, 0.07f, 0.70f, kWood);            // caja de madera
    b.box(0.0f, 0.06f, -0.05f, 0.044f, 0.05f, 0.26f, kGunmetal);       // cajon
    b.tubeZ(0.0f, 0.07f, -1.02f, -0.40f, 0.012f, 6, kDark);            // canon
    b.tubeZ(0.0f, 0.15f, -0.32f, -0.02f, 0.027f, 8, kBlack);           // mira
    b.tubeZ(0.0f, 0.15f, -0.38f, -0.32f, 0.038f, 8, kBlack);           // objetivo
    b.tubeZ(0.0f, 0.15f, -0.02f, 0.02f, 0.032f, 8, kBlack);            // ocular
    b.detail(0.0f, 0.15f, -0.382f, 0.05f, 0.05f, 0.004f, kLensBlue);
    b.detail(0.0f, 0.105f, -0.24f, 0.02f, 0.04f, 0.02f, kDark);
    b.detail(0.0f, 0.105f, -0.06f, 0.02f, 0.04f, 0.02f, kDark);
    b.detail(0.04f, 0.06f, 0.03f, 0.05f, 0.012f, 0.012f, kSilver);     // palanca del cerrojo
    pistolGrip(b, kWood);
    b.box(0.0f, 0.0f, 0.32f, 0.05f, 0.13f, 0.05f, kWoodDark);          // cantonera
}

void buildSemiAutoSniper(Builder& b) {
    // Francotirador semiautomatico (Icon_Weapons_SK_Sniper_02): negro con camisa perforada,
    // mira de lente azul, bipode y culata marron.
    b.box(0.0f, 0.058f, -0.10f, 0.056f, 0.09f, 0.40f, kDark);
    b.tubeZ(0.0f, 0.065f, -0.86f, -0.30f, 0.022f, 6, kGunmetal);       // camisa
    b.tubeZ(0.0f, 0.065f, -0.93f, -0.86f, 0.012f, 5, kBlack);
    if (b.details()) {
        for (int i = 0; i < 5; ++i) b.box(0.0f, 0.065f, -0.38f - i * 0.09f, 0.05f, 0.012f, 0.03f, kBlack);
    }
    b.tubeZ(0.0f, 0.152f, -0.28f, 0.02f, 0.027f, 8, kBlack);
    b.tubeZ(0.0f, 0.152f, -0.33f, -0.28f, 0.036f, 8, kBlack);
    b.detail(0.0f, 0.152f, -0.332f, 0.045f, 0.045f, 0.004f, kLensBlue);
    magazine(b, -0.14f, 0.11f, kBlack, false);
    pistolGrip(b, kBlack);
    b.setLocal(Mat4::translation({0.0f, 0.04f, -0.55f}) * Mat4::rotationX(0.5f));
    b.box(0.022f, -0.09f, 0.0f, 0.01f, 0.18f, 0.012f, kBlack);         // bipode
    b.box(-0.022f, -0.09f, 0.0f, 0.01f, 0.18f, 0.012f, kBlack);
    b.endLocal();
    b.box(0.0f, 0.04f, 0.24f, 0.05f, 0.10f, 0.26f, rgba(118, 82, 50));
    b.box(0.0f, 0.03f, 0.37f, 0.052f, 0.13f, 0.03f, kBlack);
}

void buildRocketLauncher(Builder& b) {
    // Lanzacohetes (Recursos/Weapons/Bazooka...): tubo verde con boca de tiburon, sobre el
    // hombro. La empunadura esta bajo el tubo.
    b.tubeZ(0.0f, 0.13f, -0.68f, 0.42f, 0.072f, 8, kOlive);
    b.tubeZ(0.0f, 0.13f, 0.42f, 0.52f, 0.088f, 8, kDark);              // tobera
    b.tubeZ(0.0f, 0.13f, -0.74f, -0.68f, 0.08f, 8, kSharkRed);         // boca
    if (b.details()) {
        for (int i = 0; i < 4; ++i) b.spike({-0.045f + i * 0.03f, 0.07f, -0.70f}, 0.01f, {-0.045f + i * 0.03f, 0.11f, -0.72f}, kWhite);
        b.box(0.05f, 0.16f, -0.62f, 0.01f, 0.03f, 0.04f, kWhite);   // ojos del tiburon
        b.box(-0.05f, 0.16f, -0.62f, 0.01f, 0.03f, 0.04f, kWhite);
        b.box(-0.08f, 0.17f, -0.20f, 0.03f, 0.05f, 0.12f, kBlack);  // mira lateral
    }
    pistolGrip(b, kWoodDark);
    b.box(0.0f, 0.04f, -0.02f, 0.03f, 0.06f, 0.08f, kDark);            // union con el tubo
    b.box(0.0f, 0.02f, -0.30f, 0.032f, 0.12f, 0.045f, kWoodDark);      // empunadura delantera
}

void buildGrenadeLauncher(Builder& b) {
    // Lanzagranadas (Icon_Weapons_SK_Grenade_Launcher): tambor de laton, canon ancho.
    b.tubeZ(0.0f, 0.045f, -0.20f, -0.03f, 0.075f, 8, kBrass);          // tambor
    b.tubeZ(0.0f, 0.065f, -0.48f, -0.20f, 0.038f, 8, kDark);           // canon
    b.tubeZ(0.0f, 0.065f, -0.50f, -0.48f, 0.046f, 8, kBlack);
    b.box(0.0f, 0.10f, 0.0f, 0.05f, 0.04f, 0.16f, kDark);              // cajon
    b.detail(0.0f, 0.135f, -0.30f, 0.02f, 0.03f, 0.06f, kBrassDark);   // mira
    b.box(0.0f, -0.03f, -0.33f, 0.03f, 0.10f, 0.04f, kDark);           // empunadura delantera
    pistolGrip(b, kBrassDark);
    b.box(0.0f, 0.07f, 0.17f, 0.04f, 0.05f, 0.18f, kDark);
    b.box(0.0f, 0.04f, 0.27f, 0.046f, 0.12f, 0.035f, kBrassDark);
}

void buildGrenade(Builder& b) {
    // Granada (Recursos/Weapons/Granada.png): bote metalico con una franja de luz azul.
    b.tubeY(0.0f, -0.06f, 0.0f, 0.06f, 0.045f, 8, kSilver);
    b.tubeY(0.0f, 0.06f, 0.0f, 0.08f, 0.05f, 8, kGrey);
    b.tubeY(0.0f, -0.08f, 0.0f, -0.06f, 0.05f, 8, kGrey);
    b.box(0.0f, 0.0f, -0.045f, 0.02f, 0.09f, 0.008f, kGlowBlue);
}

void buildConsumable(Builder& b, ItemModel model) {
    switch (model) {
        case ItemModel::Bandage:  // dos rollos blancos con banda roja (Referencias/Armas/Bandage)
            b.setLocal(Mat4::rotationZ(0.5f * math3d::kPi));
            b.tubeZ(0.0f, 0.0f, -0.08f, 0.0f, 0.06f, 8, kWhite);
            b.tubeZ(0.0f, 0.0f, 0.01f, 0.09f, 0.06f, 8, kWhite);
            b.tubeZ(0.0f, 0.0f, -0.05f, -0.03f, 0.062f, 8, rgba(196, 50, 40), false);
            b.endLocal();
            break;
        case ItemModel::Medkit:
            b.box(0.0f, 0.0f, 0.0f, 0.30f, 0.20f, 0.10f, rgba(240, 240, 240));
            b.box(0.0f, 0.0f, -0.052f, 0.06f, 0.14f, 0.006f, rgba(214, 40, 40));
            b.box(0.0f, 0.0f, -0.052f, 0.15f, 0.05f, 0.006f, rgba(214, 40, 40));
            b.box(0.0f, 0.12f, 0.0f, 0.12f, 0.03f, 0.03f, kGrey);  // asa
            break;
        case ItemModel::SmallShield:  // frasco azul (Recursos/HUD/minipocion.png)
            b.tubeY(0.0f, -0.06f, 0.0f, 0.04f, 0.055f, 8, rgba(70, 150, 235));
            b.tubeY(0.0f, 0.04f, 0.0f, 0.09f, 0.024f, 6, rgba(110, 190, 250));
            b.tubeY(0.0f, 0.09f, 0.0f, 0.11f, 0.03f, 6, rgba(214, 214, 214));
            break;
        default:  // pocion de escudo (Referencias/Armas/Shield_Potion)
            b.tubeY(0.0f, -0.10f, 0.0f, 0.08f, 0.085f, 8, rgba(60, 130, 230));
            b.tubeY(0.0f, 0.08f, 0.0f, 0.12f, 0.04f, 6, rgba(110, 180, 250));
            b.tubeY(0.0f, 0.12f, 0.0f, 0.15f, 0.05f, 6, rgba(150, 150, 150));
            break;
    }
}

void buildSpikeTrap(Builder& b) {
    // Trampa plegada (botin del suelo): la placa con su marco y unos pinchos.
    b.box(0.0f, 0.0f, 0.0f, 0.36f, 0.04f, 0.36f, rgba(210, 200, 176));
    b.box(0.0f, 0.03f, -0.17f, 0.38f, 0.03f, 0.03f, kDark);
    b.box(0.0f, 0.03f, 0.17f, 0.38f, 0.03f, 0.03f, kDark);
    b.box(-0.17f, 0.03f, 0.0f, 0.03f, 0.03f, 0.38f, rgba(230, 190, 40));
    b.box(0.17f, 0.03f, 0.0f, 0.03f, 0.03f, 0.38f, rgba(230, 190, 40));
    for (int i = 0; i < 4; ++i) {
        const float x = (i % 2 ? 0.08f : -0.08f), z = (i / 2 ? 0.08f : -0.08f);
        b.spike({x, 0.02f, z}, 0.02f, {x, 0.13f, z}, kSilver);
    }
}

void buildRocket(Builder& b) {
    b.tubeZ(0.0f, 0.0f, -0.18f, 0.20f, 0.05f, 6, kOlive);
    b.tubeZ(0.0f, 0.0f, -0.28f, -0.18f, 0.03f, 6, kSharkRed);
    b.box(0.0f, 0.0f, 0.18f, 0.18f, 0.012f, 0.06f, kDark);  // aletas
    b.box(0.0f, 0.0f, 0.18f, 0.012f, 0.18f, 0.06f, kDark);
    b.tubeZ(0.0f, 0.0f, 0.20f, 0.30f, 0.035f, 6, rgba(255, 180, 60));  // llama del motor
}

void buildLauncherGrenade(Builder& b) {
    b.tubeZ(0.0f, 0.0f, -0.06f, 0.06f, 0.035f, 6, kBrass);
    b.tubeZ(0.0f, 0.0f, -0.09f, -0.06f, 0.025f, 6, kDark);
}

}  // namespace

ItemModel itemModelFor(ItemType type, Rarity rarity) {
    switch (type) {
        case ItemType::AssaultRifle: return rarity >= Rarity::Epic ? ItemModel::Scar : ItemModel::AssaultRifle;
        case ItemType::Shotgun: return ItemModel::PumpShotgun;
        case ItemType::Smg: return ItemModel::SuppressedSmg;
        case ItemType::Sniper: return ItemModel::BoltSniper;
        case ItemType::Bandage: return ItemModel::Bandage;
        case ItemType::Medkit: return ItemModel::Medkit;
        case ItemType::SmallShield: return ItemModel::SmallShield;
        case ItemType::ShieldPotion: return ItemModel::ShieldPotion;
        case ItemType::BurstRifle: return ItemModel::BurstRifle;
        case ItemType::ScopedRifle: return ItemModel::ScopedRifle;
        case ItemType::SubmachineGun: return ItemModel::Smg;
        case ItemType::TacticalShotgun: return ItemModel::TacticalShotgun;
        case ItemType::Pistol: return ItemModel::Pistol;
        case ItemType::Revolver: return ItemModel::Revolver;
        case ItemType::SemiAutoSniper: return ItemModel::SemiAutoSniper;
        case ItemType::RocketLauncher: return ItemModel::RocketLauncher;
        case ItemType::GrenadeLauncher: return ItemModel::GrenadeLauncher;
        case ItemType::Grenade: return ItemModel::Grenade;
        case ItemType::SpikeTrap: return ItemModel::SpikeTrap;
        case ItemType::None:
        case ItemType::Count: break;
    }
    return ItemModel::None;
}

bool ItemModels::build() {
    // Mano izquierda (guardamanos) y boca del canon de cada modelo, y como se sujeta.
    struct Spec {
        ItemModel model;
        Vec3 support, muzzle;
        GripStyle grip;
    };
    const Spec specs[] = {
        {ItemModel::Pickaxe, {0.05f, 0.0f, -0.20f}, {0.05f, 0.30f, -0.55f}, GripStyle::Pickaxe},
        {ItemModel::AssaultRifle, {0.0f, 0.03f, -0.34f}, {0.0f, 0.06f, -0.66f}, GripStyle::Rifle},
        {ItemModel::Scar, {0.0f, 0.02f, -0.36f}, {0.0f, 0.065f, -0.71f}, GripStyle::Rifle},
        {ItemModel::BurstRifle, {0.0f, 0.02f, -0.33f}, {0.0f, 0.062f, -0.74f}, GripStyle::Rifle},
        {ItemModel::ScopedRifle, {0.0f, 0.02f, -0.36f}, {0.0f, 0.06f, -0.72f}, GripStyle::Rifle},
        {ItemModel::SuppressedSmg, {0.0f, -0.07f, -0.06f}, {0.0f, 0.062f, -0.56f}, GripStyle::Rifle},
        {ItemModel::Smg, {0.0f, -0.10f, -0.11f}, {0.0f, 0.065f, -0.46f}, GripStyle::Rifle},
        {ItemModel::PumpShotgun, {0.0f, 0.02f, -0.43f}, {0.0f, 0.072f, -0.76f}, GripStyle::Rifle},
        {ItemModel::TacticalShotgun, {0.0f, 0.02f, -0.36f}, {0.0f, 0.07f, -0.58f}, GripStyle::Rifle},
        {ItemModel::Pistol, {0.0f, -0.04f, 0.01f}, {0.0f, 0.068f, -0.185f}, GripStyle::Pistol},
        {ItemModel::Revolver, {0.0f, -0.04f, 0.01f}, {0.0f, 0.072f, -0.33f}, GripStyle::Pistol},
        {ItemModel::BoltSniper, {0.0f, 0.0f, -0.36f}, {0.0f, 0.07f, -1.02f}, GripStyle::Rifle},
        {ItemModel::SemiAutoSniper, {0.0f, 0.02f, -0.40f}, {0.0f, 0.065f, -0.93f}, GripStyle::Rifle},
        {ItemModel::RocketLauncher, {0.0f, 0.0f, -0.30f}, {0.0f, 0.13f, -0.74f}, GripStyle::Shoulder},
        {ItemModel::GrenadeLauncher, {0.0f, -0.05f, -0.33f}, {0.0f, 0.065f, -0.50f}, GripStyle::Rifle},
        {ItemModel::Grenade, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, GripStyle::Throw},
    };
    for (ItemModelInfo& i : m_info) i = ItemModelInfo{};
    for (const Spec& s : specs) {
        ItemModelInfo& i = m_info[static_cast<int>(s.model)];
        i.support = s.support;
        i.muzzle = s.muzzle;
        i.grip = s.grip;
    }
    bool ok = true;
    for (int lod = 0; lod < 2; ++lod) {
        Builder b(lod == 0);
        for (int m = 1; m < kCount; ++m) {
            const ItemModel model = static_cast<ItemModel>(m);
            switch (model) {
                case ItemModel::Pickaxe: buildPickaxe(b); break;
                case ItemModel::AssaultRifle: buildAssaultRifle(b); break;
                case ItemModel::Scar: buildScar(b); break;
                case ItemModel::BurstRifle: buildBurstRifle(b); break;
                case ItemModel::ScopedRifle: buildScopedRifle(b); break;
                case ItemModel::SuppressedSmg: buildSuppressedSmg(b); break;
                case ItemModel::Smg: buildSmg(b); break;
                case ItemModel::PumpShotgun: buildPumpShotgun(b); break;
                case ItemModel::TacticalShotgun: buildTacticalShotgun(b); break;
                case ItemModel::Pistol: buildPistol(b); break;
                case ItemModel::Revolver: buildRevolver(b); break;
                case ItemModel::BoltSniper: buildBoltSniper(b); break;
                case ItemModel::SemiAutoSniper: buildSemiAutoSniper(b); break;
                case ItemModel::RocketLauncher: buildRocketLauncher(b); break;
                case ItemModel::GrenadeLauncher: buildGrenadeLauncher(b); break;
                case ItemModel::Grenade: buildGrenade(b); break;
                case ItemModel::Rocket: buildRocket(b); break;
                case ItemModel::SpikeTrap: buildSpikeTrap(b); break;
                case ItemModel::LauncherGrenade: buildLauncherGrenade(b); break;
                case ItemModel::Bandage:
                case ItemModel::Medkit:
                case ItemModel::SmallShield:
                case ItemModel::ShieldPotion: buildConsumable(b, model); break;
                case ItemModel::None:
                case ItemModel::Count: break;
            }
            ok &= b.finish(m_meshes[lod][m], &m_triangles[lod][m], lod == 0 ? &m_info[m].center : nullptr);
        }
    }
    m_ready = ok;
    return ok;
}

void ItemModels::destroy() {
    for (auto& lod : m_meshes) {
        for (gfx::Mesh& m : lod) gfx::destroyMesh(m);
    }
    m_ready = false;
}

void ItemModels::draw(ItemModel model, const Mat4& transform, bool lowDetail) const {
    const int m = static_cast<int>(model);
    if (m <= 0 || m >= kCount || !m_ready) return;
    gfx::bindTexture(nullptr);
    gfx::setModelMatrix(transform);
    gfx::drawMesh(m_meshes[lowDetail ? 1 : 0][m]);
}

}  // namespace game
