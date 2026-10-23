#include "game/character_model.h"

#include <math.h>

#include <vector>

#include "gfx/graphics.h"
#include "world/terrain.h"

namespace game {

using gfx::rgba;
using math3d::Mat4;
using math3d::Vec3;

namespace palettes {
//                         piel                  pelo                  cejas
const CharacterPalette kPlayer = {
    rgba(232, 172, 128), rgba(236, 200, 96),  rgba(196, 150, 70),
    // camiseta           chaleco/correas       panuelo
    rgba(74, 84, 54),    rgba(56, 62, 42),     rgba(196, 192, 156),
    // cinturon           hebilla
    rgba(64, 70, 46),    rgba(184, 188, 184),
    // pantalon           correas
    rgba(200, 150, 96),  rgba(50, 44, 34),
    // botas              rodilleras
    rgba(50, 48, 44),    rgba(116, 124, 62),
    // guantes            brazalete
    rgba(74, 80, 54),    rgba(196, 62, 42),
    true, false, true};
const CharacterPalette kCommando = {
    rgba(200, 150, 112), rgba(66, 48, 34),    rgba(50, 36, 26),
    rgba(44, 46, 48),    rgba(30, 30, 32),    rgba(110, 112, 108),
    rgba(30, 30, 30),    rgba(150, 152, 150),
    rgba(118, 120, 118), rgba(30, 30, 30),
    rgba(30, 30, 32),    rgba(80, 82, 84),
    rgba(34, 34, 36),    rgba(90, 92, 90),
    true, false, true};
const CharacterPalette kOrange = {
    rgba(214, 160, 120), rgba(36, 30, 26),    rgba(40, 32, 26),
    rgba(226, 124, 42),  rgba(176, 90, 30),   rgba(236, 210, 130),
    rgba(70, 56, 42),    rgba(200, 200, 196),
    rgba(226, 124, 42),  rgba(70, 56, 42),
    rgba(70, 56, 42),    rgba(236, 150, 60),
    rgba(70, 56, 42),    rgba(240, 222, 200),
    false, false, false};
const CharacterPalette kRanger = {
    rgba(172, 122, 92),  rgba(184, 54, 48),   rgba(50, 36, 28),
    rgba(176, 54, 48),   rgba(92, 104, 146),  rgba(206, 196, 172),
    rgba(44, 44, 54),    rgba(220, 200, 90),
    rgba(92, 104, 146),  rgba(44, 44, 54),
    rgba(44, 44, 54),    rgba(110, 124, 172),
    rgba(44, 44, 54),    rgba(232, 212, 84),
    false, true, true};
}  // namespace palettes

namespace {

// --- Esqueleto (metros, pies en Y = 0, frente hacia -Z) -----------------------------------
constexpr float kHipY = 0.95f;
constexpr float kHipX = 0.105f;
constexpr float kKneeDrop = 0.43f;    // cadera -> rodilla
constexpr float kShoulderY = 1.395f;
constexpr float kShoulderX = 0.245f;
constexpr float kElbowDrop = 0.285f;  // hombro -> codo
constexpr float kArmSpread = 0.12f;   // rad: los brazos no cuelgan pegados al cuerpo
constexpr float kStrideLength = 1.6f; // metros por ciclo completo de pasos

// Lados de los anillos en cada nivel de detalle y distancia a partir de la que se usa.
constexpr int kLodSides[2] = {8, 4};

const u32 kGun = rgba(204, 170, 104);      // tostado del SCAR (Referencias/Armas)
const u32 kGunDark = rgba(52, 50, 46);
const u32 kHandle = rgba(110, 76, 44);     // mango del pico
const u32 kPickHead = rgba(120, 150, 200); // cabeza azulada (Recursos/HUD/Pico.png)

// --- Textura de detalle (64x64) ---------------------------------------------------------
// Se tine con el color de cada vertice: el blanco deja el color tal cual y lo gris lo
// oscurece. Asi un mismo dibujo sirve para todas las paletas.
constexpr int kTexSize = 64;
struct Tile {
    int x, y, w, h;
};
enum class Uv : u8 { Plain, Camo, Cloth, Hair, Boot, Face };
constexpr Tile kTiles[] = {
    {0, 0, 16, 16},   // Plain: blanco
    {16, 0, 16, 16},  // Camo: manchas en tres tonos
    {32, 0, 16, 16},  // Cloth: tejido con costuras
    {48, 0, 16, 16},  // Hair: mechones
    {32, 16, 16, 16}, // Boot: cuero con suela oscura abajo
    {0, 16, 32, 32},  // Face: ojos, cejas, nariz y boca sobre la mitad delantera de la cabeza
};

u32 hash32(u32 x) {
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
}

float channel(u32 c, int shift) { return static_cast<float>((c >> shift) & 0xFF); }

// Tinte relativo: que color de textura hace que `target` salga sobre la piel `base`.
float ratio(u32 target, u32 base, int shift) {
    const float b = channel(base, shift);
    const float r = b > 0.0f ? channel(target, shift) / b : 1.0f;
    return r > 1.0f ? 1.0f : r;
}

bool buildSkinTexture(gfx::Texture& tex, const CharacterPalette& p) {
    if (!gfx::createTexture(tex, kTexSize, kTexSize, GU_PSM_5551)) return false;
    u16* px = static_cast<u16*>(tex.pixels);
    auto put = [&](int x, int y, float r, float g, float b) {
        auto c = [](float v) { return static_cast<u32>(v < 0.0f ? 0.0f : (v > 1.0f ? 255.0f : v * 255.0f)); };
        px[y * tex.stride + x] = gfx::pack5551(c(r), c(g), c(b));
    };
    auto grey = [&](int x, int y, float v) { put(x, y, v, v, v); };
    for (int y = 0; y < kTexSize; ++y)
        for (int x = 0; x < kTexSize; ++x) grey(x, y, 1.0f);

    // Camuflaje: manchas suaves periodicas (la baldosa se repite sin costura).
    const Tile& camo = kTiles[static_cast<int>(Uv::Camo)];
    for (int y = 0; y < 16; ++y) {
        for (int x = 0; x < 16; ++x) {
            const float a = 2.0f * math3d::kPi / 16.0f;
            const float n = sinf(a * x + 1.3f) * cosf(a * y * 2.0f + 0.4f) + 0.6f * sinf(a * (x + y) * 2.0f) +
                            0.45f * cosf(a * (3 * x - y) + 2.1f);
            grey(camo.x + x, camo.y + y, n > 0.55f ? 0.62f : (n > -0.15f ? 0.82f : 1.0f));
        }
    }
    // Tejido: costuras horizontales y algo de grano.
    const Tile& cloth = kTiles[static_cast<int>(Uv::Cloth)];
    for (int y = 0; y < 16; ++y) {
        for (int x = 0; x < 16; ++x) {
            float v = 0.92f + 0.08f * static_cast<float>(hash32(x * 31 + y * 17) & 7) / 7.0f;
            if (y % 8 == 7) v = 0.80f;
            grey(cloth.x + x, cloth.y + y, v);
        }
    }
    // Pelo: mechones diagonales.
    const Tile& hair = kTiles[static_cast<int>(Uv::Hair)];
    for (int y = 0; y < 16; ++y) {
        for (int x = 0; x < 16; ++x) grey(hair.x + x, hair.y + y, ((x + y / 3) % 4) < 2 ? 1.0f : 0.78f);
    }
    // Botas: cuero con una costura y la suela (oscura) en las ultimas filas.
    const Tile& boot = kTiles[static_cast<int>(Uv::Boot)];
    for (int y = 0; y < 16; ++y) {
        for (int x = 0; x < 16; ++x) grey(boot.x + x, boot.y + y, y >= 12 ? 0.45f : (y == 4 ? 0.78f : 0.95f));
    }

    // Cara: 32x32 sobre los 180 grados delanteros de la cabeza, de la linea del pelo
    // (fila 0) a la barbilla (fila 31). Los colores se expresan relativos a la piel.
    const Tile& face = kTiles[static_cast<int>(Uv::Face)];
    auto faceDot = [&](int x, int y, u32 target, float extraDark = 1.0f) {
        put(face.x + x, face.y + y, ratio(target, p.skin, 0) * extraDark, ratio(target, p.skin, 8) * extraDark,
            ratio(target, p.skin, 16) * extraDark);
    };
    for (int y = 0; y < 32; ++y) {
        for (int x = 0; x < 32; ++x) {
            // Mejillas y laterales algo mas oscuros: la cara se lee con volumen.
            const float side = fabsf(x - 15.5f) / 15.5f;
            float v = 1.0f - 0.10f * side * side;
            if (y >= 27) v *= 0.94f;  // sombra bajo la mandibula
            grey(face.x + x, face.y + y, v);
        }
    }
    const u32 eye = rgba(40, 46, 60);
    const u32 browShadow = rgba(170, 120, 90);
    for (int s = 0; s < 2; ++s) {
        const int ex = s == 0 ? 10 : 20;  // ojos a +-25 grados del frente
        // Cejas gruesas y algo inclinadas, como las de las skins de Fortnite.
        for (int k = 0; k < 4; ++k) {
            faceDot(ex - 1 + k, 9 + ((s == 0) == (k < 2) ? 0 : 1), p.brow);
            faceDot(ex - 1 + k, 10, p.brow);
        }
        for (int k = 0; k < 3; ++k) faceDot(ex + k - (s == 0 ? 0 : 1), 12, browShadow, 0.95f);
        faceDot(ex, 13, eye);
        faceDot(ex + 1, 13, eye);
        faceDot(ex, 14, eye);
        faceDot(ex + 1, 14, eye);
        faceDot(s == 0 ? ex - 1 : ex + 2, 13, rgba(250, 250, 250));  // brillo / blanco del ojo
    }
    // Nariz: sombra lateral y aleta.
    for (int y = 14; y <= 20; ++y) grey(face.x + 16, face.y + y, 0.90f);
    grey(face.x + 15, face.y + 21, 0.78f);
    grey(face.x + 16, face.y + 21, 0.70f);
    grey(face.x + 17, face.y + 21, 0.78f);
    // Boca: linea de los labios y sombra del labio inferior.
    for (int x = 13; x <= 18; ++x) grey(face.x + x, face.y + 24, 0.70f);
    for (int x = 14; x <= 17; ++x) grey(face.x + x, face.y + 25, 0.88f);
    // Barbilla marcada.
    for (int x = 13; x <= 18; ++x) grey(face.x + x, face.y + 29, 0.88f);

    gfx::uploadTexture(tex);
    return true;
}

// --- Constructor de mallas --------------------------------------------------------------

// Anillo eliptico horizontal (en el espacio de la pieza). Los vertices de atras (+Z)
// bajan `back` metros: asi el pelo cubre la nuca sin tapar la frente.
struct Ring {
    float y, rx, rz;
    float cx = 0.0f, cz = 0.0f, back = 0.0f;
};

Ring ring(float y, float rx, float rz, float cx = 0.0f, float cz = 0.0f, float back = 0.0f) {
    Ring r;
    r.y = y;
    r.rx = rx;
    r.rz = rz;
    r.cx = cx;
    r.cz = cz;
    r.back = back;
    return r;
}

// Luz horneada: principal desde arriba-delante-derecha y relleno desde atras, para que
// la espalda (lo que ve la camara del jugador) tambien tenga volumen.
u32 lit(u32 color, const Vec3& n) {
    static const Vec3 kKey = math3d::normalize({0.35f, 0.85f, -0.40f});
    static const Vec3 kFill = math3d::normalize({-0.45f, 0.35f, 0.82f});
    float b = 0.50f + 0.45f * fmaxf(0.0f, math3d::dot(n, kKey)) + 0.22f * fmaxf(0.0f, math3d::dot(n, kFill));
    if (n.y < 0.0f) b += 0.12f * n.y;  // lo que mira al suelo, mas oscuro
    auto ch = [&](int shift) {
        const float v = channel(color, shift) * b;
        return static_cast<u32>(v > 255.0f ? 255.0f : v);
    };
    return rgba(ch(0), ch(8), ch(16), color >> 24);
}

class MeshBuilder {
public:
    MeshBuilder(int sides, bool details) : m_sides(sides), m_details(details) {}

