#include "world/textures.h"

#include "assets/embedded_assets.h"
#include "third_party/stb_image_config.h"

#include <math.h>

namespace world {
namespace {

constexpr int kSize = 32;       // texturas procedurales (si falta el atlas)
constexpr int kAtlasTile = 64;  // texturas del atlas de Recursos/Texturas

u32 hash(u32 x, u32 y, u32 seed) {
    u32 h = x * 374761393u + y * 668265263u + seed * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

// Ruido blanco en [0, 1).
float white(int x, int y, u32 seed) {
    return static_cast<float>(hash(static_cast<u32>(x), static_cast<u32>(y), seed) & 0xFFFF) /
           65536.0f;
}

// Ruido de valor suave y repetible (tileable) con `cells` celdas por lado, en [0, 1).
float blotch(int x, int y, int cells, u32 seed) {
    const float fx = static_cast<float>(x) * cells / kSize;
    const float fy = static_cast<float>(y) * cells / kSize;
    const int x0 = static_cast<int>(fx), y0 = static_cast<int>(fy);
    float tx = fx - x0, ty = fy - y0;
    tx = tx * tx * (3.0f - 2.0f * tx);
    ty = ty * ty * (3.0f - 2.0f * ty);
    auto at = [&](int cx, int cy) { return white(cx % cells, cy % cells, seed); };
    const float a = at(x0, y0), b = at(x0 + 1, y0), c = at(x0, y0 + 1), d = at(x0 + 1, y0 + 1);
    return (a + (b - a) * tx) + ((c + (d - c) * tx) - (a + (b - a) * tx)) * ty;
}

struct Rgb {
    float r, g, b;
};

Rgb scaled(Rgb c, float s) { return {c.r * s, c.g * s, c.b * s}; }
Rgb offset(Rgb c, float o) { return {c.r + o, c.g + o, c.b + o}; }

u32 clampByte(float v) { return v <= 0.0f ? 0u : (v >= 255.0f ? 255u : static_cast<u32>(v)); }

template <typename Fn>
bool generate(gfx::Texture& tex, Fn&& pixel) {
    if (!gfx::createTexture(tex, kSize, kSize, GU_PSM_5551)) return false;
    auto* dst = static_cast<u16*>(tex.pixels);
    for (int y = 0; y < kSize; ++y) {
        for (int x = 0; x < kSize; ++x) {
            const Rgb c = pixel(x, y);
            dst[y * tex.stride + x] = gfx::pack5551(clampByte(c.r), clampByte(c.g), clampByte(c.b));
        }
    }
    gfx::uploadTexture(tex);
    return true;
}

Rgb grass(int x, int y) {
    const Rgb base{72, 122, 50};
    float s = 0.82f + 0.32f * blotch(x, y, 4, 11) + 0.10f * (white(x, y, 12) - 0.5f);
    const float speck = white(x, y, 13);
    if (speck < 0.07f) s *= 0.72f;          // briznas oscuras
    else if (speck > 0.96f) s *= 1.18f;     // briznas claras
    return scaled(base, s);
}

Rgb dirt(int x, int y) {
    const Rgb base{138, 106, 72};
    float s = 0.85f + 0.25f * blotch(x, y, 4, 21) + 0.10f * (white(x, y, 22) - 0.5f);
    const float pebble = white(x, y, 23);
    if (pebble < 0.05f) return offset(scaled(base, s), 34.0f);
    if (pebble > 0.95f) s *= 0.7f;
    return scaled(base, s);
}

Rgb concrete(int x, int y) {
    const Rgb base{152, 150, 144};
    float s = 0.90f + 0.14f * blotch(x, y, 4, 31) + 0.07f * (white(x, y, 32) - 0.5f);
    if (x == 0 || y == 0) s *= 0.68f;  // junta entre losas
    if (white(x, y, 33) < 0.03f) s *= 0.8f;
    return scaled(base, s);
}

// Chapa ondulada: nervios verticales cada 8 texeles. Neutra para tintar.
Rgb metalSiding(int x, int y) {
    static const float kProfile[8] = {0.70f, 0.86f, 1.0f, 1.0f, 0.90f, 0.78f, 0.64f, 0.62f};
    float s = kProfile[x % 8] + 0.06f * (white(x, y, 41) - 0.5f);
    // Suciedad que escurre desde abajo.
    if (y > 24) s *= 1.0f - 0.25f * (static_cast<float>(y - 24) / 8.0f) * blotch(x, 0, 8, 42);
    return scaled({235, 235, 235}, s);
}

// Panel de cubierta: nervios anchos y algo de oxido.
Rgb roofPanel(int x, int y) {
    float s = (x % 16) < 3 ? 1.0f : 0.80f;
    s += 0.08f * (white(x, y, 51) - 0.5f);
    Rgb c = scaled({220, 220, 215}, s);
    const float rust = blotch(x, y, 4, 52);
    if (rust > 0.72f) c = {c.r * 0.95f, c.g * 0.70f, c.b * 0.55f};
    return c;
}

// Lateral de contenedor: nervios estrechos y marco oscuro.
Rgb container(int x, int y) {
    if (x < 2 || x > 29 || y < 2 || y > 29) return {120, 120, 120};
    float s = (x % 4) < 2 ? 1.0f : 0.78f;
    s += 0.06f * (white(x, y, 61) - 0.5f);
    return scaled({230, 230, 230}, s);
}

// Caja de madera: tablones horizontales, marco y riostra diagonal.
Rgb wood(int x, int y) {
    const Rgb base{156, 112, 64};
    const bool frame = x < 3 || x > 28 || y < 3 || y > 28;
    const int d = x - y;
    const bool brace = d >= -2 && d <= 2;
    float s = 0.92f + 0.12f * (white(x / 4, y, 71) - 0.5f) + 0.06f * (white(x, y, 72) - 0.5f);
    if (frame || brace) s *= 1.12f;
    else if (y % 8 == 0) s *= 0.55f;  // separacion entre tablones
    if ((frame || brace) && (x == 3 || x == 28 || y == 3 || y == 28 || d == -2 || d == 2)) {
        s *= 0.7f;
    }
    return scaled(base, s);
}

// Damero de prueba: deja ver la perspectiva y el muestreo sin filtrado.
Rgb checker(int x, int y) {
    const bool on = ((x / 8) + (y / 8)) % 2 == 0;
    if (x % 16 == 0 || y % 16 == 0) return {255, 255, 255};
    return on ? Rgb{228, 176, 55} : Rgb{44, 44, 56};
}

// Persiana enrollable: lamas horizontales.
Rgb shutter(int x, int y) {
    float s = (y % 4) == 0 ? 0.62f : ((y % 4) == 1 ? 1.0f : 0.88f);
    s += 0.05f * (white(x, y, 81) - 0.5f);
    if (y > 29) s *= 0.5f;
    return scaled({210, 210, 205}, s);
}

// Fachada de oficina: un hueco de ventana por cada 4 x 4 m.
Rgb officeWindow(int x, int y) {
    const Rgb wall{210, 200, 182};
    const bool inFrame = x >= 5 && x <= 26 && y >= 7 && y <= 22;
    if (!inFrame) {
        float s = 0.92f + 0.10f * (white(x, y, 91) - 0.5f);
        if (y >= 30) s *= 0.75f;  // zocalo / forjado
        return scaled(wall, s);
    }
    if (x == 5 || x == 26 || y == 7 || y == 22 || x == 15 || x == 16) return {70, 72, 78};
    const bool glint = (x - y) > 2 && (x - y) < 6;
    return glint ? Rgb{150, 185, 215} : Rgb{48, 78, 112};
}

// Camuflaje desertico: manchas grandes de dos marrones sobre tostado.
Rgb camo(int x, int y) {
    const float a = blotch(x, y, 4, 101);
    const float b = blotch(x + 7, y + 3, 6, 102);
    Rgb c{196, 150, 98};
    if (a > 0.62f) c = {150, 104, 62};
    else if (b > 0.66f) c = {214, 176, 122};
    return scaled(c, 0.95f + 0.08f * (white(x, y, 103) - 0.5f));
}

// Agua: azul con ondas claras horizontales.
Rgb water(int x, int y) {
    const float wave = sinf((x * 0.45f + y * 1.3f) + 3.0f * blotch(x, y, 4, 111));
    float s = 0.9f + 0.1f * blotch(x, y, 4, 112);
    if (wave > 0.82f) s *= 1.25f;
    return scaled({52, 104, 150}, s);
}

// Roca de la meseta: estratos horizontales y grietas.
Rgb rock(int x, int y) {
    const float band = 0.88f + 0.14f * blotch(0, y, 8, 121);
    float s = band * (0.9f + 0.18f * blotch(x, y, 4, 122)) + 0.06f * (white(x, y, 123) - 0.5f);
    if (white(x / 2, y, 124) < 0.04f) s *= 0.65f;
    return scaled({150, 128, 108}, s);
}

Rgb sand(int x, int y) {
    const float s = 0.92f + 0.12f * blotch(x, y, 4, 131) + 0.08f * (white(x, y, 132) - 0.5f);
    return scaled({214, 196, 150}, s);
}

// Bloques de hormigon de 2 x 1 m (la textura cubre 4 x 2 m) con junta oscura.
Rgb blockWall(int x, int y) {
    const int row = y / 16;
    const int offset = (row % 2) * 8;
    const bool joint = (y % 16) == 0 || ((x + offset) % 16) == 0;
    float s = 0.92f + 0.10f * blotch(x, y, 4, 141) + 0.05f * (white(x, y, 142) - 0.5f);
    if (joint) s *= 0.72f;
    if (y >= 30) s *= 0.85f;  // albardilla
    return scaled({190, 184, 168}, s);
}

// Pared de madera construida: tablones horizontales de 8 texeles con vetas, juntas
// oscuras, clavos y un listón vertical en cada borde (la textura cubre 2 x 2 m).
Rgb woodWall(int x, int y) {
    const Rgb base{150, 98, 52};
    const int plank = y / 8;
    float s = 0.88f + 0.10f * white(plank, 0, 151) + 0.10f * (white(x / 3, y, 152) - 0.5f);
    if (y % 8 == 0) s *= 0.55f;                          // junta entre tablones
    if (x < 3 || x > 28) s = 0.78f + 0.06f * white(x, y, 153);  // listones verticales
    if ((x == 1 || x == 30) && y % 8 == 4) return {60, 56, 50};   // clavos
    return scaled(base, s);
}

// v0.14 (modo zombi), por si falta el atlas: versiones oxidadas y apagadas.
Rgb deadGrass(int x, int y) {
    const float s = 0.82f + 0.30f * blotch(x, y, 4, 211) + 0.10f * (white(x, y, 212) - 0.5f);
    return scaled({112, 104, 62}, s);
}

Rgb rust(int x, int y, u32 seed) {
    const float s = 0.78f + 0.30f * blotch(x, y, 4, seed) + 0.12f * (white(x, y, seed + 1) - 0.5f);
    return blotch(x, y, 6, seed + 2) > 0.55f ? scaled({150, 78, 42}, s) : scaled({150, 140, 128}, s);
}

Rgb murkyWater(int x, int y) {
    const float s = 0.85f + 0.15f * blotch(x, y, 4, 241);
    return scaled({58, 74, 52}, s);
}

Rgb rottenWood(int x, int y) {
    const float s = 0.80f + 0.20f * white(x / 3, y, 251) + 0.10f * blotch(x, y, 4, 252);
    return scaled({104, 82, 58}, (y % 8 == 0) ? s * 0.6f : s);
}

Rgb canvas(int x, int y) {
    const float s = 0.88f + 0.08f * white(x, y, 261) + ((x + y) % 4 == 0 ? -0.06f : 0.0f);
    return scaled({200, 196, 180}, s);
}

}  // namespace

bool TextureLibrary::buildFromAtlas() {
    const assets::Blob png = assets::texturesPng();
    int w = 0, h = 0, channels = 0;
    unsigned char* rgba = stbi_load_from_memory(static_cast<const stbi_uc*>(png.data),
                                                static_cast<int>(png.size), &w, &h, &channels, 4);
    if (!rgba) return false;
    const int tilesPerRow = w / kAtlasTile;
    bool ok = w == 4 * kAtlasTile && h == (kTexCount + 3) / 4 * kAtlasTile;
    for (int id = 0; ok && id < kTexCount; ++id) {
        gfx::Texture& tex = m_textures[id];
        ok = gfx::createTexture(tex, kAtlasTile, kAtlasTile, GU_PSM_5551);
        if (!ok) break;
        const int ox = (id % tilesPerRow) * kAtlasTile, oy = (id / tilesPerRow) * kAtlasTile;
        auto* dst = static_cast<u16*>(tex.pixels);
        for (int y = 0; y < kAtlasTile; ++y) {
            for (int x = 0; x < kAtlasTile; ++x) {
                const unsigned char* p = rgba + ((oy + y) * w + ox + x) * 4;
                dst[y * tex.stride + x] = gfx::pack5551(p[0], p[1], p[2]);
            }
        }
        gfx::uploadTexture(tex);
    }
    stbi_image_free(rgba);
    if (!ok) destroy();
    return ok;
}

bool TextureLibrary::build() {
    m_fromResources = buildFromAtlas();
    if (m_fromResources) return true;
    bool ok = true;
    ok &= generate(m_textures[kTexGrass], grass);
    ok &= generate(m_textures[kTexDirt], dirt);
    ok &= generate(m_textures[kTexConcrete], concrete);
    ok &= generate(m_textures[kTexMetalSiding], metalSiding);
    ok &= generate(m_textures[kTexRoofPanel], roofPanel);
    ok &= generate(m_textures[kTexContainer], container);
    ok &= generate(m_textures[kTexWood], wood);
    ok &= generate(m_textures[kTexChecker], checker);
    ok &= generate(m_textures[kTexShutter], shutter);
    ok &= generate(m_textures[kTexOfficeWindow], officeWindow);
    ok &= generate(m_textures[kTexCamo], camo);
    ok &= generate(m_textures[kTexWater], water);
    ok &= generate(m_textures[kTexRock], rock);
    ok &= generate(m_textures[kTexSand], sand);
    ok &= generate(m_textures[kTexBlockWall], blockWall);
    ok &= generate(m_textures[kTexWoodWall], woodWall);
    ok &= generate(m_textures[kTexDeadGrass], deadGrass);
    ok &= generate(m_textures[kTexRustSiding], [](int x, int y) { return rust(x, y, 221); });
    ok &= generate(m_textures[kTexRustRoof], [](int x, int y) { return rust(x, y, 225); });
    ok &= generate(m_textures[kTexRustContainer], [](int x, int y) { return rust(x, y, 229); });
    ok &= generate(m_textures[kTexMurkyWater], murkyWater);
    ok &= generate(m_textures[kTexCrackedConcrete], [](int x, int y) { return rust(x, y, 233); });
    ok &= generate(m_textures[kTexRottenWood], rottenWood);
    ok &= generate(m_textures[kTexCanvas], canvas);
    return ok;
}

void TextureLibrary::destroy() {
    for (gfx::Texture& t : m_textures) gfx::destroyTexture(t);
}

const gfx::Texture* TextureLibrary::get(int id) const {
    if (id < 0 || id >= kTexCount) return nullptr;
    if (m_theme == TextureTheme::Ruined) {
        switch (id) {
            case kTexGrass: id = kTexDeadGrass; break;
            case kTexMetalSiding: id = kTexRustSiding; break;
            case kTexRoofPanel:
            case kTexShutter: id = kTexRustRoof; break;
            case kTexContainer: id = kTexRustContainer; break;
            case kTexWater: id = kTexMurkyWater; break;
            case kTexConcrete: id = kTexCrackedConcrete; break;
            case kTexWood: id = kTexRottenWood; break;
            default: break;
        }
    }
    return m_textures[id].pixels ? &m_textures[id] : nullptr;
}

size_t TextureLibrary::totalBytes() const {
    size_t total = 0;
    for (const gfx::Texture& t : m_textures) total += gfx::textureBytes(t);
    return total;
}

}  // namespace world
