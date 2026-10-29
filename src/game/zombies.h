// Modo Supervivencia zombi (v0.14, docs/ZOMBIS.md): datos del modo.
//
// El jugador despierta en una base abandonada de Polvorin Polvoriento (en ruinas, mas
// tenebroso), sin tormenta ni rivales: solo zombis. Hay dia y noche; de noche salen mas
// zombis, mas rapidos, y llega una horda. En el botin solo hay escopetas, pistolas y
// curativos: el resto de armas se fabrican con materiales en los centros de crafteo. Por
// el mapa esperan supervivientes (se unen si les das un arma y una curacion) y planos para
// fortificar la base.
//
// Los zombis y los supervivientes son entidades de la partida (game::Bot con su BotKind),
// asi que comparten fisica, disparos, dano, dibujo y navegacion con los bots del battle
// royale.
#pragma once

#include "core/i18n.h"
#include "game/inventory.h"
#include "game/structures.h"
#include "world/zombie_sites.h"

namespace game {

// --- Zombis -----------------------------------------------------------------------------------

enum class ZombieType : u8 { Walker, Runner, Brute, Count };

struct ZombieDef {
    i18n::T name;
    float health;
    float speed;            // fraccion de la carrera del jugador (de dia)
    float nightSpeed;       // ...de noche
    float damage;           // por golpe al jugador o a un superviviente
    float structureDamage;  // por golpe a una pieza construida
    float attackInterval;   // s entre golpes
    float reach;            // m (desde el centro) para golpear
    float scale;            // tamano del modelo y de la caja de impacto
};

const ZombieDef& zombieDef(ZombieType type);

// --- Crafteo (centros de crafteo) -----------------------------------------------------------

struct Recipe {
    i18n::T name;
    ItemType item;       // None: municion del arma en la mano
    Rarity rarity;
    int count;
    int cost[static_cast<int>(world::Resource::Count)];  // madera, piedra, metal
};

int recipeCount();
const Recipe& recipe(int index);

// --- Planos de la base ----------------------------------------------------------------------

// Una pieza de un plano: tipo, celda de la rejilla de 4 m (esquina menor), piso (0 = suelo,
// 1 = a 3 m) y orientacion (pared: a lo largo de X en el borde z = z0 si alongX, si no a lo
// largo de Z en el borde x = x0; rampa: sube hacia `dir`, como WallPlacement::dir).
struct BlueprintPiece {
    PieceType type;
    float x0, z0;
    int floor;
    bool alongX;
    int dir;
};

constexpr int kBlueprintCount = world::zsites::kBlueprintSpots;
constexpr int kMaxBlueprintPieces = 8;

struct Blueprint {
    i18n::T name;
    i18n::T description;
    int pieceCount;
    BlueprintPiece pieces[kMaxBlueprintPieces];
};

const Blueprint& blueprint(int index);
// Pieza de un plano como colocacion de construccion (caja y orientacion; sin validar).
WallPlacement blueprintPlacement(const BlueprintPiece& piece, const BuildConfig& config);

// --- Configuracion --------------------------------------------------------------------------

constexpr int kZombieSlots = 22;  // zombis a la vez como mucho (entidades 1..22)
constexpr int kSurvivorCount = world::zsites::kCampCount;  // uno por campamento (entidades 23..27)

struct ZombieConfig {
    float dayLength = 150.0f;    // s de dia
    float nightLength = 95.0f;   // s de noche
    float startClock = 15.0f;    // s de dia ya pasados al despertar
    float duskSeconds = 12.0f;   // transicion de la luz (anochecer / amanecer)
    // Zombis en el mapa a la vez: base + por dia, sin pasar de kZombieSlots.
    int dayZombies = 7;
    int nightZombies = 13;
    int zombiesPerDay = 2;
    float daySpawnInterval = 2.6f;   // s entre apariciones mientras falten
    float nightSpawnInterval = 0.9f;
    float spawnMinDistance = 30.0f;  // m del jugador (y fuera de su vista si es posible)
    float spawnMaxDistance = 58.0f;
    int hordeBase = 6;               // zombis de la horda de cada noche (+ hordePerDay)
    int hordePerDay = 2;
    float daySight = 16.0f;          // m: a esta distancia te ven (de noche, nightSight)
    float nightSight = 26.0f;
    float smellRange = 5.0f;         // m: te notan aunque no te vean
    float hearingRange = 34.0f;      // disparos
    int runnersFromDay = 2;          // dia desde el que salen corredores (de noche, desde el 1)
    int brutesFromNight = 2;         // noche desde la que salen brutos
    float survivorDamageScale = 0.8f;
    float survivorSight = 28.0f;
    int pieceCost = 10;              // material por pieza de un plano
    float pieceHealthScale = 2.0f;   // las piezas de los planos son reforzadas
    float pieceReach = 3.2f;         // m para construir una pieza fantasma con CUADRADO
};

}  // namespace game