    bool details() const { return m_details; }
    void setTransform(const Mat4& t) {
        m_transform = t;
        m_hasTransform = true;
    }
    void resetTransform() { m_hasTransform = false; }

    // Tramo de superficie entre dos anillos. `faceY0..faceY1`: rango de alturas de la cara.
    void segment(const Ring& a, const Ring& b, u32 color, Uv uv = Uv::Plain, float faceY0 = 0.0f,
                 float faceY1 = 1.0f) {
        const int n = m_sides;
        const float ra = 0.5f * (a.rx + a.rz), rb = 0.5f * (b.rx + b.rz);
        const float dy = b.y - a.y;
        float slope = fabsf(dy) > 1e-4f ? (ra - rb) / dy : 0.0f;
        slope = slope > 2.5f ? 2.5f : (slope < -2.5f ? -2.5f : slope);
        const Tile& t = kTiles[static_cast<int>(uv)];
        for (int i = 0; i < n; ++i) {
            const float a0 = 2.0f * math3d::kPi * i / n, a1 = 2.0f * math3d::kPi * (i + 1) / n;
            Vec3 p[4] = {point(a, a0), point(a, a1), point(b, a1), point(b, a0)};
            Vec3 nn[4] = {normal(a0, slope), normal(a1, slope), normal(a1, slope), normal(a0, slope)};
            float u[4], v[4];
            const float angles[4] = {a0, a1, a1, a0};
            const bool face = uv == Uv::Face && fabsf(wrapPi(0.5f * (a0 + a1))) < 0.5f * math3d::kPi - 0.05f;
            for (int k = 0; k < 4; ++k) {
                if (uv == Uv::Plain || (uv == Uv::Face && !face)) {
                    u[k] = 8.0f;
                    v[k] = 8.0f;
                } else if (face) {
                    const float s = 0.5f + wrapPi(angles[k]) / math3d::kPi;  // 0 = izquierda, 1 = derecha
                    const float h = (faceY1 - p[k].y) / (faceY1 - faceY0);
                    u[k] = t.x + 0.5f + (t.w - 1.0f) * clamp01(s);
                    v[k] = t.y + 0.5f + (t.h - 1.0f) * clamp01(h);
                } else {
                    u[k] = t.x + 0.5f + (t.w - 1.0f) * (k == 0 || k == 3 ? i : i + 1) / n;
                    v[k] = t.y + 0.5f + (t.h - 1.0f) * (k < 2 ? 0.0f : 1.0f);
                }
            }
            quad(p, nn, u, v, color);
        }
    }
    // Abanico que cierra un anillo hacia un punto (cima de la cabeza, punta de los dedos).
    void cap(const Ring& r, const Vec3& tip, u32 color, Uv uv = Uv::Plain) {
        const int n = m_sides;
        const Tile& t = kTiles[static_cast<int>(uv)];
        const Vec3 axis = math3d::normalize(tip - Vec3{r.cx, r.y, r.cz});
        for (int i = 0; i < n; ++i) {
            const float a0 = 2.0f * math3d::kPi * i / n, a1 = 2.0f * math3d::kPi * (i + 1) / n;
            const Vec3 p0 = point(r, a0), p1 = point(r, a1);
            const Vec3 n0 = math3d::normalize(normal(a0, 0.0f) + axis * 1.2f);
            const Vec3 n1 = math3d::normalize(normal(a1, 0.0f) + axis * 1.2f);
            const float u0 = uv == Uv::Plain ? 8.0f : t.x + 0.5f + (t.w - 1.0f) * i / n;
            const float u1 = uv == Uv::Plain ? 8.0f : t.x + 0.5f + (t.w - 1.0f) * (i + 1) / n;
            const float vv = uv == Uv::Plain ? 8.0f : t.y + 0.5f;
            const u16 base = static_cast<u16>(m_vertices.size());
            emit(p0, n0, u0, vv, color);
            emit(p1, n1, u1, vv, color);
            emit(tip, axis, 0.5f * (u0 + u1), uv == Uv::Plain ? 8.0f : t.y + t.h - 0.5f, color);
            m_indices.push_back(base);
            m_indices.push_back(base + 1);
            m_indices.push_back(base + 2);
        }
    }
    // Caja de caras planas (arma, hebillas, bolsillos): aristas duras.
    void box(const Vec3& mn, const Vec3& mx, u32 color) {
        const Vec3 c[8] = {{mn.x, mn.y, mn.z}, {mx.x, mn.y, mn.z}, {mx.x, mx.y, mn.z}, {mn.x, mx.y, mn.z},
                           {mn.x, mn.y, mx.z}, {mx.x, mn.y, mx.z}, {mx.x, mx.y, mx.z}, {mn.x, mx.y, mx.z}};
        const int f[6][4] = {{0, 1, 2, 3}, {5, 4, 7, 6}, {1, 5, 6, 2}, {4, 0, 3, 7}, {3, 2, 6, 7}, {4, 5, 1, 0}};
        const Vec3 normals[6] = {{0, 0, -1}, {0, 0, 1}, {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}};
        for (int i = 0; i < 6; ++i) {
            Vec3 p[4] = {c[f[i][0]], c[f[i][1]], c[f[i][2]], c[f[i][3]]};
            Vec3 nn[4] = {normals[i], normals[i], normals[i], normals[i]};
            const float u[4] = {8, 8, 8, 8}, v[4] = {8, 8, 8, 8};
            quad(p, nn, u, v, color);
        }
    }
    // Puntas (mechones del tupe, pico del panuelo): piramide de 4 caras.
    void spike(const Vec3& base, float rx, float rz, const Vec3& tip, u32 color, Uv uv = Uv::Plain) {
        Ring r = ring(base.y, rx, rz, base.x, base.z);
        const int saved = m_sides;
        m_sides = 4;
        cap(r, tip, color, uv);
        m_sides = saved;
    }

