#include "soft_gfx.h"

#include <math.h>

#include <pspdebug.h>

#include "gfx/mesh.h"
#include "gfx/texture.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-field-initializers"
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#pragma clang diagnostic pop

using math3d::Mat4;
using math3d::Vec3;

namespace softgfx {

Raster g;

namespace {

constexpr float kGuardBand = 2048.0f;
const unsigned char* g_font = nullptr;
u32 g_cpuBufferDummy[4];

struct ClipVertex {
    float x, y, z, w;  // espacio de recorte
    float u, v;
    float r, gg, b, a;
    float viewDepth;  // distancia positiva delante de la camara
};

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

// Texel (x, y) en RGBA 0..255.
void texel(const gfx::Texture* t, int tx, int ty, float out[4]) {
    tx %= t->texWidth;
    ty %= t->texHeight;
    if (tx < 0) tx += t->texWidth;
    if (ty < 0) ty += t->texHeight;
    if (t->psm == GU_PSM_5551) {
        const u16 p = static_cast<const u16*>(t->pixels)[ty * t->stride + tx];
        out[0] = static_cast<float>((p & 31) << 3);
        out[1] = static_cast<float>(((p >> 5) & 31) << 3);
        out[2] = static_cast<float>(((p >> 10) & 31) << 3);
        out[3] = (p & 0x8000) ? 255.0f : 0.0f;
    } else {
        const u32 p = static_cast<const u32*>(t->pixels)[ty * t->stride + tx];
        out[0] = static_cast<float>(p & 0xFF);
        out[1] = static_cast<float>((p >> 8) & 0xFF);
        out[2] = static_cast<float>((p >> 16) & 0xFF);
        out[3] = static_cast<float>(p >> 24);
    }
}

void sampleTexture(float u, float v, float out[3]) {
    const gfx::Texture* t = g.texture;
    if (!t) {
        out[0] = out[1] = out[2] = 255.0f;
        return;
    }
    float c[4];
    texel(t, static_cast<int>(floorf(u * t->texWidth)), static_cast<int>(floorf(v * t->texHeight)), c);
    out[0] = c[0];
    out[1] = c[1];
    out[2] = c[2];
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
            if (g.depthWrite) g.depth[idx] = z;  // la GE escribe profundidad tambien con mezcla (sceGuDepthMask)
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

// Mezcla un pixel 2D (rgb 0..255, alfa 0..1).
inline void blendPixel(int x, int y, float r, float gg, float b, float a) {
    if (x < 0 || y < 0 || x >= kW || y >= kH || a <= 0.0f) return;
    float* p = &g.rgb[(y * kW + x) * 3];
    p[0] = r * a + p[0] * (1.0f - a);
    p[1] = gg * a + p[1] * (1.0f - a);
    p[2] = b * a + p[2] * (1.0f - a);
}

void line2D(float x0, float y0, float x1, float y1, u32 color) {
    const float r = static_cast<float>(color & 0xFF), gg = static_cast<float>((color >> 8) & 0xFF),
                b = static_cast<float>((color >> 16) & 0xFF), a = static_cast<float>(color >> 24) / 255.0f;
    const float dx = x1 - x0, dy = y1 - y0;
    const int steps = static_cast<int>(fmaxf(fabsf(dx), fabsf(dy))) + 1;
    for (int i = 0; i <= steps; ++i) {
        const float t = static_cast<float>(i) / steps;
        blendPixel(static_cast<int>(floorf(x0 + dx * t)), static_cast<int>(floorf(y0 + dy * t)), r, gg, b, a);
    }
}

}  // namespace

void mul4(const Mat4& m, const float in[4], float out[4]) {
    for (int r = 0; r < 4; ++r) {
        out[r] = m.m[r] * in[0] + m.m[4 + r] * in[1] + m.m[8 + r] * in[2] + m.m[12 + r] * in[3];
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

void savePng(const char* path, bool quantize565) {
    std::vector<unsigned char> out(kW * kH * 3);
    for (int i = 0; i < kW * kH * 3; ++i) {
        const float c = g.rgb[i];
        int v = static_cast<int>(c < 0 ? 0 : (c > 255 ? 255 : c));
        if (quantize565) v = (i % 3 == 1) ? (v & 0xFC) : (v & 0xF8);
        out[i] = static_cast<unsigned char>(v);
    }
    stbi_write_png(path, kW, kH, 3, out.data(), kW * 3);
}

void setFont(const unsigned char* msx2048) { g_font = msx2048; }

}  // namespace softgfx

using softgfx::g;

// --- Sustitutos de host de gfx:: ----------------------------------------------------------
namespace gfx {

bool init() { return true; }
void shutdown() {}
void beginFrame(u32 clearColor) {
    if (g.enabled) softgfx::clear(clearColor);
    g.blend = false;
}
void endFrame() {}
void present(bool) {}
u32* cpuDrawBuffer() { return softgfx::g_cpuBufferDummy; }

void begin3D(const Mat4& projection, const Mat4& view, const FogSettings& fog) {
    g.proj = projection;
    g.view = view;
    g.model = Mat4::identity();
    g.fog = fog;
    g.fogConfigured = fog.enabled;
    g.depthWrite = true;
    g.texture = nullptr;
    g.blend = false;
}
void setModelMatrix(const Mat4& model) { g.model = model; }
void bindTexture(const Texture* texture) { g.texture = texture; }
void setBlend(bool enabled) { g.blend = enabled; }
void setFog(bool enabled) { g.fog.enabled = enabled && g.fogConfigured; }
void setDepthWrite(bool enabled) { g.depthWrite = enabled; }

void drawMesh(const Mesh& mesh) {
    if (!g.enabled) return;
    for (int i = 0; i + 2 < mesh.indexCount; i += 3) {
        softgfx::ClipVertex tri[3];
        bool bad = false;
        for (int k = 0; k < 3; ++k) {
            const int idx = mesh.indices[i + k];
            if (idx >= mesh.vertexCount) bad = true;
            else tri[k] = softgfx::toClip(mesh.vertices[idx]);
        }
        if (bad) {
            ++g.badIndices;
            continue;
        }
        ++g.trianglesIn;

        // Recorte Sutherland-Hodgman contra el plano cercano (z >= -w), como GU_CLIP_PLANES.
        softgfx::ClipVertex poly[4];
        int n = 0;
        bool clipped = false;
        for (int k = 0; k < 3; ++k) {
            const softgfx::ClipVertex& a = tri[k];
            const softgfx::ClipVertex& b = tri[(k + 1) % 3];
            const float da = a.z + a.w, db = b.z + b.w;
            if (da >= 0) poly[n++] = a;
            if ((da >= 0) != (db >= 0)) {
                poly[n++] = softgfx::lerpClip(a, b, da / (da - db));
                clipped = true;
            }
            if (da < 0) clipped = true;
        }
        if (clipped) ++g.nearClipped;
        for (int k = 1; k + 1 < n; ++k) {
            const softgfx::ClipVertex t[3] = {poly[0], poly[k], poly[k + 1]};
            softgfx::rasterTriangle(t);
        }
    }
}

void drawLines(const LineVertex* vertices, int count) {
    // Trazadoras: se proyectan los extremos y se dibuja la linea sin test de profundidad.
    if (!g.enabled) return;
    const Mat4 vp = g.proj * g.view;
    for (int i = 0; i + 1 < count; i += 2) {
        float s[2][2];
        bool ok = true;
        for (int k = 0; k < 2; ++k) {
            const LineVertex& v = vertices[i + k];
            const float in[4] = {v.x, v.y, v.z, 1.0f};
            float c[4];
            softgfx::mul4(vp, in, c);
            if (c[3] < 0.2f) ok = false;
            s[k][0] = softgfx::kW * 0.5f + c[0] / c[3] * softgfx::kW * 0.5f;
            s[k][1] = softgfx::kH * 0.5f - c[1] / c[3] * softgfx::kH * 0.5f;
        }
        if (ok) softgfx::line2D(s[0][0], s[0][1], s[1][0], s[1][1], vertices[i].color);
    }
}

void begin2D() { g.blend = true; }

void fillRect(float x, float y, float w, float h, u32 color) {
    if (!g.enabled) return;
    const float r = static_cast<float>(color & 0xFF), gg = static_cast<float>((color >> 8) & 0xFF),
                b = static_cast<float>((color >> 16) & 0xFF), a = static_cast<float>(color >> 24) / 255.0f;
    const int x0 = static_cast<int>(floorf(x + 0.5f)), x1 = static_cast<int>(floorf(x + w + 0.5f));
    const int y0 = static_cast<int>(floorf(y + 0.5f)), y1 = static_cast<int>(floorf(y + h + 0.5f));
    for (int py = y0; py < y1; ++py) {
        for (int px = x0; px < x1; ++px) softgfx::blendPixel(px, py, r, gg, b, a);
    }
}

void drawTextureRect(const Texture& texture, float sx, float sy, float sw, float sh, float dx, float dy,
                     float dw, float dh, u32 color) {
    if (!g.enabled || !texture.pixels || dw <= 0.0f || dh <= 0.0f) return;
    const float cr = static_cast<float>(color & 0xFF) / 255.0f, cg = static_cast<float>((color >> 8) & 0xFF) / 255.0f,
                cb = static_cast<float>((color >> 16) & 0xFF) / 255.0f, ca = static_cast<float>(color >> 24) / 255.0f;
    const int x0 = static_cast<int>(floorf(dx + 0.5f)), x1 = static_cast<int>(floorf(dx + dw + 0.5f));
    const int y0 = static_cast<int>(floorf(dy + 0.5f)), y1 = static_cast<int>(floorf(dy + dh + 0.5f));
    for (int py = y0; py < y1; ++py) {
        const float v = sy + (py + 0.5f - dy) / dh * sh;
        for (int px = x0; px < x1; ++px) {
            const float u = sx + (px + 0.5f - dx) / dw * sw;
            float t[4];
            softgfx::texel(&texture, static_cast<int>(floorf(u)), static_cast<int>(floorf(v)), t);
            softgfx::blendPixel(px, py, t[0] * cr, t[1] * cg, t[2] * cb, t[3] / 255.0f * ca);
        }
    }
}

void drawTexture(const Texture& texture, float x, float y) {
    drawTextureRect(texture, 0, 0, static_cast<float>(texture.width), static_cast<float>(texture.height), x, y,
                    static_cast<float>(texture.width), static_cast<float>(texture.height));
}

void drawLines2D(const LineVertex* vertices, int count) {
    if (!g.enabled) return;
    for (int i = 0; i + 1 < count; i += 2) {
        softgfx::line2D(vertices[i].x, vertices[i].y, vertices[i + 1].x, vertices[i + 1].y, vertices[i].color);
    }
}

}  // namespace gfx

// --- Sustitutos de pspDebugScreen ---------------------------------------------------------

void pspDebugScreenInitEx(void*, int, int) {}
void pspDebugScreenEnableBackColor(int) {}
void pspDebugScreenSetBase(u32*) {}

void pspDebugScreenPutChar(int x, int y, u32 color, u8 ch) {
    if (!g.enabled) return;
    const float r = static_cast<float>(color & 0xFF), gg = static_cast<float>((color >> 8) & 0xFF),
                b = static_cast<float>((color >> 16) & 0xFF);
    for (int row = 0; row < 8; ++row) {
        // Misma fuente y orden de bits que libpspdebug: bit 7 = columna izquierda.
        const unsigned bits = softgfx::g_font ? softgfx::g_font[ch * 8 + row] : (row > 0 && row < 7 ? 0x7C : 0);
        for (int col = 0; col < 8; ++col) {
            if (bits & (0x80u >> col)) softgfx::blendPixel(x + col, y + row, r, gg, b, 1.0f);
        }
    }
}
