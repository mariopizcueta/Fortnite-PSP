// Objetos del mapa que se pueden picar (y destruir) con el pico.
//
// Todo lo que hay en el mapa salvo el suelo y la meseta: paredes de las naves (en paneles
// de 4 m), tejados, altillos, escaleras, vallas, muros, farolas, contenedores, cajas,
// arboles y rocas. Se registran al construir el escenario (world_layout.cpp) con sus
// cajas de colision fijas (consecutivas) y sus lotes de dibujo: el raycast devuelve el
// indice de una caja (RayHit::collider) y con el se sabe que objeto se ha golpeado.
// Cada golpe da material y quita vida; sin vida, el objeto desaparece (sus cajas dejan de
// colisionar y sus lotes no se dibujan). La vida la lleva la partida (todo vuelve a estar
// en pie en la siguiente).
#pragma once

#include <psptypes.h>

#include "world/collision.h"

namespace world {

enum class Resource : u8 { Wood, Stone, Metal, Count };

const char* resourceName(Resource resource);

struct Harvestable {
    int collider = -1;      // primera caja fija del objeto
    int colliderCount = 1;  // sus cajas son [collider, collider + colliderCount)
    int batchBegin = 0;     // sus lotes de dibujo (World) son [batchBegin, batchEnd)
    int batchEnd = 0;
    Resource resource = Resource::Wood;
    Aabb box;               // engloba todas sus cajas
    int perHit = 0;         // material por golpe de pico
    float health = 0.0f;    // vida inicial (cada golpe quita MatchConfig::pickaxeObjectDamage)
    // Modo zombi (v0.14): probabilidad (0-100) de que el objeto ya este derruido al empezar
    // (tejados, paneles, vallas...), y objetos que solo existen en ese modo (la base, los
    // campamentos de supervivientes y los centros de crafteo; en el resto, no estan).
    u8 ruinChance = 0;
    bool zombieOnly = false;

    bool owns(int c) const { return c >= collider && c < collider + colliderCount; }
};

}  // namespace world