    int triangles() const { return static_cast<int>(m_indices.size() / 3); }
    // Crea la malla (una sola llamada de dibujo por pieza) y vacia el constructor.
    bool finish(gfx::Mesh& out) {
        const bool ok = !m_vertices.empty() && m_vertices.size() <= gfx::kMaxMeshVertices &&
                        gfx::createMesh(out, m_vertices.data(), static_cast<int>(m_vertices.size()),
                                        m_indices.data(), static_cast<int>(m_indices.size()));
        m_vertices.clear();
        m_indices.clear();
        resetTransform();
        return ok;
    }

private:
    static float clamp01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }
    static float wrapPi(float a) {
        while (a > math3d::kPi) a -= 2.0f * math3d::kPi;
        while (a < -math3d::kPi) a += 2.0f * math3d::kPi;
        return a;
    }
    // Angulo 0 = frente (-Z), pi/2 = derecha (+X).
    static Vec3 point(const Ring& r, float a) {
        const float back = -cosf(a) > 0.0f ? -cosf(a) : 0.0f;
        return {r.cx + r.rx * sinf(a), r.y - r.back * back, r.cz - r.rz * cosf(a)};
    }
    static Vec3 normal(float a, float slope) { return math3d::normalize({sinf(a), slope, -cosf(a)}); }

