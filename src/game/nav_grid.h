// Rejilla de navegacion para los bots, generada automaticamente desde la colision.
//
// Un nodo cada kCellSize metros a nivel del suelo. Un nodo es transitable si cabe un
// personaje (con margen) sin tocar cajas que bloqueen el movimiento. Dos nodos vecinos
// (8 direcciones) estan conectados si el recorrido recto entre ellos esta libre. Los
// caminos se buscan con A* sobre esta rejilla.
//
// Las puertas de las naves y del recinto estan centradas en nodos de la rejilla
// (x, z = -94 + 4k) para que siempre haya un paso.
#pragma once

#include <vector>

#include "core/random.h"
#include "math/math3d.h"
#include "world/collision.h"

namespace game {

class NavGrid {
public:
    static constexpr float kCellSize = 4.0f;
    static constexpr float kAgentRadius = 0.45f;  // un poco mas que el cuerpo: margen

    // Genera nodos y conexiones (al cargar la partida).
    void build(const world::CollisionWorld& world, float halfExtent);
    // Recalcula nodos y conexiones alrededor de `changed` (una pared construida o
    // destruida). Barato: solo unas decenas de nodos.
    void updateRegion(const world::CollisionWorld& world, const world::Aabb& changed);

    int size() const { return m_size; }
    int walkableCount() const { return m_walkableCount; }
    bool walkable(int ix, int iz) const;
    math3d::Vec3 center(int ix, int iz) const;
    // Nodo transitable mas cercano a `p` (busca en anillos de hasta `maxRing` celdas).
    bool nearestNode(const math3d::Vec3& p, int maxRing, int* ix, int* iz) const;
    // Punto transitable al azar dentro de un circulo.
    bool randomPointNear(const math3d::Vec3& center, float radius, core::Random& rng,
                         math3d::Vec3* out) const;

    // A*: escribe en `out` los centros de los nodos (sin el de partida) hasta `maxPoints`.
    // Devuelve el numero de puntos, 0 si no hay camino.
    int findPath(const math3d::Vec3& from, const math3d::Vec3& to, math3d::Vec3* out,
                 int maxPoints, int* expansions = nullptr);

    // Recorrido recto libre para un personaje (muestras cada 0.5 m).
    static bool clearWalk(const world::CollisionWorld& world, const math3d::Vec3& a,
                          const math3d::Vec3& b, float radius = kAgentRadius);

private:
    int index(int ix, int iz) const { return iz * m_size + ix; }
    bool computeWalkable(const world::CollisionWorld& world, int ix, int iz) const;
    u8 computeLinks(const world::CollisionWorld& world, int ix, int iz) const;
    bool cellOf(const math3d::Vec3& p, int* ix, int* iz) const;

    int m_size = 0;
    float m_origin = 0.0f;
    int m_walkableCount = 0;
    std::vector<u8> m_walkable;
    std::vector<u8> m_links;  // bit d = conectado con el vecino d (8 direcciones)

    // Buffers de A* reutilizados (sin reservar memoria por busqueda).
    std::vector<float> m_g;
    std::vector<int> m_parent;
    std::vector<u32> m_visitStamp;
    std::vector<u8> m_closed;
    std::vector<int> m_heap;
    u32 m_stamp = 0;
};

}  // namespace game
