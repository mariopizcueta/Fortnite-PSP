// Inicializacion de PSP GU, doble buffer, estado 2D/3D y primitivas basicas.
#pragma once

#include <psptypes.h>

#include "math/math3d.h"

namespace gfx {

constexpr int kScreenWidth = 480;
constexpr int kScreenHeight = 272;
constexpr int kBufferWidth = 512;  // ancho de linea del framebuffer (potencia de 2)

// Framebuffer de 16 bits (RGB 565) con tramado de la GE: el aspecto de color de PS1 y la
// mitad de memoria y ancho de banda que con 32 bits. Para volver a 32 bits basta con
// poner false (la portada y el HUD funcionan igual en los dos modos).
constexpr bool kRetro16BitFramebuffer = true;

struct Texture;
struct Mesh;

// Color GU (0xAABBGGRR) a partir de componentes RGBA.
constexpr u32 rgba(u32 r, u32 g, u32 b, u32 a = 255) {
    return (a << 24) | (b << 16) | (g << 8) | r;
}

struct FogSettings {
    bool enabled = true;
    float start = 0.0f;  // distancia (m, espacio de vista) donde empieza la niebla
    float end = 0.0f;    // distancia donde el color es 100 % niebla
    u32 color = 0;       // debe coincidir con el color de limpieza
};

// Reserva framebuffers en VRAM, configura GU y enciende la pantalla.
bool init();
void shutdown();

// Abre la lista de comandos, limpia color + profundidad y deja el estado 2D.
void beginFrame(u32 clearColor);
// Cierra la lista y espera a que la GPU termine de dibujar.
void endFrame();
// Intercambia buffers. Con vsync=false el cambio es inmediato (puede haber tearing).
void present(bool vsync);

// --- 3D -----------------------------------------------------------------------
// Sube proyeccion y vista, activa test de profundidad, recorte y niebla.
void begin3D(const math3d::Mat4& projection, const math3d::Mat4& view, const FogSettings& fog);
void setModelMatrix(const math3d::Mat4& model);
// Textura para las siguientes mallas (muestreo GU_NEAREST, repeticion). nullptr = color plano.
void bindTexture(const Texture* texture);
void drawMesh(const Mesh& mesh);
// Mezcla alfa con el alfa del color de vertice (p. ej. sombras). Por defecto en 3D: no.
void setBlend(bool enabled);
// Niebla de begin3D encendida o apagada para lo siguiente (el cielo se dibuja sin ella).
void setFog(bool enabled);
// Escritura de profundidad (el cielo no escribe: no tapa nada de lo que se dibuja despues).
void setDepthWrite(bool enabled);

// Lineas 3D de color (trazadoras de disparos). Se copian a la lista de comandos del frame.
struct LineVertex {
    u32 color;
    float x, y, z;
};
void drawLines(const LineVertex* vertices, int count);

// --- 2D -----------------------------------------------------------------------
// Vuelve al estado 2D: sin profundidad ni niebla, con mezcla alfa.
void begin2D();
void fillRect(float x, float y, float w, float h, u32 color);
// Dibuja la textura completa a escala 1:1 con la esquina superior izquierda en (x, y).
void drawTexture(const Texture& texture, float x, float y);
// Dibuja el rectangulo (sx, sy, sw, sh) de la textura en (dx, dy, dw, dh), escalado sin
// filtrar y multiplicado por `color` (tinte y transparencia).
void drawTextureRect(const Texture& texture, float sx, float sy, float sw, float sh, float dx,
                     float dy, float dw, float dh, u32 color = 0xFFFFFFFF);
// Lineas 2D de color en coordenadas de pantalla.
void drawLines2D(const LineVertex* vertices, int count);

// Direccion (no cacheada) del buffer que se esta dibujando, para escritura por CPU.
// Solo es valida entre endFrame() y present().
u32* cpuDrawBuffer();

}  // namespace gfx