    void emit(const Vec3& p, const Vec3& n, float u, float v, u32 color) {
        Vec3 wp = p, wn = n;
        if (m_hasTransform) {
            wp = m_transform.transformPoint(p);
            wn = math3d::normalize(m_transform.transformPoint(n) - m_transform.transformPoint({0, 0, 0}));
        }
        m_vertices.push_back({u / kTexSize, v / kTexSize, lit(color, wn), wp.x, wp.y, wp.z});
    }
    void quad(const Vec3* p, const Vec3* n, const float* u, const float* v, u32 color) {
        const u16 base = static_cast<u16>(m_vertices.size());
        for (int k = 0; k < 4; ++k) emit(p[k], n[k], u[k], v[k], color);
        const u16 idx[6] = {0, 1, 2, 0, 2, 3};
        for (u16 i : idx) m_indices.push_back(base + i);
    }

    int m_sides;
    bool m_details;
    Mat4 m_transform = Mat4::identity();
    bool m_hasTransform = false;
    std::vector<gfx::Vertex> m_vertices;
    std::vector<u16> m_indices;
};

// --- Piezas -----------------------------------------------------------------------------

void buildBody(MeshBuilder& b, const CharacterPalette& p) {
    const Uv pantsUv = p.camoPants ? Uv::Camo : Uv::Cloth;
    // Cadera (pantalon) y cinturon con hebilla.
    b.segment(ring(0.84f, 0.165f, 0.125f), ring(0.985f, 0.172f, 0.128f), p.pants, pantsUv);
    b.segment(ring(0.975f, 0.182f, 0.136f), ring(1.065f, 0.176f, 0.131f), p.belt);
    b.cap(ring(0.84f, 0.165f, 0.125f), {0.0f, 0.80f, 0.0f}, p.pants);
    // Torso en V: cintura estrecha, pecho y hombros anchos.
    const Ring torso[] = {ring(1.06f, 0.163f, 0.12f), ring(1.18f, 0.172f, 0.124f, 0.0f, -0.004f),
                          ring(1.30f, 0.205f, 0.140f, 0.0f, -0.012f), ring(1.40f, 0.236f, 0.146f, 0.0f, -0.008f),
                          ring(1.465f, 0.212f, 0.130f), ring(1.515f, 0.115f, 0.095f)};
    for (int i = 0; i + 1 < 6; ++i) b.segment(torso[i], torso[i + 1], p.shirt, Uv::Cloth);
    // Cuello y cabeza (cara texturizada en la mitad delantera).
    b.segment(ring(1.50f, 0.066f, 0.064f, 0.0f, 0.006f), ring(1.585f, 0.062f, 0.060f, 0.0f, 0.004f), p.skin);
    const float faceTop = 1.84f, faceBottom = 1.575f;
    // Mandibula ancha y cabeza algo grande, como en las skins de Fortnite.
    const Ring head[] = {ring(1.572f, 0.074f, 0.076f, 0.0f, -0.016f), ring(1.625f, 0.104f, 0.106f, 0.0f, -0.007f),
                         ring(1.705f, 0.110f, 0.115f), ring(1.785f, 0.107f, 0.117f, 0.0f, 0.004f),
                         ring(1.855f, 0.090f, 0.102f, 0.0f, 0.008f)};
    for (int i = 0; i + 1 < 5; ++i) b.segment(head[i], head[i + 1], p.skin, Uv::Face, faceBottom, faceTop);
    b.cap(head[4], {0.0f, 1.895f, 0.01f}, p.skin);
    b.cap(head[0], {0.0f, 1.56f, -0.004f}, p.skin);
    if (b.details()) {
        b.box({-0.013f, 1.664f, -0.130f}, {0.013f, 1.712f, -0.110f}, p.skin);   // nariz
        b.box({-0.120f, 1.668f, -0.012f}, {-0.103f, 1.738f, 0.022f}, p.skin);   // orejas
        b.box({0.103f, 1.668f, -0.012f}, {0.120f, 1.738f, 0.022f}, p.skin);
    }

    if (p.hasHelmet) {
        // Casco redondeado con visera.
        const Ring helm[] = {ring(1.735f, 0.126f, 0.133f, 0.0f, 0.004f, 0.05f), ring(1.86f, 0.121f, 0.129f, 0.0f, 0.006f),
                             ring(1.935f, 0.081f, 0.091f, 0.0f, 0.008f)};
        b.segment(helm[0], helm[1], p.hair);
        b.segment(helm[1], helm[2], p.hair);
        b.cap(helm[2], {0.0f, 1.965f, 0.01f}, p.hair);
        b.box({-0.115f, 1.735f, -0.168f}, {0.115f, 1.752f, -0.10f}, p.hair);
    } else {
        // Pelo corto con tupe (Referencias/Personaje): casquete que baja por la nuca y una
        // onda que sube y se echa hacia delante sobre la frente.
        const Ring hair[] = {ring(1.79f, 0.115f, 0.124f, 0.0f, 0.006f, 0.10f), ring(1.865f, 0.110f, 0.120f, 0.0f, 0.004f, 0.03f),
                             ring(1.93f, 0.080f, 0.096f, 0.0f, 0.0f)};
        b.segment(hair[0], hair[1], p.hair, Uv::Hair);
        b.segment(hair[1], hair[2], p.hair, Uv::Hair);
        b.cap(hair[2], {0.0f, 1.952f, 0.012f}, p.hair, Uv::Hair);
        if (b.details()) {
            // Onda del tupe: un volumen ancho que sobresale por delante y arriba.
            const Ring q0 = ring(1.86f, 0.080f, 0.050f, 0.004f, -0.075f);
            const Ring q1 = ring(1.925f, 0.070f, 0.050f, 0.006f, -0.092f);
            b.segment(q0, q1, p.hair, Uv::Hair);
            b.cap(q1, {0.01f, 1.955f, -0.112f}, p.hair, Uv::Hair);
            b.spike({-0.05f, 1.90f, -0.07f}, 0.035f, 0.03f, {-0.07f, 1.945f, -0.10f}, p.hair, Uv::Hair);
        }
    }

    if (p.hasScarf) {
        // Panuelo al cuello que cae en pico sobre el pecho.
        b.segment(ring(1.435f, 0.150f, 0.128f, 0.0f, -0.004f), ring(1.535f, 0.092f, 0.088f, 0.0f, 0.004f), p.scarf,
                  Uv::Cloth);
        b.spike({0.0f, 1.47f, -0.118f}, 0.075f, 0.022f, {0.0f, 1.33f, -0.152f}, p.scarf, Uv::Cloth);
    }
    if (b.details()) {
        b.box({-0.046f, 0.992f, -0.146f}, {0.046f, 1.052f, -0.128f}, p.buckle);  // hebilla
        // Correa en bandolera por delante y bolsa en la cadera izquierda.
        b.setTransform(Mat4::translation({0.0f, 1.27f, -0.146f}) * Mat4::rotationZ(0.78f));
        b.box({-0.25f, -0.022f, -0.008f}, {0.25f, 0.022f, 0.006f}, p.vest);
        b.setTransform(Mat4::translation({0.0f, 1.27f, 0.141f}) * Mat4::rotationZ(-0.78f));
        b.box({-0.25f, -0.022f, -0.006f}, {0.25f, 0.022f, 0.008f}, p.vest);
        b.resetTransform();
        b.box({-0.222f, 0.84f, -0.10f}, {-0.165f, 1.00f, 0.03f}, p.vest);
    }
}

// Brazo (del hombro al codo), en coordenadas del hombro: el brazo cuelga hacia -Y.
void buildUpperArm(MeshBuilder& b, const CharacterPalette& p, float side) {
    // Deltoide redondo (se funde con el hombro del torso), biceps marcado y codo.
    const Ring r0 = ring(0.035f, 0.060f, 0.060f, 0.006f * side), r1 = ring(-0.03f, 0.094f, 0.092f, 0.012f * side);
    const Ring r2 = ring(-0.13f, 0.082f, 0.088f, 0.006f * side, -0.008f), r3 = ring(-0.235f, 0.062f, 0.064f);
    const Ring r4 = ring(-kElbowDrop, 0.057f, 0.057f);
    b.cap(r0, {0.006f * side, 0.06f, 0.0f}, p.skin);
    b.segment(r0, r1, p.skin);
    b.segment(r1, r2, p.skin);
    b.segment(r2, r3, p.skin);
    b.segment(r3, r4, p.skin);
    if (side > 0.0f) {
        // Brazalete rojo atado en el brazo derecho, con las puntas colgando.
        b.segment(ring(-0.085f, 0.097f, 0.097f, 0.01f), ring(-0.122f, 0.092f, 0.095f, 0.008f), p.armband);
        if (b.details()) {
            b.setTransform(Mat4::translation({0.10f, -0.11f, 0.02f}) * Mat4::rotationZ(0.5f));
            b.box({-0.012f, -0.10f, -0.016f}, {0.012f, 0.0f, 0.016f}, p.armband);
            b.resetTransform();
        }
    }
}

// Antebrazo y mano (del codo a los dedos), con lo que lleve en la mano derecha.
void buildForearm(MeshBuilder& b, const CharacterPalette& p, float side, HeldItem held) {
    const Ring f0 = ring(0.0f, 0.057f, 0.057f), f1 = ring(-0.07f, 0.071f, 0.066f, 0.0f, -0.004f);
    const Ring f2 = ring(-0.165f, 0.050f, 0.049f);
    b.segment(f0, f1, p.skin);
    b.segment(f1, f2, side < 0.0f ? p.glove : p.skin);  // protector en el antebrazo izquierdo
    // Guante sin dedos, manos grandes.
    const Ring g0 = ring(-0.16f, 0.055f, 0.054f), g1 = ring(-0.205f, 0.063f, 0.067f, 0.0f, -0.006f);
    const Ring g2 = ring(-0.265f, 0.059f, 0.063f, 0.0f, -0.008f), g3 = ring(-0.295f, 0.038f, 0.042f, 0.0f, -0.006f);
    b.segment(g0, g1, p.glove);
    b.segment(g1, g2, p.glove);
    b.segment(g2, g3, p.skin);
    b.cap(g3, {0.0f, -0.315f, -0.004f}, p.skin);
    if (side < 0.0f && b.details()) {
        b.box({-0.03f, -0.13f, -0.068f}, {0.03f, -0.09f, -0.052f}, rgba(80, 200, 230));  // pantallita azul
    }
    if (held == HeldItem::Rifle) {
        // SCAR a lo largo del antebrazo (al estirarlo hacia delante, apunta al frente);
        // -Z local es "arriba" del arma y +Z "abajo".
        const float x0 = -0.075f, x1 = -0.025f;
        b.box({x0, -0.62f, -0.034f}, {x1, -0.16f, 0.034f}, kGun);                    // cajon de mecanismos
        b.box({x0 + 0.006f, -0.16f, -0.022f}, {x1 - 0.006f, 0.04f, 0.050f}, kGun);   // culata
        b.box({-0.058f, -0.88f, -0.012f}, {-0.042f, -0.62f, 0.012f}, kGunDark);       // canon
        b.box({x0 + 0.008f, -0.43f, 0.034f}, {x1 - 0.008f, -0.35f, 0.14f}, kGunDark); // cargador
        b.box({x0 + 0.01f, -0.50f, -0.074f}, {x1 - 0.01f, -0.30f, -0.034f}, kGunDark); // mira
        b.box({x0 + 0.01f, -0.25f, 0.034f}, {x1 - 0.01f, -0.20f, 0.10f}, kGun);       // empunadura
    } else if (held == HeldItem::Pickaxe) {
        b.box({-0.022f, -0.80f, -0.022f}, {0.022f, -0.10f, 0.022f}, kHandle);
        b.box({-0.032f, -0.86f, -0.30f}, {0.032f, -0.75f, 0.13f}, kPickHead);
        b.box({-0.022f, -0.84f, -0.40f}, {0.022f, -0.77f, -0.30f}, kPickHead);  // punta
    }
}

// Muslo (de la cadera a la rodilla), en coordenadas de la cadera.
void buildThigh(MeshBuilder& b, const CharacterPalette& p, float side) {
    const Uv uv = p.camoPants ? Uv::Camo : Uv::Cloth;
    const Ring t0 = ring(0.06f, 0.100f, 0.106f), t1 = ring(-0.10f, 0.106f, 0.110f, 0.006f * side);
    const Ring t2 = ring(-0.26f, 0.088f, 0.093f, 0.004f * side), t3 = ring(-0.39f, 0.076f, 0.080f);
    const Ring t4 = ring(-kKneeDrop, 0.072f, 0.075f);
    b.segment(t0, t1, p.pants, uv);
    b.segment(t1, t2, p.pants, uv);
    b.segment(t2, t3, p.pants, uv);
    b.segment(t3, t4, p.pants, uv);
    // Dos correas en el muslo (como la referencia) y bolsillo lateral.
    b.segment(ring(-0.15f, 0.104f, 0.108f, 0.006f * side), ring(-0.185f, 0.101f, 0.105f, 0.006f * side), p.strap);
    if (b.details()) {
        b.segment(ring(-0.30f, 0.088f, 0.092f, 0.003f * side), ring(-0.33f, 0.085f, 0.088f), p.strap);
        const float x0 = side > 0.0f ? 0.088f : -0.126f;
        b.box({x0, -0.30f, -0.05f}, {x0 + 0.038f, -0.17f, 0.05f}, p.pants);
    }
}

// Espinilla, bota y pie (de la rodilla al suelo), en coordenadas de la rodilla.
void buildShin(MeshBuilder& b, const CharacterPalette& p) {
    const Uv uv = p.camoPants ? Uv::Camo : Uv::Cloth;
    b.segment(ring(0.0f, 0.072f, 0.075f), ring(-0.12f, 0.078f, 0.080f, 0.0f, 0.004f), p.pants, uv);
    // Bota alta con borde vuelto, tobillo y pie grande.
    b.segment(ring(-0.115f, 0.080f, 0.082f, 0.0f, 0.004f), ring(-0.16f, 0.074f, 0.077f), p.boot, Uv::Boot);
    b.segment(ring(-0.16f, 0.068f, 0.071f), ring(-0.30f, 0.061f, 0.064f), p.boot);
    b.segment(ring(-0.30f, 0.061f, 0.064f), ring(-0.42f, 0.058f, 0.062f, 0.0f, -0.004f), p.boot);
    const Ring foot0 = ring(-0.42f, 0.062f, 0.072f, 0.0f, -0.02f);
    const Ring foot1 = ring(-0.485f, 0.068f, 0.122f, 0.0f, -0.055f);
    const Ring foot2 = ring(-0.525f, 0.068f, 0.126f, 0.0f, -0.055f);
    b.segment(foot0, foot1, p.boot);
    b.segment(foot1, foot2, p.boot, Uv::Boot);
    b.cap(foot2, {0.0f, -0.53f, -0.055f}, rgba(34, 32, 30));
    if (b.details()) {
        // Rodillera y espinillera (las placas oliva de la referencia).
        b.box({-0.052f, -0.30f, -0.098f}, {0.052f, -0.13f, -0.074f}, p.kneePad);
        b.box({-0.048f, -0.12f, -0.100f}, {0.048f, -0.03f, -0.066f}, p.kneePad);
    }
}

void buildShadow(MeshBuilder& b) {
    // Mancha circular bajo los pies (se dibuja sin textura, con mezcla).
    b.cap(ring(0.0f, 0.45f, 0.45f), {0.0f, 0.001f, 0.0f}, rgba(0, 0, 0, 110));
}

// Suavizado exponencial independiente del frame rate.
float smoothToward(float current, float target, float rate, float dt) {
    return current + (target - current) * (1.0f - expf(-rate * dt));
}

}  // namespace

