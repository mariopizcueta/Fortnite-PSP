// Geometria de colision: cajas alineadas a los ejes (AABB), separadas de la geometria visual.
//
// El escenario registra por un lado mallas (SceneBuilder, solo para dibujar) y por otro
// cajas de colision (CollisionWorld, solo para fisica, IA y disparos). Una pared suele
// tener ambas cosas; un arbol tiene copa visual sin colision y tronco con colision; el
// agua tiene colision de movimiento pero no bloquea disparos; el limite del mapa es
// colision invisible.
//
// Las cajas se indexan en una rejilla uniforme de kCellSize metros en XZ para que las
// consultas solo miren las cajas cercanas.
//
// Ademas de las cajas fijas del mapa hay cajas DINAMICAS (las paredes que construye el
// jugador): se anaden y quitan durante la partida y todas las consultas (fisica, IA,
// linea de vision, camara y disparos) las tienen en cuenta igual que a las fijas.
#pragma once

#include <vector>

#include "math/math3d.h"

namespace world {

struct Aabb {
    math3d::Vec3 min;
    math3d::Vec3 max;

    bool overlaps(const Aabb& o) const {
        return min.x < o.max.x && max.x > o.min.x && min.y < o.max.y && max.y > o.min.y &&
               min.z < o.max.z && max.z > o.min.z;
    }
};

// Que bloquea cada caja.
enum ColliderFlags : u8 {
    kBlocksMovement = 1 << 0,  // personajes (jugador y bots)
    kBlocksShots = 1 << 1,     // disparos y linea de vision
    kSolid = kBlocksMovement | kBlocksShots,
};

struct Collider {
    Aabb box;
    u8 flags = kSolid;
};

struct RayHit {
    float t = 1.0f;          // fraccion del segmento [0, 1]
    math3d::Vec3 point;
    int collider = -1;       // caja fija del mapa, o -1
    int dynamicHandle = -1;  // caja dinamica (pared construida), o -1
};

// Interseccion segmento p + t*d (t en [0, 1]) con una caja. Devuelve la t de entrada.
bool segmentVsAabb(const math3d::Vec3& p, const math3d::Vec3& d, const Aabb& box, float* tEnter);

class CollisionWorld {
public:
    static constexpr float kCellSize = 8.0f;

    void clear();
    void add(const Aabb& box, u8 flags);
    // Construye la rejilla. Llamar despues de anadir todas las cajas.
    void build(float halfExtent);

    int count() const { return static_cast<int>(m_colliders.size()); }
    const Collider& collider(int i) const { return m_colliders[i]; }

    // Cajas dinamicas. addDynamic devuelve un identificador (o -1 si no hay hueco).
    // Una rampa son varias cajas (escalones): hace falta sitio para unas 20 de ellas.
    static constexpr int kMaxDynamic = 224;
    int addDynamic(const Aabb& box, u8 flags);
    void removeDynamic(int handle);
    void clearDynamic();
    int dynamicCount() const;

    // Llama fn(const Collider&) para cada caja con (flags & mask) que solapa `box`.
    // `includeDynamic = false`: solo las cajas fijas del mapa.
    template <typename Fn>
    void query(const Aabb& box, u8 mask, Fn&& fn, bool includeDynamic = true) const;
    bool overlapsAny(const Aabb& box, u8 mask) const;
    bool overlapsAnyStatic(const Aabb& box, u8 mask) const;

    // Primer impacto del segmento [from, to] contra cajas con (flags & mask).
    bool raycast(const math3d::Vec3& from, const math3d::Vec3& to, u8 mask, RayHit* hit) const;

    // Altura sobre la que puede apoyarse un personaje: la mas alta entre el suelo (0) y
    // los techos de cajas bajo la huella [x +- r, z +- r] que no superan maxTop.
    float supportHeight(float x, float z, float r, float maxTop) const;
    // Base mas baja de las cajas sobre la huella cuya base esta por encima de minBottom
    // (techos). Devuelve un valor enorme si no hay ninguna.
    float ceilingHeight(float x, float z, float r, float minBottom) const;

private:
    void cellRange(float minX, float minZ, float maxX, float maxZ, int* x0, int* z0, int* x1,
                   int* z1) const;
    u32 nextStamp() const;

    std::vector<Collider> m_colliders;
    // Rejilla en formato CSR: los indices de la celda c estan en
    // m_cellItems[m_cellStart[c] .. m_cellStart[c + 1]).
    std::vector<int> m_cellStart;
    std::vector<u16> m_cellItems;
    int m_cells = 0;
    float m_origin = 0.0f;
    // Marca de consulta para no visitar dos veces una caja que ocupa varias celdas.
    mutable std::vector<u32> m_stamp;
    mutable u32 m_stampCounter = 0;

    struct DynamicSlot {
        Collider collider;
        bool active = false;
    };
    DynamicSlot m_dynamic[kMaxDynamic];
    int m_dynamicEnd = 0;  // las consultas solo recorren [0, m_dynamicEnd)
};

template <typename Fn>
void CollisionWorld::query(const Aabb& box, u8 mask, Fn&& fn, bool includeDynamic) const {
    if (includeDynamic) {
        for (int i = 0; i < m_dynamicEnd; ++i) {
            const DynamicSlot& d = m_dynamic[i];
            if (d.active && (d.collider.flags & mask) && d.collider.box.overlaps(box)) fn(d.collider);
        }
    }
    if (m_cells == 0) return;
    int x0, z0, x1, z1;
    cellRange(box.min.x, box.min.z, box.max.x, box.max.z, &x0, &z0, &x1, &z1);
    const u32 stamp = nextStamp();
    for (int cz = z0; cz <= z1; ++cz) {
        for (int cx = x0; cx <= x1; ++cx) {
            const int cell = cz * m_cells + cx;
            for (int k = m_cellStart[cell]; k < m_cellStart[cell + 1]; ++k) {
                const int idx = m_cellItems[k];
                if (m_stamp[idx] == stamp) continue;
                m_stamp[idx] = stamp;
                const Collider& c = m_colliders[idx];
                if ((c.flags & mask) && c.box.overlaps(box)) fn(c);
            }
        }
    }
}

}  // namespace world
