// Construccion: paredes de madera que el jugador coloca delante.
//
// Cada pared es una caja alineada con los ejes (4 x 3 x 0.25 m por defecto) ajustada a
// una rejilla de 4 m, como en Fortnite. La MISMA caja es:
//   - la colision dinamica en CollisionWorld (fisica, IA, linea de vision, disparos),
//   - la geometria que se dibuja (escalada a esa caja),
// asi que lo que se ve es exactamente lo que bloquea.
//
// Al colocar o quitar una pared se recalcula la navegacion de la zona y los bots
// replanifican sus caminos (revision()).
#pragma once

#include "game/nav_grid.h"
#include "world/collision.h"

namespace game {

struct BuildConfig {
    int maxWalls = 20;           // limite de paredes en pie a la vez
    float wallHealth = 150.0f;   // vida de una pared de madera (los disparos la danan)
    float gridSize = 4.0f;       // rejilla de construccion (m)
    float wallHeight = 3.0f;
    float wallThickness = 0.25f;
    float minDistance = 1.0f;    // distancia minima delante de los pies
    float minHeight = 2.2f;      // bajo un dintel la pared se recorta, pero no por debajo de esto
    float removeRange = 7.0f;    // m: alcance para quitar una pared apuntandola
    int wallCost = 10;           // madera por pared (como en Fortnite)
};

constexpr int kMaxWallCapacity = 64;  // limite tecnico (cajas dinamicas disponibles)

enum class PlacementError {
    None,
    LimitReached,
    OverlapsPlayer,
    OverlapsBot,
    OverlapsStructure,  // choca con el mapa o con otra pared
    Blocked,            // hay un solido entre el jugador y la pared (no se construye a traves)
    OutOfBounds,
    NoMaterials,        // no hay madera suficiente (la comprueba la partida)
};

const char* placementErrorName(PlacementError error);

struct WallPlacement {
    bool valid = false;
    PlacementError error = PlacementError::None;
    world::Aabb box;
    bool alongX = true;  // la pared se extiende a lo largo de X (si no, de Z)
};

struct BuiltWall {
    bool active = false;
    world::Aabb box;
    bool alongX = true;
    int collider = -1;  // identificador de la caja dinamica
    float health = 0.0f;
    float maxHealth = 0.0f;
    float sinceHit = 99.0f;  // para el parpadeo al recibir un disparo
};

class StructureSystem {
public:
    // Quita todas las paredes (restaurando colision y navegacion) y aplica la configuracion.
    void reset(world::CollisionWorld& world, NavGrid& nav, const BuildConfig& config);

    // Calcula donde iria la pared delante de `feet` mirando a `yaw` y si es valida.
    // `player` y `bots` son las cajas de los cuerpos que no puede atravesar.
    WallPlacement plan(const math3d::Vec3& feet, float yaw, const world::Aabb& player,
                       const world::Aabb* bots, int botCount) const;

    int place(const WallPlacement& placement);  // indice de la pared o -1
    bool remove(int index);
    // Resta vida; devuelve true si la pared se ha destruido.
    bool damage(int index, float amount);
    void update(float dt);

    // Pared a la que pertenece una caja dinamica (o -1).
    int wallFromCollider(int dynamicHandle) const;
    // Primera pared construida en el segmento (para quitarla apuntando), o -1.
    int wallAlong(const math3d::Vec3& from, const math3d::Vec3& to) const;

    int count() const { return m_count; }
    const BuildConfig& config() const { return m_config; }
    const BuiltWall& wall(int i) const { return m_walls[i]; }
    int capacity() const { return kMaxWallCapacity; }
    // Cambia cada vez que se coloca o desaparece una pared.
    unsigned revision() const { return m_revision; }

private:
    void changed(const world::Aabb& box);
    bool blockedFrom(const math3d::Vec3& chest, const world::Aabb& box) const;
    bool duplicates(const WallPlacement& placement) const;

    BuildConfig m_config;
    world::CollisionWorld* m_world = nullptr;
    NavGrid* m_nav = nullptr;
    BuiltWall m_walls[kMaxWallCapacity];
    int m_count = 0;
    unsigned m_revision = 0;
};

}  // namespace game