// --- Animacion --------------------------------------------------------------------------

void CharacterAnim::update(const CharacterState& state, bool aiming, bool alive, float dt) {
    idleTime += dt;
    recoil = smoothToward(recoil, 0.0f, 14.0f, dt);
    flinch = smoothToward(flinch, 0.0f, 8.0f, dt);
    swing = smoothToward(swing, 0.0f, 6.0f, dt);
    if (!alive) {
        deathSeconds += dt;
        return;
    }
    deathSeconds = 0.0f;
    const float speed = state.horizontalSpeed();
    float legL, legR, kneeL, kneeR;
    float armL, armR, elbowL, elbowR, inL = 0.0f, inR = 0.0f;
    if (state.grounded()) {
        // La fase avanza con la distancia recorrida: los pies no "patinan".
        const float before = walkPhase;
        walkPhase += speed * dt * (2.0f * math3d::kPi / kStrideLength);
        // Un pie apoya en las fases 0 y pi (piernas cruzandose bajo el cuerpo).
        if (speed > 1.0f && static_cast<int>(walkPhase / math3d::kPi) != static_cast<int>(before / math3d::kPi)) {
            ++footsteps;
        }
        if (walkPhase > 2.0f * math3d::kPi) walkPhase -= 2.0f * math3d::kPi;
        const float amplitude = 0.65f * math3d::clampf(speed / tuning::kRunSpeed, 0.0f, 1.0f);
        const float s = sinf(walkPhase) * amplitude;
        const float c = cosf(walkPhase);
        legL = s;
        legR = -s;
        // La rodilla se dobla mientras la pierna avanza (fase de vuelo del paso).
        kneeL = 0.06f + amplitude * 1.5f * (c > 0.0f ? c : 0.0f);
        kneeR = 0.06f + amplitude * 1.5f * (c < 0.0f ? -c : 0.0f);
        armL = -s * 0.8f + 0.06f;
        armR = s * 0.8f + 0.06f;
        elbowL = elbowR = 0.22f + amplitude * 0.9f;
    } else {
        legL = 0.65f;
        kneeL = 0.95f;
        legR = -0.10f;
        kneeR = 0.40f;
        armL = 0.55f;
        armR = 0.45f;
        elbowL = elbowR = 0.55f;
    }
    if (aiming) {
        // Rifle al frente sujeto con las dos manos: el antebrazo derecho queda horizontal
        // y la mano izquierda va al guardamanos.
        armR = 0.95f;
        elbowR = 0.62f;
        inR = 0.10f;
        armL = 1.15f;
        elbowL = 0.50f;
        inL = 0.62f;
    }
    // Retroceso: el rifle sube un poco con cada disparo.
    armR += 0.22f * recoil;
    armL += 0.16f * recoil;
    float rate = 14.0f;
    if (swing > 0.03f) {
        // Golpe de pico: el brazo baja desde encima de la cabeza hasta delante del pecho.
        armR = 0.55f + 2.1f * swing;
        elbowR = 0.35f * (1.0f - swing);
        inR = 0.15f;
        rate = 30.0f;
    }
    leftLeg = smoothToward(leftLeg, legL, 14.0f, dt);
    rightLeg = smoothToward(rightLeg, legR, 14.0f, dt);
    leftKnee = smoothToward(leftKnee, kneeL, 14.0f, dt);
    rightKnee = smoothToward(rightKnee, kneeR, 14.0f, dt);
    leftArm = smoothToward(leftArm, armL, 14.0f, dt);
    rightArm = smoothToward(rightArm, armR, rate, dt);
    leftElbow = smoothToward(leftElbow, elbowL, 14.0f, dt);
    rightElbow = smoothToward(rightElbow, elbowR, rate, dt);
    leftArmIn = smoothToward(leftArmIn, inL, 14.0f, dt);
    rightArmIn = smoothToward(rightArmIn, inR, rate, dt);
}

