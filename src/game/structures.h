// Construccion: paredes, suelos y rampas de madera que el jugador coloca delante.
//
// Cada pared es una caja alineada con los ejes (4 x 3 x 0.25 m por defecto) ajustada a
// una rejilla de 4 m, como en Fortnite. Un suelo es una losa de 4 x 4 m en la celda de
// delante y una rampa sube 3 m a lo largo de esa celda; como la colision solo tiene
// cajas, la rampa son kRampSteps escalones (mas bajos que kStepHeight, asi que se sube
// andando) y se dibuja como una losa inclinada que pasa por el centro de cada escalon.
// Las cajas de cada pieza son:
//   - la colision dinamica en CollisionWorld (fisica, IA, linea de vision, disparos),
//   - la geometria que se dibuja (escalada a esa caja),
// asi que lo que se ve es exactamente lo que bloquea.
//
// Al colocar o quitar una pared se recalcula la navegacion de la zona y los bots
// replanifican sus caminos (revision()).
#pragma once

#include "game/nav_grid.h"
#include "world/collision.h"
#include "world/harvest.h"

namespace game {

struct BuildConfig {
    int maxWalls = 20;           // limite de paredes en pie a la vez
    // Vida de cada pieza segun su material (como en Fortnite): madera, piedra y metal.
    float wallHealth = 150.0f;
    float stoneHealth = 300.0f;
    float metalHealth = 500.0f;
    float gridSize = 4.0f;       // rejilla de construccion (m)
    float wallHeight = 3.0f;
    float wallThickness = 0.25f;
    float minDistance = 1.0f;    // distancia minima delante de los pies
    float minHeight = 2.2f;      // bajo un dintel la pared se recorta, pero no por debajo de esto
    float removeRange = 7.0f;    // m: alcance para quitar una pared apuntandola
    int wallCost = 10;           // madera por pieza (pared, suelo o rampa, como en Fortnite)
    float floorThickness = 0.2f;
    float rampThickness = 1.0f;  // grosor vertical de la losa de la rampa (debajo se puede pasar)
    int maxBotWalls = 40;        // piezas de bots en pie a la vez (entre todos)
    int maxWallsPerBot = 6;      // v0.14: una caja de cuatro paredes y una rampa
};

// Limite tecnico de piezas en pie (v0.15: 128; el modo zombi no tiene otro limite).
constexpr int kMaxWallCapacity = 128;
constexpr int kRampSteps = 8;         // escalones de 0.375 m (kStepHeight = 0.45)
constexpr int kMaxPieceBoxes = kRampSteps;

enum class PieceType : u8 { Wall, Floor, Ramp };
const char* pieceName(PieceType type);

enum class PlacementError {
    None,
    LimitReached,
    OverlapsPlayer,
    OverlapsBot,
    OverlapsStructure,  // choca con el mapa o con otra pared
    Blocked,            // hay un solido entre el jugador y la pared (no se construye a traves)
    OutOfBounds,
    NoMaterials,        // no hay material suficiente de ningun tipo (la comprueba la partida)
};

const char* placementErrorName(PlacementError error);

struct WallPlacement {
    bool valid = false;
    PlacementError error = PlacementError::None;
    PieceType type = PieceType::Wall;
    world::Aabb box;     // caja que engloba la pieza
    bool alongX = true;  // pared: se extiende a lo largo de X (si no, de Z)
    // Rampa: sube hacia 0 = +X, 1 = -X, 2 = +Z, 3 = -Z; `level` = altura del pie de la rampa.
    int dir = 0;
    float level = 0.0f;
    // Material con que se construiria: el elegido o, si no hay bastante, otro que si haya.
    world::Resource material = world::Resource::Wood;
};

// Cajas de colision de una pieza (devuelve cuantas).
int pieceBoxes(const WallPlacement& placement, const BuildConfig& config, world::Aabb* out);

struct BuiltWall {
    bool active = false;
    PieceType type = PieceType::Wall;
    world::Aabb box;  // engloba la pieza
    bool alongX = true;
    int dir = 0;
    float level = 0.0f;
    world::Resource material = world::Resource::Wood;
    int colliders[kMaxPieceBoxes];  // identificadores de las cajas dinamicas
    int colliderCount = 0;
    float health = 0.0f;
    float maxHealth = 0.0f;
    float sinceHit = 99.0f;  // para el parpadeo al recibir un disparo
    int owner = 0;           // 0 = jugador, 1..N = bot que la levanto
};

class StructureSystem {
public:
    // Quita todas las paredes (restaurando colision y navegacion) y aplica la configuracion.
    void reset(world::CollisionWorld& world, NavGrid& nav, const BuildConfig& config);

    // Calcula donde iria la pared delante de `feet` mirando a `yaw` y si es valida.
    // `player` (quien construye) y `bots` (el resto) son las cajas de los cuerpos que no
    // puede atravesar. `owner`: 0 = jugador (limite maxWalls), 1..N = bot.
    WallPlacement plan(const math3d::Vec3& feet, float yaw, const world::Aabb& player,
                       const world::Aabb* bots, int botCount, int owner = 0,
                       PieceType type = PieceType::Wall) const;

    int place(const WallPlacement& placement, int owner = 0);  // indice de la pared o -1
    // Coloca una pieza ya calculada (planos del modo zombi, v0.14) sin las comprobaciones de
    // plan(): solo que no repita otra pieza y que quede sitio. `healthScale`: reforzada.
    int placeForced(const WallPlacement& placement, int owner, float healthScale);
    bool remove(int index);
    // Ya hay una pieza igual en ese sitio (v0.16: las de los aliados junto a sus camas).
    bool occupied(const WallPlacement& placement) const { return duplicates(placement); }
    // Resta vida; devuelve true si la pared se ha destruido.
    bool damage(int index, float amount);
    // Vida de una pieza en pie (reparar; cargar una partida guardada), sin pasar de su maximo.
    void setHealth(int index, float health);
    void update(float dt);

    // Pared a la que pertenece una caja dinamica (o -1).
    int wallFromCollider(int dynamicHandle) const;
    // Primera pared construida en el segmento (para quitarla apuntando), o -1.
    int wallAlong(const math3d::Vec3& from, const math3d::Vec3& to) const;

    // Paredes del jugador en pie (las del limite maxWalls) y de todos.
    int count() const { return countOwnedBy(0); }
    int totalCount() const { return m_count; }
    int countOwnedBy(int owner) const;  // owner < 0: de cualquier bot
    const BuildConfig& config() const { return m_config; }
    const BuiltWall& wall(int i) const { return m_walls[i]; }
    int capacity() const { return kMaxWallCapacity; }
    // Cambia cada vez que se coloca o desaparece una pared.
    unsigned revision() const { return m_revision; }

private:
    void changed(const world::Aabb& box);
    bool blockedFrom(const math3d::Vec3& chest, const world::Aabb& box) const;
    WallPlacement planCell(const math3d::Vec3& feet, const math3d::Vec3& forward, const world::Aabb& player,
                           const world::Aabb* bots, int botCount, int owner, PieceType type) const;
    bool duplicates(const WallPlacement& placement) const;
    bool limitReached(int owner) const;

    BuildConfig m_config;
    world::CollisionWorld* m_world = nullptr;
    NavGrid* m_nav = nullptr;
    BuiltWall m_walls[kMaxWallCapacity];
    int m_count = 0;
    unsigned m_revision = 0;
};

}  // namespace game
