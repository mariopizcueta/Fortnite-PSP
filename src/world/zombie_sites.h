// Lugares del modo zombi (v0.14, docs/ZOMBIS.md): la base abandonada donde despierta el
// jugador, los campamentos de los supervivientes, los centros de crafteo y los planos.
// v0.15: lugares de interes (casas en ruinas, la casona, el almacen del polvorin, puestos
// de avanzada y la gasolinera) con sus neveras, despensas, cofres y cajas de municion.
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

// --- Lugares de interes (v0.15) -----------------------------------------------------------
// Cuanto mas grande el lugar, mas botin y comida, pero mas zombis lo guardan (`guards`:
// aparecen al acercarse el jugador, una vez por dia; algunos, tumbados entre las ruinas).
enum class SiteKind : unsigned char { House, BigHouse, Storehouse, Outpost, GasStation };

struct Site {
    SiteKind kind;
    float x, z;     // centro
    float radius;   // m: dentro se esta "en" el lugar (descubrirlo, objetivos)
    int guards;     // zombis que lo guardan
    int nameIndex;  // texto del nombre (i18n::T::Site0 + nameIndex)
};

constexpr int kSiteCount = 10;
constexpr Site kSites[kSiteCount] = {
    {SiteKind::BigHouse, 46.0f, 53.0f, 12.0f, 6, 0},     // La Casona
    {SiteKind::Storehouse, 8.0f, -66.0f, 14.0f, 7, 1},   // Almacen del Polvorin
    {SiteKind::House, 16.0f, 33.0f, 7.0f, 2, 2},         // Casa del guarda
    {SiteKind::House, -16.0f, 78.0f, 7.0f, 2, 3},        // Casa del lago
    {SiteKind::House, 72.0f, 16.0f, 7.0f, 3, 4},         // Granja abandonada
    {SiteKind::House, -56.0f, 24.0f, 7.0f, 2, 5},        // Cabana del bosque
    {SiteKind::Outpost, 34.0f, -80.0f, 9.0f, 3, 6},      // Puesto norte
    {SiteKind::Outpost, 78.0f, 56.0f, 9.0f, 3, 7},       // Puesto este
    {SiteKind::Outpost, -66.0f, -56.0f, 9.0f, 3, 8},     // Puesto del bosque
    {SiteKind::GasStation, 52.0f, -14.0f, 10.0f, 3, 9},  // Gasolinera
};

// Contenedores de los lugares (la partida los coloca; el escenario deja su hueco).
// Nevera: sobre todo comida. Despensa (caja de viveres): comida y vendas.
enum class SpotKind : unsigned char { Fridge, Pantry, Chest, AmmoBox };
struct ContainerSpot {
    SpotKind kind;
    float x, z;
    float yaw;  // hacia donde mira el frente
};

// Orientaciones (como game::forwardFromYaw: 0 mira al norte, -Z).
constexpr float kYawS = 3.14159265f, kYawN = 0.0f, kYawE = -1.5707963f, kYawW = 1.5707963f;
constexpr ContainerSpot kSiteContainers[] = {
    // La Casona (x 39..53, z 47..59): dos neveras, despensa, dos cofres y municion.
    {SpotKind::Fridge, 40.0f, 48.4f, kYawS}, {SpotKind::Fridge, 41.4f, 48.4f, kYawS},
    {SpotKind::Pantry, 44.0f, 48.2f, kYawS}, {SpotKind::Chest, 51.0f, 57.6f, kYawN},
    {SpotKind::Chest, 49.5f, 49.0f, kYawS}, {SpotKind::AmmoBox, 51.8f, 52.0f, kYawW},
    // Almacen del Polvorin (x -2..18, z -74..-58): despensas, municion, cofres y una nevera.
    {SpotKind::Pantry, -0.6f, -70.0f, kYawE}, {SpotKind::Pantry, -0.6f, -67.5f, kYawE},
    {SpotKind::Pantry, 16.6f, -70.0f, kYawW}, {SpotKind::AmmoBox, 4.0f, -72.6f, kYawS},
    {SpotKind::AmmoBox, 12.0f, -72.6f, kYawS}, {SpotKind::Chest, 8.0f, -72.4f, kYawS},
    {SpotKind::Chest, 15.5f, -61.0f, kYawN}, {SpotKind::Fridge, 1.0f, -59.4f, kYawN},
    // Casas: una nevera cada una, y algo mas segun la casa.
    {SpotKind::Fridge, 12.8f, 30.0f, kYawE}, {SpotKind::Chest, 19.0f, 36.0f, kYawN},
    {SpotKind::Fridge, -19.2f, 75.0f, kYawE}, {SpotKind::AmmoBox, -13.0f, 81.0f, kYawN},
    {SpotKind::Fridge, 68.8f, 13.0f, kYawE}, {SpotKind::Pantry, 75.0f, 13.2f, kYawS},
    {SpotKind::Fridge, -59.2f, 21.0f, kYawE}, {SpotKind::Pantry, -53.0f, 27.0f, kYawN},
    // Puestos de avanzada: municion, despensa y un cofre.
    {SpotKind::AmmoBox, 31.5f, -78.0f, kYawE}, {SpotKind::Pantry, 36.5f, -82.0f, kYawW},
    {SpotKind::AmmoBox, 75.5f, 58.0f, kYawE}, {SpotKind::Chest, 80.5f, 54.0f, kYawW},
    {SpotKind::AmmoBox, -68.5f, -54.0f, kYawE}, {SpotKind::Pantry, -63.5f, -58.0f, kYawW},
    // Gasolinera (tienda x 48..56, z -20..-15): nevera de bebidas, despensa y municion.
    {SpotKind::Fridge, 49.0f, -19.2f, kYawS}, {SpotKind::Pantry, 55.0f, -19.0f, kYawS},
    {SpotKind::AmmoBox, 45.0f, -10.0f, kYawE},
};
constexpr int kSiteContainerCount = sizeof(kSiteContainers) / sizeof(kSiteContainers[0]);

// Lugar en el que esta (x, z), o -1.
inline int siteAt(float x, float z, float margin = 0.0f) {
    for (int i = 0; i < kSiteCount; ++i) {
        const float dx = x - kSites[i].x, dz = z - kSites[i].z, r = kSites[i].radius + margin;
        if (dx * dx + dz * dz < r * r) return i;
    }
    return -1;
}

}  // namespace zsites
}  // namespace world