// --- Construccion y dibujo --------------------------------------------------------------

bool CharacterModel::buildLod(const CharacterPalette& palette, int sides, Lod& out) {
    MeshBuilder b(sides, sides >= 6);
    bool ok = true;
    int tris = 0;
    auto finish = [&](PartId id) {
        tris += b.triangles();
        ok &= b.finish(out.parts[id]);
    };
    buildBody(b, palette);
    finish(kBody);
    buildUpperArm(b, palette, -1.0f);
    finish(kUpperArmL);
    buildUpperArm(b, palette, 1.0f);
    finish(kUpperArmR);
    buildForearm(b, palette, -1.0f, HeldItem::Empty);
    finish(kForearmL);
    buildForearm(b, palette, 1.0f, HeldItem::Rifle);
    finish(kForearmRifle);
    const int withRifle = tris;
    buildForearm(b, palette, 1.0f, HeldItem::Pickaxe);
    ok &= b.finish(out.parts[kForearmPickaxe]);
    buildForearm(b, palette, 1.0f, HeldItem::Empty);
    ok &= b.finish(out.parts[kForearmEmpty]);
    tris = withRifle;
    buildThigh(b, palette, -1.0f);
    finish(kThighL);
    buildThigh(b, palette, 1.0f);
    finish(kThighR);
    buildShin(b, palette);
    finish(kShinL);
    buildShin(b, palette);
    finish(kShinR);
    m_triangles[&out == &m_lods[0] ? 0 : 1] = tris;
    return ok;
}

