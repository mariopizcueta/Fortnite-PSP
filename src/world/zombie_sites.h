// Lugares del modo zombi (v0.14, docs/ZOMBIS.md): la base abandonada donde despierta el
// jugador, los campamentos de los supervivientes, los centros de crafteo y los planos.
// Los usan el escenario (world_layout.cpp: objetos que solo existen en ese modo) y la
// partida (game/match_zombies.cpp).
#pragma once

#include "math/math3d.h"

namespace world {
namespace zsites {

// Base: 3 x 3 celdas de la rejilla de construccion (4 m), de x 56..68 y z 28..40. Los
// planos fortifican su perimetro (lineas x = 56, 68 y z = 28, 40).
constexpr float kBaseMinX = 56.0f, kBaseMaxX = 68.0f;
constexpr float kBaseMinZ = 28.0f, kBaseMaxZ = 40.0f;
constexpr math3d::Vec3 kBaseCenter{62.0f, 0.0f, 34.0f};
// Donde despierta el jugador (dentro de la base, junto al catre) mirando a la puerta.
constexpr math3d::Vec3 kWakeSpot{58.4f, 0.0f, 31.6f};
constexpr float kWakeYaw = 3.14159265f;  // mirando al sur (+Z): la puerta

constexpr int kCampCount = 5;
constexpr math3d::Vec3 kCamps[kCampCount] = {
    {-60.0f, 0.0f, -14.0f}, {8.0f, 0.0f, 72.0f}, {-26.0f, 0.0f, -70.0f}, {82.0f, 0.0f, -34.0f}, {-74.0f, 0.0f, 72.0f}};

constexpr int kWorkbenchCount = 3;
constexpr math3d::Vec3 kWorkbenches[kWorkbenchCount] = {{3.0f, 0.0f, -15.0f}, {-60.0f, 0.0f, 46.0f}, {38.0f, 0.0f, -42.0f}};

constexpr int kBlueprintSpots = 4;
constexpr math3d::Vec3 kBlueprints[kBlueprintSpots] = {
    {-20.0f, 0.0f, -14.0f}, {12.0f, 0.0f, 70.0f}, {-22.0f, 0.0f, -68.0f}, {80.0f, 0.0f, -30.0f}};

inline bool insideBase(float x, float z, float margin = 0.0f) {
    return x > kBaseMinX - margin && x < kBaseMaxX + margin && z > kBaseMinZ - margin && z < kBaseMaxZ + margin;
}

}  // namespace zsites
}  // namespace world
