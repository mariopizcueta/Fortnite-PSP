// Texturas en RAM principal, listas para la GE.
#pragma once

#include <stddef.h>

#include <pspgu.h>
#include <psptypes.h>

namespace gfx {

struct Texture {
    int width = 0;      // tamano real de la imagen
    int height = 0;
    int texWidth = 0;   // tamano declarado a la GE (potencias de 2, max. 512)
    int texHeight = 0;
    int stride = 0;     // texeles por fila en memoria (== texWidth)
    int psm = GU_PSM_8888;
    void* pixels = nullptr;
};

// Reserva una textura vacia (alineada a 16 bytes). Formatos: GU_PSM_8888 o GU_PSM_5551.
bool createTexture(Texture& out, int width, int height, int psm);
// Vuelca la cache de datos de la CPU para que la GE vea el contenido. Llamar tras escribir.
void uploadTexture(const Texture& texture);

// Decodifica un PNG en memoria a RGBA8888 o, con psm = GU_PSM_5551, a 16 bits (la mitad
// de memoria; para imagenes opacas a pantalla completa). Devuelve false si falla o
// supera 512x512.
bool loadPngFromMemory(Texture& out, const void* data, size_t size, int psm = GU_PSM_8888);
void destroyTexture(Texture& texture);

size_t textureBytes(const Texture& texture);

// Empaqueta un color en el formato 5551 de la GE (A1 B5 G5 R5, 15 bits como PS1).
constexpr u16 pack5551(u32 r, u32 g, u32 b, u32 a = 1) {
    return static_cast<u16>((a << 15) | ((b >> 3) << 10) | ((g >> 3) << 5) | (r >> 3));
}

}  // namespace gfx