bool CharacterModel::build(const CharacterPalette& palette) {
    bool ok = buildSkinTexture(m_skin, palette);
    for (int i = 0; i < 2; ++i) ok &= buildLod(palette, kLodSides[i], m_lods[i]);
    MeshBuilder shadow(8, false);
    buildShadow(shadow);
    ok &= shadow.finish(m_shadow);
    return ok;
}

void CharacterModel::destroy() {
    for (Lod& lod : m_lods) {
        for (gfx::Mesh& m : lod.parts) gfx::destroyMesh(m);
    }
    gfx::destroyMesh(m_shadow);
    gfx::destroyTexture(m_skin);
}

void CharacterModel::draw(const CharacterState& state, const CharacterAnim& anim,
                          const world::TextureLibrary& /*textures*/, bool eliminated, HeldItem held,
                          bool lowDetail, BodyPose pose) const {
    const Vec3& pos = state.position;
    if (!eliminated && pose == BodyPose::Normal) {
        // Sombra circular bajo los pies: encoge al subir, para leer la altura del salto.
        // Se apoya en la altura real de los pies al tocar suelo (cajas, altillo).
        const float groundY = state.grounded() ? pos.y : world::groundHeightAt(pos.x, pos.z);
        const float shrink = math3d::clampf(1.0f - (pos.y - groundY) / 6.0f, 0.4f, 1.0f);
        gfx::setBlend(true);
        gfx::setModelMatrix(Mat4::translation({pos.x, groundY + 0.04f, pos.z}) * Mat4::scale({shrink, 1.0f, shrink}));
        gfx::bindTexture(nullptr);
        gfx::drawMesh(m_shadow);
        gfx::setBlend(false);
    }

    Mat4 root = Mat4::translation(pos) * Mat4::rotationY(state.yaw);
    if (eliminated) {
        // Cae de espaldas en 0.45 s (acelerando, como con gravedad) y queda tumbado:
        // rotationX(+90) lleva la cabeza (+Y) hacia +Z (detras).
        const float t = anim.deathSeconds > 0.0f ? math3d::clampf(anim.deathSeconds / 0.45f, 0.0f, 1.0f) : 1.0f;
        const float fall = t * t;
        root = root * Mat4::translation({0.0f, 0.15f * fall, 0.0f}) * Mat4::rotationX(math3d::kPi * 0.5f * fall);
    } else if (pose == BodyPose::Skydive) {
        // Caida libre: boca abajo (la cabeza hacia delante), girando sobre la cadera.
        root = root * Mat4::translation({0.0f, kHipY, 0.0f}) * Mat4::rotationX(-1.35f) *
               Mat4::translation({0.0f, -kHipY, 0.0f});
    } else if (pose == BodyPose::Glide) {
        root = root * Mat4::translation({0.0f, kHipY, 0.0f}) * Mat4::rotationX(-0.18f) *
               Mat4::translation({0.0f, -kHipY, 0.0f});
    } else {
        // Respiracion (muy leve) y sacudida hacia atras al recibir un impacto.
        const float breath = 0.012f * sinf(anim.idleTime * 2.4f) * (state.horizontalSpeed() < 0.5f ? 1.0f : 0.0f);
        root = root * Mat4::translation({0.0f, breath, 0.0f}) * Mat4::rotationX(-0.22f * anim.flinch);
    }

    const Lod& lod = m_lods[lowDetail ? 1 : 0];
    gfx::bindTexture(&m_skin);
    auto part = [&](PartId id, const Mat4& m) {
        gfx::setModelMatrix(m);
        gfx::drawMesh(lod.parts[id]);
    };
    part(kBody, root);
    const bool still = eliminated && (anim.deathSeconds <= 0.0f || anim.deathSeconds > 0.45f);
    // Piernas: rotationX(+a) lleva el extremo hacia delante (-Z); la rodilla se dobla hacia atras.
    const bool skydive = pose == BodyPose::Skydive, glide = pose == BodyPose::Glide;
    for (int s = 0; s < 2; ++s) {
        const float side = s == 0 ? -1.0f : 1.0f;
        float hip = still ? 0.0f : (s == 0 ? anim.leftLeg : anim.rightLeg);
        float knee = still ? 0.1f : (s == 0 ? anim.leftKnee : anim.rightKnee);
        float legSpread = 0.0f;
        if (skydive) {
            hip = -0.15f;
            knee = 0.55f;
            legSpread = 0.28f;
        } else if (glide) {
            hip = s == 0 ? 0.25f : 0.05f;
            knee = 0.35f;
        }
        const Mat4 thigh = root * Mat4::translation({side * kHipX, kHipY, 0.0f}) * Mat4::rotationX(hip) *
                           Mat4::rotationZ(side * legSpread);
        part(s == 0 ? kThighL : kThighR, thigh);
        part(s == 0 ? kShinL : kShinR, thigh * Mat4::translation({0.0f, -kKneeDrop, 0.0f}) * Mat4::rotationX(-knee));
    }
    // Brazos: separacion lateral, giro hacia delante, giro hacia el centro y codo.
    for (int s = 0; s < 2; ++s) {
        const float side = s == 0 ? -1.0f : 1.0f;
        float shoulder = still ? (s == 0 ? 0.2f : -0.2f) : (s == 0 ? anim.leftArm : anim.rightArm);
        float elbow = still ? 0.15f : (s == 0 ? anim.leftElbow : anim.rightElbow);
        float inward = still ? 0.0f : (s == 0 ? anim.leftArmIn : anim.rightArmIn);
        float spread = kArmSpread;
        if (skydive) {
            // Brazos abiertos en cruz, algo adelantados.
            shoulder = 0.35f;
            elbow = 0.45f;
            inward = 0.0f;
            spread = 1.25f;
        } else if (glide) {
            // Agarrado a la barra del ala delta, por encima de la cabeza.
            shoulder = 2.85f;
            elbow = 0.25f;
            inward = 0.0f;
            spread = 0.3f;
        }
        const Mat4 upper = root * Mat4::translation({side * kShoulderX, kShoulderY, 0.0f}) *
                           Mat4::rotationY(side * inward) * Mat4::rotationX(shoulder) *
                           Mat4::rotationZ(side * spread);
        part(s == 0 ? kUpperArmL : kUpperArmR, upper);
        PartId fore = kForearmL;
        if (s == 1) fore = held == HeldItem::Pickaxe ? kForearmPickaxe : (held == HeldItem::Empty ? kForearmEmpty : kForearmRifle);
        part(fore, upper * Mat4::translation({0.0f, -kElbowDrop, 0.0f}) * Mat4::rotationX(elbow));
    }
    gfx::setModelMatrix(Mat4::identity());
}

}  // namespace game
