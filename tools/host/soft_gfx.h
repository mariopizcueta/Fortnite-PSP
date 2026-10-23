// Rasterizador por software de host (Mac) que sustituye a src/gfx/graphics.cpp en las
// herramientas de prueba. Imita a la GE: z-buffer, texturas con muestreo GU_NEAREST +
// GU_REPEAT, color de vertice con GU_TFX_MODULATE, niebla lineal, recorte contra el plano
// cercano, descarte fuera de la banda de guarda de +-2048 px, sprites 2D con mezcla alfa y
// el texto de pspDebugScreen con su misma fuente 8x8. Es una aproximacion para revisar
// la imagen sin hardware: no sustituye a probar en PPSSPP o en una PSP.
#pragma once

#include <vector>

#include "gfx/graphics.h"
#include "math/math3d.h"

namespace softgfx {

constexpr int kW = gfx::kScreenWidth;
constexpr int kH = gfx::kScreenHeight;

struct Raster {
    std::vector<float> rgb = std::vector<float>(kW * kH * 3);
    std::vector<float> depth = std::vector<float>(kW * kH);
    math3d::Mat4 proj = math3d::Mat4::identity(), view = math3d::Mat4::identity(),
                 model = math3d::Mat4::identity();
    gfx::FogSettings fog;
    const gfx::Texture* texture = nullptr;
    bool blend = false;
    bool enabled = true;  // false: las llamadas de dibujo no hacen nada (simulacion rapida)

    int trianglesIn = 0;
    int nearClipped = 0;
    int guardBandCulled = 0;
    int badIndices = 0;
};
extern Raster g;

void clear(u32 color);
// Guarda la imagen; con `quantize565` reduce el color a 16 bits como el framebuffer.
void savePng(const char* path, bool quantize565 = false);
// out = m * in (columnas como en math3d::Mat4).
void mul4(const math3d::Mat4& m, const float in[4], float out[4]);

// Fuente 8x8 de pspDebugScreen (la genera el script de compilacion desde libpspdebug.a).
// Si no se ha cargado, los caracteres se dibujan como bloques.
void setFont(const unsigned char* msx2048);

}  // namespace softgfx
