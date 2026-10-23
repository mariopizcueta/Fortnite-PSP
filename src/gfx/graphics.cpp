#include "gfx/graphics.h"

#include <pspdisplay.h>
#include <pspge.h>
#include <pspgu.h>

#include <stdint.h>

#include "gfx/mesh.h"
#include "gfx/texture.h"

namespace gfx {
namespace {

// Lista de comandos de la GE (256 KB). Debe estar alineada a 16 bytes.
unsigned int __attribute__((aligned(16))) g_displayList[0x10000];

void* g_drawBuffer = nullptr;  // relativo al inicio de la VRAM
bool g_initialized = false;
bool g_vsync = true;

// Ultima textura enlazada en 3D; kUnbound obliga a reenviar el estado.
const Texture* const kUnbound = reinterpret_cast<const Texture*>(1);
const Texture* g_boundTexture = kUnbound;

struct ColorVertex {
    u32 color;
    float x, y, z;
};

struct TexVertex2D {
    float u, v;
    float x, y, z;
};

// Ancho de las tiras al dibujar texturas en 2D: mantiene el acceso dentro de la
// cache de texturas de la GE.
constexpr int kTextureSliceWidth = 64;

void uploadMatrix(int type, const math3d::Mat4& m) {
    const ScePspFMatrix4 psp = m.toPsp();
    sceGuSetMatrix(type, &psp);
}

}  // namespace

bool init() {
    // VRAM (2 MB): 2 framebuffers 32 bpp (2 x 544 KB) + profundidad 16 bpp (272 KB).
    void* fbp0 = guGetStaticVramBuffer(kBufferWidth, kScreenHeight, GU_PSM_8888);
    void* fbp1 = guGetStaticVramBuffer(kBufferWidth, kScreenHeight, GU_PSM_8888);
    void* zbp = guGetStaticVramBuffer(kBufferWidth, kScreenHeight, GU_PSM_4444);
    // OJO: son desplazamientos relativos al inicio de la VRAM, asi que fbp0 vale 0
    // legitimamente. Comprobar "!fbp0" cerraba la aplicacion nada mas arrancar.

    sceGuInit();
    sceGuStart(GU_DIRECT, g_displayList);

    sceGuDrawBuffer(GU_PSM_8888, fbp0, kBufferWidth);
    sceGuDispBuffer(kScreenWidth, kScreenHeight, fbp1, kBufferWidth);
    sceGuDepthBuffer(zbp, kBufferWidth);

    sceGuOffset(2048 - (kScreenWidth / 2), 2048 - (kScreenHeight / 2));
    sceGuViewport(2048, 2048, kScreenWidth, kScreenHeight);
    // Profundidad invertida: cerca = 65535, lejos = 0, test GU_GEQUAL y limpieza a 0.
    sceGuDepthRange(65535, 0);
    sceGuDepthFunc(GU_GEQUAL);
    sceGuScissor(0, 0, kScreenWidth, kScreenHeight);
    sceGuEnable(GU_SCISSOR_TEST);

    sceGuFrontFace(GU_CW);
    sceGuShadeModel(GU_SMOOTH);
    sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);

    sceGuClearColor(rgba(0, 0, 0));
    sceGuClearDepth(0);
    sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);

    sceGuFinish();
    sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE);

    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);

    g_drawBuffer = fbp0;
    g_initialized = true;
    return true;
}

void shutdown() {
    if (!g_initialized) return;
    sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE);
    sceGuDisplay(GU_FALSE);
    sceGuTerm();
    g_initialized = false;
}

void beginFrame(u32 clearColor) {
    sceGuStart(GU_DIRECT, g_displayList);
    sceGuClearColor(clearColor);
    sceGuClearDepth(0);
    sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);
    begin2D();
}

void endFrame() {
    sceGuFinish();
    sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE);
}

void present(bool vsync) {
    if (vsync != g_vsync) {
        guSwapBuffersBehaviour(vsync ? PSP_DISPLAY_SETBUF_NEXTFRAME : PSP_DISPLAY_SETBUF_IMMEDIATE);
        g_vsync = vsync;
    }
    if (vsync) sceDisplayWaitVblankStart();
    g_drawBuffer = sceGuSwapBuffers();
}

void begin3D(const math3d::Mat4& projection, const math3d::Mat4& view, const FogSettings& fog) {
    uploadMatrix(GU_PROJECTION, projection);
    uploadMatrix(GU_VIEW, view);
    uploadMatrix(GU_MODEL, math3d::Mat4::identity());

    sceGuDisable(GU_BLEND);  // la escena es opaca: ahorra lecturas del framebuffer
    sceGuEnable(GU_DEPTH_TEST);
    sceGuDepthMask(GU_FALSE);  // GU_FALSE = escritura de profundidad activada
    // Recorte contra los planos cercano/lejano: sin el, los triangulos que cruzan
    // el plano cercano se deforman o desaparecen al acercar la camara.
    sceGuEnable(GU_CLIP_PLANES);
    // Sin culling de caras traseras en esta fase: toda la geometria es visible
    // por ambos lados y la profundidad resuelve el orden.
    sceGuDisable(GU_CULL_FACE);

    sceGuTexMapMode(GU_TEXTURE_COORDS, 0, 0);
    sceGuTexScale(1.0f, 1.0f);
    sceGuTexOffset(0.0f, 0.0f);

    if (fog.enabled) {
        sceGuFog(fog.start, fog.end, fog.color & 0x00FFFFFFu);
        sceGuEnable(GU_FOG);
    } else {
        sceGuDisable(GU_FOG);
    }

    g_boundTexture = kUnbound;
    bindTexture(nullptr);
}

