#include "gfx/texture.h"

#include <malloc.h>
#include <string.h>

#include <pspkernel.h>

#include "third_party/stb_image_config.h"

namespace gfx {
namespace {

constexpr int kMaxTextureSize = 512;

int nextPow2(int v) {
    int p = 1;
    while (p < v) p <<= 1;
    return p;
}

int bytesPerPixel(int psm) { return psm == GU_PSM_8888 ? 4 : 2; }

}  // namespace

size_t textureBytes(const Texture& texture) {
    // Solo se reservan las filas reales: nunca se muestrea por debajo de `height`.
    return static_cast<size_t>(texture.stride) * texture.height * bytesPerPixel(texture.psm);
}

bool createTexture(Texture& out, int width, int height, int psm) {
    if (width <= 0 || height <= 0 || width > kMaxTextureSize || height > kMaxTextureSize) {
        return false;
    }
    Texture tex;
    tex.width = width;
    tex.height = height;
    tex.texWidth = nextPow2(width);
    tex.texHeight = nextPow2(height);
    // La GE exige filas multiplo de 16 bytes: 8 texeles en formatos de 16 bits.
    tex.stride = tex.texWidth < 8 ? 8 : tex.texWidth;
    tex.psm = psm;

    const size_t bytes = textureBytes(tex);
    tex.pixels = memalign(16, bytes);
    if (!tex.pixels) return false;
    memset(tex.pixels, 0, bytes);
    out = tex;
    return true;
}

void uploadTexture(const Texture& texture) {
    // La GE lee de RAM sin pasar por la cache de la CPU.
    if (texture.pixels) sceKernelDcacheWritebackRange(texture.pixels, textureBytes(texture));
}

bool loadPngFromMemory(Texture& out, const void* data, size_t size, int psm) {
    int w = 0, h = 0, channels = 0;
    unsigned char* decoded = stbi_load_from_memory(static_cast<const stbi_uc*>(data),
                                                   static_cast<int>(size), &w, &h, &channels, 4);
    if (!decoded) return false;

    Texture tex;
    if (!createTexture(tex, w, h, psm == GU_PSM_5551 ? GU_PSM_5551 : GU_PSM_8888)) {
        stbi_image_free(decoded);
        return false;
    }
    if (tex.psm == GU_PSM_5551) {
        auto* dst = static_cast<u16*>(tex.pixels);
        for (int y = 0; y < h; ++y) {
            const unsigned char* src = decoded + y * w * 4;
            for (int x = 0; x < w; ++x, src += 4) {
                dst[y * tex.stride + x] = pack5551(src[0], src[1], src[2], src[3] >= 128 ? 1 : 0);
            }
        }
    } else {
        auto* dst = static_cast<u32*>(tex.pixels);
        for (int y = 0; y < h; ++y) {
            memcpy(dst + y * tex.stride, decoded + y * w * 4, static_cast<size_t>(w) * 4);
        }
    }
    stbi_image_free(decoded);

    uploadTexture(tex);
    out = tex;
    return true;
}

void destroyTexture(Texture& texture) {
    free(texture.pixels);
    texture = Texture{};
}

}  // namespace gfx
