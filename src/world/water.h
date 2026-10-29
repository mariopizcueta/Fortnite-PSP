// Agua del lago (v0.14): se puede cruzar andando, mas despacio.
//
// El fondo esta en el suelo (Y = 0) y la superficie se dibuja kWaterLevel por encima: los
// pies quedan bajo el agua. No hay colision (antes el lago bloqueaba el paso).
#pragma once

namespace world {

// Lago: elipse de centro (kLakeX, kLakeZ) y semiejes kLakeRx, kLakeRz (lo usan el mapa y
// la fisica).
constexpr float kLakeX = -30.0f, kLakeZ = 56.0f, kLakeRx = 20.0f, kLakeRz = 12.0f;
constexpr float kWaterLevel = 0.35f;
// Fraccion de la velocidad normal dentro del agua (y del impulso del salto).
constexpr float kWaterSpeed = 0.5f;
constexpr float kWaterJump = 0.8f;

// < 1 dentro de la elipse del lago.
inline float lakeValue(float x, float z) {
    const float dx = (x - kLakeX) / kLakeRx, dz = (z - kLakeZ) / kLakeRz;
    return dx * dx + dz * dz;
}

// Dentro del lago en (x, z), por losas de suelo (como se dibuja el agua).
bool inWater(float x, float z);

}  // namespace world
