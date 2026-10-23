// Objetos del mapa que se pueden picar con el pico para conseguir materiales.
//
// Se registran al construir el escenario (world_layout.cpp) junto a su caja de colision
// fija: el raycast devuelve el indice de esa caja (RayHit::collider) y con el se sabe que
// objeto se ha golpeado. Los arboles y las cajas dan madera, las rocas piedra y los
// contenedores metal. Cuanto queda de cada uno lo lleva la partida (se repone en cada una).
#pragma once

#include <psptypes.h>

#include "world/collision.h"

namespace world {

enum class Resource : u8 { Wood, Stone, Metal, Count };

const char* resourceName(Resource resource);

struct Harvestable {
    int collider = -1;  // indice de la caja fija del mapa
    Resource resource = Resource::Wood;
    Aabb box;
    int perHit = 0;     // material por golpe de pico
    int total = 0;      // material total antes de agotarse
};

}  // namespace world