void setModelMatrix(const math3d::Mat4& model) { uploadMatrix(GU_MODEL, model); }

void bindTexture(const Texture* texture) {
    if (texture == g_boundTexture) return;
    g_boundTexture = texture;
    if (!texture || !texture->pixels) {
        sceGuDisable(GU_TEXTURE_2D);
        return;
    }
    sceGuEnable(GU_TEXTURE_2D);
    sceGuTexMode(texture->psm, 0, 0, 0);
    sceGuTexImage(0, texture->texWidth, texture->texHeight, texture->stride, texture->pixels);
    // Textura * color de vertice (sombreado horneado), sin filtrado bilineal.
    sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGB);
    sceGuTexFilter(GU_NEAREST, GU_NEAREST);
    sceGuTexWrap(GU_REPEAT, GU_REPEAT);
    sceGuTexFlush();
}

void drawMesh(const Mesh& mesh) {
    if (mesh.indexCount <= 0) return;
    sceGuDrawArray(GU_TRIANGLES, kMeshVertexFormat, mesh.indexCount, mesh.indices, mesh.vertices);
}

void drawLines(const LineVertex* vertices, int count) {
    if (count < 2) return;
    bindTexture(nullptr);
    auto* v = static_cast<LineVertex*>(sceGuGetMemory(count * sizeof(LineVertex)));
    for (int i = 0; i < count; ++i) v[i] = vertices[i];
    sceGuDrawArray(GU_LINES, GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D, count, nullptr, v);
}

void setBlend(bool enabled) {
    if (enabled) sceGuEnable(GU_BLEND);
    else sceGuDisable(GU_BLEND);
}

void begin2D() {
    sceGuDisable(GU_DEPTH_TEST);
    sceGuDisable(GU_FOG);
    sceGuDisable(GU_TEXTURE_2D);
    sceGuEnable(GU_BLEND);
    g_boundTexture = kUnbound;
}

u32* cpuDrawBuffer() {
    // 0x40000000 selecciona el acceso no cacheado: lo escrito llega directo a VRAM.
    const uintptr_t base = reinterpret_cast<uintptr_t>(sceGeEdramGetAddr());
    const uintptr_t addr = (base + reinterpret_cast<uintptr_t>(g_drawBuffer)) | 0x40000000u;
    return reinterpret_cast<u32*>(addr);
}

void fillRect(float x, float y, float w, float h, u32 color) {
    auto* v = static_cast<ColorVertex*>(sceGuGetMemory(2 * sizeof(ColorVertex)));
    v[0] = {color, x, y, 0.0f};
    v[1] = {color, x + w, y + h, 0.0f};
    sceGuDrawArray(GU_SPRITES, GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D, 2, nullptr, v);
}

void drawTexture(const Texture& texture, float x, float y) {
    if (!texture.pixels) return;

    sceGuEnable(GU_TEXTURE_2D);
    sceGuTexMode(texture.psm, 0, 0, 0);
    sceGuTexImage(0, texture.texWidth, texture.texHeight, texture.stride, texture.pixels);
    sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGBA);
    // 1:1 con muestreo puntual: nunca se leen texeles fuera de la imagen real.
    sceGuTexFilter(GU_NEAREST, GU_NEAREST);
    sceGuTexWrap(GU_CLAMP, GU_CLAMP);
    sceGuTexFlush();

    const float h = static_cast<float>(texture.height);
    for (int sx = 0; sx < texture.width; sx += kTextureSliceWidth) {
        const int sw = (texture.width - sx) < kTextureSliceWidth ? (texture.width - sx)
                                                                 : kTextureSliceWidth;
        const float u0 = static_cast<float>(sx);
        const float u1 = static_cast<float>(sx + sw);
        auto* v = static_cast<TexVertex2D*>(sceGuGetMemory(2 * sizeof(TexVertex2D)));
        v[0] = {u0, 0.0f, x + u0, y, 0.0f};
        v[1] = {u1, h, x + u1, y + h, 0.0f};
        sceGuDrawArray(GU_SPRITES, GU_TEXTURE_32BITF | GU_VERTEX_32BITF | GU_TRANSFORM_2D, 2,
                       nullptr, v);
    }

    sceGuDisable(GU_TEXTURE_2D);
    g_boundTexture = kUnbound;
}

}  // namespace gfx
