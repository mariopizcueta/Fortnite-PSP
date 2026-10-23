// Texto de depuracion 8x8 escrito por CPU sobre el framebuffer (fuente de pspdebug).
#pragma once

#include <psptypes.h>

namespace gfx {
namespace text {

constexpr int kGlyphSize = 8;

void init();

// Fija el framebuffer de destino (ver gfx::cpuDrawBuffer()).
void setTarget(u32* framebuffer);

// Coordenadas en pixeles. Color en formato GU (0xAABBGGRR; el alfa se ignora).
void print(int x, int y, u32 color, const char* fmt, ...) __attribute__((format(printf, 4, 5)));
// Igual que print() con sombra negra desplazada 1 px, para texto sobre imagenes.
void printShadow(int x, int y, u32 color, const char* fmt, ...)
    __attribute__((format(printf, 4, 5)));

int width(const char* str);

}  // namespace text
}  // namespace gfx
