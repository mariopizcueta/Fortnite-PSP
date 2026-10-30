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
//
// v0.15 (docs/ZOMBIS.md): lugares de interes con neveras y despensas, hambre y sueno (cama
// y saco de dormir), partida guardada, inventario de los aliados, aliados caidos que se
// levantan como zombis armados, aliados que construyen, mochilas, objetivos variados y mas
// miedo.
#pragma once

#include "core/i18n.h"
#include "game/inventory.h"
#include "game/structures.h"
#include "world/zombie_sites.h"

namespace game {

// --- Zombis -----------------------------------------------------------------------------------

// Fallen (v0.15): un aliado caido que se levanta con sus armas y dispara.
enum class ZombieType : u8 { Walker, Runner, Brute, Fallen, Count };

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
// Los aliados levantan, aunque no se tenga ningun plano, el muro del perimetro de la base
// (las piezas de los planos 0 y 1): kPalisadeBlueprints.
constexpr int kPalisadeBlueprints = 2;
// Pieza de un plano como colocacion de construccion (caja y orientacion; sin validar).
WallPlacement blueprintPlacement(const BlueprintPiece& piece, const BuildConfig& config);

// --- Objetivos (v0.15) -------------------------------------------------------------------------
// Siempre hay dos a la vista; al cumplir uno llega otro distinto. Cada uno da una recompensa
// (materiales, municion, comida o curas) y pavos.
enum class ObjectiveType : u8 {
    KillZombies,    // abate N zombis
    KillRunners,    // abate N corredores
    KillBrute,      // abate un bruto
    OpenFridges,    // registra N neveras o despensas
    EatFood,        // come N veces
    Craft,          // fabrica N objetos
    BuildPieces,    // construye N piezas
    ExploreSite,    // llega a un lugar de interes (`site`)
    Harvest,        // consigue N materiales con el pico
    SleepWell,      // duerme hasta llenar el descanso
    Headshots,      // N disparos a la cabeza a zombis
    Count
};
enum class RewardType : u8 { Materials, Ammo, Food, Heals, Count };

struct Objective {
    ObjectiveType type = ObjectiveType::Count;  // Count: hueco libre
    int target = 0;
    int progress = 0;
    int site = -1;         // ExploreSite
    RewardType reward = RewardType::Materials;
    float doneTime = -1.0f;  // >= 0: cumplido (se muestra un momento y llega otro)
};
constexpr int kActiveObjectives = 2;
constexpr int kObjectiveVbucks = 10;

// --- Configuracion --------------------------------------------------------------------------

constexpr int kZombieSlots = 22;  // zombis a la vez como mucho (entidades 1..22)
constexpr int kMaxBeds = 8;       // camas colocadas (v0.15)
constexpr int kSurvivorCount = world::zsites::kCampCount;  // uno por campamento (entidades 23..27)

struct ZombieConfig {
    float dayLength = 300.0f;    // s de dia (v0.15: el doble)
    float nightLength = 180.0f;  // s de noche
    float startClock = 20.0f;    // s de dia ya pasados al despertar
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

    // v0.15: hambre (100 = saciado). Se vacia en ~1,5 dias; sin comida, se pierde vida.
    float startFood = 80.0f;
    float foodPerSecond = 100.0f / 720.0f;
    float starveDamage = 0.7f;       // vida por segundo con el hambre a 0
    // Descanso (100 = descansado). Se vacia en ~3 dias sin dormir; por debajo de tiredLevel
    // el personaje se equivoca (falla, se le escapa el disparo, ve cosas) y a 0 pierde vida.
    float startRest = 75.0f;
    float restPerSecond = 100.0f / 1440.0f;
    float tiredLevel = 33.0f;
    float exhaustedDamage = 0.5f;
    // Dormir: el tiempo pasa sleepTimeScale veces mas deprisa. En una cama el descanso se
    // llena en media noche; en el saco de dormir, en una noche entera.
    float sleepTimeScale = 8.0f;
    float sleepBlockRange = 18.0f;   // m: con un zombi mas cerca no se puede dormir
    float sleepWakeRange = 11.0f;    // ...y si se acerca, te despierta
    // Lugares de interes: sus guardianes aparecen a esta distancia del borde (una vez por dia).
    float siteGuardRange = 10.0f;
    float lurkerChance = 0.4f;       // guardianes tumbados entre las ruinas (se levantan al acercarse)
    float lurkerWakeRange = 5.5f;
    // Aliados: construyen y reparan la base (una pieza cada allyBuildSeconds; sin material del
    // jugador, el muro del perimetro con lo que rapinan, el triple de lento).
    float allyBuildSeconds = 6.0f;
    float allyBaseRange = 30.0f;     // m de la base: el jugador esta "en casa"
    float fallenRiseSeconds = 4.0f;  // un aliado caido se levanta como zombi armado
    // v0.16: aliados inteligentes. Vida y escudo maximos; a mas de allyLeash m del jugador lo
    // dejan todo (incluso una pelea) y van con el; buscan botin a allyLootRange y fabrican si
    // hay un centro de crafteo a allyCraftRange; con poca vida (vida + escudo) huyen y se cubren.
    float allyHealth = 125.0f;
    float allyShield = 125.0f;
    float allyLeash = 15.0f;
    float allyLootRange = 12.0f;
    float allyCraftRange = 14.0f;
    float allyCraftSeconds = 2.5f;
    float allyFleeHealth = 70.0f;
    float allyRecovered = 140.0f;
    int allyMaterialGoal = 240;      // materiales propios que buscan tener
    int allyHarvestBelow = 60;       // por debajo pican objetos cerca del jugador
    int allyStartWood = 30;          // lo que traen al unirse (para cubrirse)
    float allyFortRange = 30.0f;     // jugador a esta distancia de la cama: fortifican junto a ella
    float allyBedWalkLimit = 30.0f;  // s de noche andando a la cama: despues, acostado sin mas
    float restockChance = 0.35f;     // al amanecer, contenedores abiertos que se rellenan
};

}  // namespace game
