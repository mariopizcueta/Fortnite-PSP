// Limite del mapa: en lugar de una pared invisible, una tormenta permanente rodea la zona
// jugable. Fuera de un cuadrado de 176 m de lado con las esquinas redondeadas hay
// tormenta: niebla morada muy densa y 30 de dano por segundo (ver game::Match).
//
// El suelo sigue mas alla (hasta kWorldExtent) para que nunca se vea donde acaba el
// mundo; con 30 de dano por segundo nadie llega vivo tan lejos (100 de vida = 4 golpes,
// unos 25 m corriendo).
#pragma once

#include <math.h>

namespace world {

constexpr float kStormEdge = 88.0f;     // mitad del lado de la zona segura
constexpr float kStormCorner = 24.0f;   // radio de las esquinas redondeadas
constexpr float kWorldExtent = 144.0f;  // el suelo llega hasta +-kWorldExtent
constexpr float kStormWallHeight = 30.0f;

// Metros dentro de la tormenta (positivo) o distancia hasta ella (negativo).
inline float stormDepth(float x, float z) {
    const float inner = kStormEdge - kStormCorner;
    const float dx = fabsf(x) - inner, dz = fabsf(z) - inner;
    if (dx <= 0.0f && dz <= 0.0f) return -(kStormCorner + fminf(-dx, -dz));
    const float ox = dx > 0.0f ? dx : 0.0f, oz = dz > 0.0f ? dz : 0.0f;
    return sqrtf(ox * ox + oz * oz) - kStormCorner;
}

}  // namespace world
