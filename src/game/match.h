// Partida de Battle Royale: jugador + bots, IA, disparos, eliminaciones y victoria.
//
// Identificadores de entidad: 0 = jugador, 1..N = bots (bot i = entidad i + 1).
// Todo avanza en step() con el paso fijo de fisica; el render solo lee el estado.
#pragma once

#include "core/random.h"
#include "game/bot.h"
#include "game/match_config.h"
#include "game/nav_grid.h"
#include "game/player.h"
#include "game/storm_zone.h"
#include "game/structures.h"
#include "game/weapon.h"
#include "world/collision.h"
#include "world/zombie_sites.h"

#include <vector>

namespace game {

enum class MatchPhase {
    Playing,     // el jugador sigue vivo
    Spectating,  // el jugador ha caido; los bots siguen hasta que quede uno
    Finished,    // queda una sola entidad (o ninguna)
};

const char* matchPhaseName(MatchPhase phase);

// Donde apunta el jugador este frame (rayo de la camara por la mira).
struct PlayerAim {
    math3d::Vec3 eye;
    math3d::Vec3 direction;  // normalizado
    bool fire = false;       // R mantenido (modo combate)
};

// Ordenes del jugador. Las de pulsacion (true un solo frame) se guardan hasta el
// siguiente paso de fisica, asi no se pierden aunque ese frame no tenga ningun paso.
struct PlayerCommands {
    bool buildMode = false;  // modo construccion activo (CIRCULO)
    bool placeWall = false;  // R pulsado en modo construccion: coloca la pieza elegida
    PieceType buildPiece = PieceType::Wall;  // pieza elegida con la cruceta
    bool removeWall = false; // quitar la pieza apuntada (pruebas; en partida se rompe con el pico)
    bool cycleMaterial = false;  // CUADRADO en construccion: madera -> piedra -> metal
    bool reload = false;     // CUADRADO pulsado en modo combate: abrir / recoger o recargar
    int cycleSlot = 0;       // cruceta izquierda (-1) / derecha (+1): otra ranura
    int selectSlot = -1;     // ranura concreta (teclas 1-5 en PC; -1 = ninguna)
    bool togglePickaxe = false;  // TRIANGULO: sacar / guardar el pico
    bool toggleCrouch = false;   // cruceta abajo: agacharse / levantarse
    bool useItem = false;    // R pulsado con un consumible en la mano
    bool reviveHeld = false; // CUADRADO mantenido junto a un companero derribado (v0.16)
    bool placeTrap = false;  // R en construccion con la trampa elegida (cuarta pieza)
    // Inventario lleno: cambiar el objeto del suelo `swapPickup` por la ranura `swapSlot`
    // (el jugador la elige en el selector del HUD). -1: nada.
    int swapPickup = -1;
    int swapSlot = -1;
};

// Por que no se ha podido usar un consumible.
enum class UseError : u8 { None, FullHealth, FullShield, FullFood };
const char* useErrorName(UseError error);

// Sucesos de la partida para sonido y animaciones. La partida los apila en una cola
// circular; quien los consume guarda hasta que numero ha leido (eventCount()).
enum class GameEventType : u8 {
    Shot,          // entity = tirador, position = boca del arma
    HitEntity,     // entity = alcanzado, other = tirador
    HitStructure,  // other = tirador, position = impacto en la pared construida
    PlayerHurt,    // entity = 0, other = atacante
    Eliminated,    // entity = victima, other = quien la elimino, amount = ItemType del golpe + 1 (0: pico/otro)
    WallBuilt,
    WallBroken,
    ObjectDestroyed,  // un objeto del mapa se queda sin vida (other = indice del objeto)
    WallRemoved,
    Reload,        // entity = 0 (el jugador empieza a recargar)
    PickaxeSwing,  // entity = 0, other = 1 si ha golpeado algo
    Harvest,       // entity = 0, other = world::Resource, amount = material conseguido
    ItemUsed,      // entity = 0, other = ItemType (consumible terminado)
    UseStarted,    // entity = 0, other = ItemType (empieza a usarse un consumible)
    UseCanceled,   // entity = 0, other = ItemType (se interrumpe sin gastarlo)
    StormEnter,    // entity = quien entra en la tormenta (la del borde o la que se cierra)
    StormClosing,  // empieza un cierre de la tormenta (other = numero de cierre)
    BusJump,       // entity = quien salta del autobus
    GliderOpen,    // entity = quien abre el ala delta
    Landed,        // entity = quien llega al suelo
    ContainerOpened,  // entity = quien lo abre, other = ContainerKind
    PickedUp,      // entity = quien recoge, other = ItemType (o -1: municion)
    AirdropIncoming,  // position = donde caera
    AirdropLanded,
    // Parche (fase 8).
    Explosion,     // entity = quien la causa, other = ItemType del arma, amount = radio (dm)
    Thrown,        // entity = quien lanza, other = ItemType (granada de mano)
    LlamaSpawned,  // position = donde esta la llama de esta partida
    TrapPlaced,    // entity = quien la pone, other = indice de la trampa
    TrapTriggered, // entity = duena de la trampa, other = indice, amount = personajes alcanzados
    TrapRemoved,   // la pieza en la que estaba se ha roto (other = indice)
    // v0.14: modo zombi.
    NightStarted,     // other = dia (empieza su noche)
    DayStarted,       // other = dia que empieza (se ha sobrevivido a la noche anterior)
    HordeIncoming,    // position = de donde viene; amount = zombis
    ZombieAttack,     // entity = zombi, other = victima (-1: una pieza construida)
    ZombieGroan,      // entity = zombi (gemido de vez en cuando, para el sonido)
    SurvivorFound,    // entity = superviviente (el jugador ha pasado cerca de su campamento)
    SurvivorGift,     // entity = superviviente, other = ItemType dado (-1: nada), amount = lo que le falta
    SurvivorRescued,  // entity = superviviente
    WorkbenchFound,   // other = contenedor del centro de crafteo
    WorkbenchOpened,  // other = contenedor (la aplicacion abre el menu de crafteo)
    Crafted,          // other = receta, amount = 1 si se ha hecho, 0 si faltan materiales
    BlueprintFound,   // other = plano
    BlueprintPiece,   // other = plano, amount = pieza
    BlueprintDone,    // other = plano (todas sus piezas en pie)
    // v0.14: tacticas de los bots.
    BotCamping,       // entity = bot que se aposta junto a un airdrop
    // v0.15: modo zombi.
    SiteDiscovered,   // other = lugar de interes (world::zsites::kSites)
    SurvivorMenu,     // entity = aliado (la aplicacion abre la gestion de su inventario)
    AllyDown,         // entity = aliado caido (se levantara como zombi armado)
    AllyTurned,       // entity = el aliado que se ha levantado como zombi armado
    AllyBuilt,        // entity = aliado, other = plano, amount = pieza (o reparada: -1)
    AllySleeps,       // entity = aliado que se acuesta en su cama
    BackpackFound,    // amount = ranuras que tiene ahora el inventario
    BedPlaced,        // other = contenedor de la cama
    SleepStarted,     // other = cama (-1: saco de dormir)
    SleepEnded,       // amount = SleepEnd
    SleepBlocked,     // hay monstruos cerca (no se puede dormir)
    Hungry,           // amount = 1 con hambre, 2 muerto de hambre (pierde vida)
    Tired,            // amount = 1 cansado (se equivoca), 2 agotado (pierde vida)
    Fumble,           // amount = Fumble (el cansancio le juega una mala pasada)
    LurkerRise,       // entity = zombi que se levanta de entre las ruinas
    ObjectiveDone,    // other = hueco del objetivo, amount = pavos
    ObjectiveNew,     // other = hueco del objetivo
    Restocked,        // al amanecer: amount = contenedores que se han rellenado
    // v0.16.
    Downed,           // entity = derribado, other = quien le ha derribado (equipos)
    Revived,          // entity = reanimado, other = quien le ha reanimado
    AllyCrafted,      // entity = aliado, other = receta
    BedAssigned,      // entity = aliado (-1: cama libre), other = contenedor de la cama
};

// Por que se despierta el jugador (GameEventType::SleepEnded).
enum class SleepEnd : u8 { Rested, Canceled, Zombie, Hurt, Dawn };
// Errores por falta de sueno (GameEventType::Fumble).
enum class Fumble : u8 { Stumble, Misfire, DropAmmo };

// Fase de la caida desde el autobus de cada personaje.
enum class AirState : u8 { Bus, Freefall, Glide, Ground };
const char* airStateName(AirState state);

// --- Botin (docs/BOTIN.md) ----------------------------------------------------------------

// Objeto en el suelo: un arma o consumible (con su rareza), municion o materiales de
// construccion (los sueltan los bots al caer y la llama; se recogen solos al pasar).
// Plano (v0.14, modo zombi): `count` = indice del plano; se recoge con CUADRADO.
// Mochila (v0.15): +3 ranuras de inventario; la sueltan los brutos y los aliados caidos.
enum class PickupKind : u8 { Item, Ammo, Materials, Blueprint, Backpack };
struct Pickup {
    bool active = false;
    PickupKind kind = PickupKind::Item;
    ItemType item = ItemType::None;
    Rarity rarity = Rarity::Common;
    AmmoType ammo = AmmoType::Medium;
    int count = 0;
    int loaded = -1;  // arma: balas en el cargador (-1: lleno). Al soltarla conserva las suyas.
    world::Resource resource = world::Resource::Wood;  // PickupKind::Materials
    math3d::Vec3 position;
    int claimedBy = -1;  // bot que va a por el
};

// Llama: como mucho una por partida (MatchConfig::llamaChance, 5 %); se abre con CUADRADO o
// se rompe a disparos o con el pico y suelta materiales, municion y objetos. Centro de
// crafteo (v0.14, modo zombi): no se abre ni se gasta; CUADRADO abre el menu de crafteo.
// v0.15 (modo zombi): nevera y despensa (comida), y cama colocada por el jugador (no se
// abre: CUADRADO duerme en ella; `claimedBy` = aliado que duerme en ella).
enum class ContainerKind : u8 { Chest, AmmoBox, Airdrop, Llama, Workbench, Fridge, Pantry, Bed };
struct Container {
    ContainerKind kind = ContainerKind::Chest;
    math3d::Vec3 position;
    float yaw = 0.0f;
    bool opened = false;
    float openedAt = 0.0f;  // s de partida en que se abrio (se ve abierto un instante y desaparece)
    bool landed = true;   // airdrop: false mientras baja con el globo
    int claimedBy = -1;
    float health = 0.0f;  // llama: vida antes de romperse (las demas no se rompen)
    bool discovered = false;  // centro de crafteo: el jugador ha pasado cerca (minimapa)
    int site = -1;            // v0.15: lugar de interes al que pertenece
};

// --- Trampas (parche, fase 8; docs/TRAMPAS.md) ---------------------------------------------
// De momento solo la trampa de pinchos (los unicos recursos: Recursos/HUD/spiketrap.webp y
// Referencias/FortniteFloorTrap). La estructura es comun para anadir otros tipos.
enum class TrapType : u8 { Spike };
// Donde va: en un suelo (pinchos hacia arriba), en una pared o en un techo (hacia abajo).
enum class TrapSurface : u8 { Floor, Wall, Ceiling };
enum class TrapError : u8 { None, NoTraps, NoSurface, Ramp, TooSmall, Occupied, Obstructed, LimitReached };
const char* trapErrorName(TrapError error);

// Placa de la trampa: centro sobre la superficie, normal hacia fuera, y semiancho a lo largo
// de los dos ejes de la superficie (todas las superficies estan alineadas con los ejes).
struct TrapPlacement {
    bool valid = false;
    TrapError error = TrapError::NoSurface;
    TrapSurface surface = TrapSurface::Floor;
    math3d::Vec3 center, normal{0.0f, 1.0f, 0.0f}, axisU{1.0f, 0.0f, 0.0f};
    float halfU = 1.0f, halfV = 1.0f;
    int wall = -1;    // pieza construida en la que va (-1: el mapa o el suelo)
    int object = -1;  // objeto del mapa en el que va (si se destruye, la trampa tambien)
};

struct Trap {
    bool active = false;
    TrapType type = TrapType::Spike;
    TrapPlacement place;
    world::Aabb wallBox;      // caja de la pieza al ponerla (si la pieza cambia, ya no esta)
    world::Aabb trigger;      // donde se activa (delante de la placa)
    int owner = 0;            // no hace dano a quien la pone
    float cooldown = 0.0f;    // s hasta poder volver a saltar
    float sinceTrigger = 99.0f;
};

// Proyectil en vuelo: cohete, granada del lanzagranadas o granada de mano.
struct Projectile {
    bool active = false;
    ProjectileKind kind = ProjectileKind::None;
    int owner = -1;
    int weaponType = -1;  // ItemType (sonido y dibujo)
    math3d::Vec3 position, velocity;
    float age = 0.0f;
    float traveled = 0.0f;
    float damage = 0.0f, blastRadius = 0.0f, structureDamage = 0.0f;
    float fuse = 0.0f;         // granadas: s hasta explotar
    float maxDistance = 0.0f;  // cohete: explota al recorrerla
};

// Explosion reciente (para dibujarla).
struct Explosion {
    math3d::Vec3 position;
    float radius = 0.0f;
    float age = 99.0f;
};

// Lo que hay al alcance para abrir o recoger con CUADRADO.
// v0.14 (modo zombi): un superviviente (indice del bot) y una pieza fantasma de un plano
// (indice = plano * kMaxBlueprintPieces + pieza).
struct Interactable {
    // v0.15: Survivor tambien es un aliado ya rescatado (CUADRADO: su inventario).
    enum class Kind : u8 { None, Pickup, Container, Survivor, BlueprintPiece } kind = Kind::None;
    int index = -1;
};

// "Atacante" del dano de la tormenta (registro de eliminaciones, quien elimino al jugador).
constexpr int kStormKiller = -2;

struct GameEvent {
    GameEventType type = GameEventType::Shot;
    int entity = -1;
    int other = -1;
    int amount = 0;
    math3d::Vec3 position;
};

// Numero de dano flotante (como en Fortnite): solo los impactos del jugador.
struct DamageNumber {
    math3d::Vec3 position;
    float amount = 0.0f;
    float age = 99.0f;
    bool structure = false;  // impacto en una pared construida
    bool headshot = false;   // en la cabeza (numero amarillo)
    bool shield = false;     // el rival tenia escudo (numero azul, como en Fortnite)
};

struct KillEvent {
    int killer = -1;  // -1: desconocido
    int victim = -1;
    float time = 0.0f;
};

struct Tracer {
    math3d::Vec3 from, to;
    float age = 0.0f;
    bool byPlayer = false;
    bool hit = false;
};

struct ShotResult {
    bool hitEntity = false;
    bool headshot = false;
    int victim = -1;
    bool blockedByWorld = false;
    math3d::Vec3 end;
};

// Registro de un disparo, para verificarlo desde fuera (pruebas de host).
struct ShotRecord {
    int shooter = -1;
    math3d::Vec3 origin, end;
    int victim = -1;
    bool blockedByWorld = false;
};
using ShotObserver = void (*)(void* context, const ShotRecord& shot);

// Contadores para el HUD de depuracion y para medir rendimiento.
struct MatchStats {
    int decisions = 0;           // decisiones de IA tomadas (total)
    int pathRequests = 0;
    int pathFailures = 0;
    int maxPathExpansions = 0;
    int stuckEvents = 0;
    int shotsFired = 0;
    int shotsHitEntity = 0;
    int shotsBlockedByWorld = 0;
    int shotsHitStructure = 0;
    int wallsBuilt = 0;
    int wallsDestroyed = 0;
    int wallsRemoved = 0;
    int botHarvestHits = 0;      // golpes de pico de los bots a objetos del mapa (v0.13)
    int botFlees = 0;            // veces que un bot con poca vida ha empezado a huir
    int objectsShot = 0;         // impactos de bala en objetos del mapa
    int botCamps = 0;            // v0.14: bots que se apostan junto a un airdrop
    int botBoxes = 0;            // ...que levantan una caja de paredes
    int botRamps = 0;            // ...que levantan una rampa para ganar altura
    int botHolds = 0;            // ...que mantienen la posicion al final de la partida
    int zombieHits = 0;          // golpes de zombis (a personajes y a piezas)
    int zombiesSpawned = 0;
    int guardsSpawned = 0;       // v0.15: guardianes de los lugares de interes
    int allyPieces = 0;          // ...piezas levantadas o reparadas por los aliados
    int downs = 0;               // v0.16: derribos (equipos)
    int revives = 0;             // ...y reanimaciones
    int allyCrafts = 0;          // ...objetos fabricados por los aliados
    u64 aiMicros = 0;            // tiempo acumulado de IA (decidir + dirigir)
    u64 physicsMicros = 0;       // tiempo acumulado de fisica de personajes
    int steps = 0;
};

class Match {
public:
    static constexpr int kMaxKillFeed = 4;
    static constexpr int kMaxTracers = 24;
    static constexpr int kMaxDamageNumbers = 12;
    static constexpr int kMaxEvents = 64;

    // `world` es modificable: las paredes construidas se anaden como cajas dinamicas.
    void start(const MatchConfig& config, world::CollisionWorld& world, NavGrid& nav,
               const std::vector<math3d::Vec3>& pointsOfInterest, const math3d::Vec3& playerSpawn,
               float playerYaw);

    // Entrada del jugador para los pasos siguientes (se llama una vez por frame).
    void setPlayerInput(const PlayerInput& input, const PlayerAim& aim,
                        const PlayerCommands& commands = PlayerCommands{});
    // Un paso fijo de simulacion.
    void step(float dt);

    // --- Consultas ------------------------------------------------------------------
    MatchPhase phase() const { return m_phase; }
    int aliveCount() const { return m_alive; }
    int entityCount() const { return 1 + static_cast<int>(m_bots.size()); }
    int winner() const { return m_winner; }
    int playerPlace() const { return m_playerPlace; }
    // Equipos (v0.16, docs/EQUIPOS.md).
    bool teamMode() const { return m_config.teamSize > 1; }
    int team(int id) const { return m_team[id]; }
    bool sameTeam(int a, int b) const { return m_team[a] == m_team[b]; }
    int teamCount() const;       // equipos al empezar
    int teamsLeft() const;       // equipos con alguien vivo
    bool playerWon() const { return m_phase == MatchPhase::Finished && m_winner >= 0 && sameTeam(m_winner, 0); }
    // Queda alguien del equipo del jugador (aunque el haya caido): la partida sigue para el.
    bool playerTeamAlive() const;
    bool downed(int id) const { return m_downed[id]; }
    // Progreso 0..1 de quien le esta reanimando (0: nadie), y quien.
    float reviveProgress(int id) const { return m_reviveTime[id] / m_config.reviveSeconds; }
    int reviver(int id) const { return m_reviver[id]; }
    // Companero derribado al alcance del jugador (nadie mas le reanima) o -1, y a quien esta
    // reanimando ahora (-1: a nadie).
    int reviveTarget() const;
    int playerReviving() const { return m_playerReviving; }
    // Segundos de vida que le quedan a un derribado si nadie le reanima.
    float bleedLeft(int id) const { return m_bleedRate[id] > 0.0f ? body(id).state().health / m_bleedRate[id] : 0.0f; }
    int playerKills() const { return m_playerKills; }
    float elapsed() const { return m_time; }
    // Zona segura actual (tormenta que se cierra) y la siguiente.
    float zoneRadius() const { return m_storm.radius(); }
    const math3d::Vec3& zoneCenter() const { return m_storm.center(); }
    const StormZone& storm() const { return m_storm; }
    // Metros dentro de la tormenta (la del borde o la que se cierra); negativo: fuera.
    float stormDepthAt(float x, float z) const;
    const MatchConfig& config() const { return m_config; }

    bool alive(int id) const;
    // Dentro de la tormenta del limite del mapa (world/storm.h).
    bool inStorm(int id) const { return m_inStorm[id]; }
    const CharacterBody& body(int id) const;
    const char* name(int id) const;
    // Pruebas: el jugador no recibe dano mientras este activo.
    void setPlayerInvulnerable(bool on) { m_config.invulnerablePlayer = on; }
    // Nombre del jugador (el del perfil) para el HUD y el registro de eliminaciones.
    void setPlayerName(const char* name);
    int botCount() const { return static_cast<int>(m_bots.size()); }
    const Bot& bot(int i) const { return m_bots[i]; }
    bool playerAiming() const { return m_playerAiming; }
    bool buildMode() const { return m_buildMode; }
    // Arma en la mano o, si se lleva otra cosa, la primera del inventario (para el HUD).
    const Weapon& playerWeapon() const;
    // Municion de reserva del arma que se lleva (o de la primera del inventario).
    int playerReserve() const;
    const Inventory& inventory() const { return m_inventory; }
    const Materials& materials() const { return m_materials; }
    // Consumible en uso: progreso 0..1 (o -1 si no se esta usando nada) y que objeto es.
    // Mientras se usa, el jugador no se mueve, no salta ni se agacha (la camara si gira).
    float useProgress() const { return m_useSlot >= 0 ? m_useTimer / m_useDuration : -1.0f; }
    float useDuration() const { return m_useDuration; }
    // Tiempo de uso de un consumible (MatchConfig::useSeconds o, si es 0, el de ItemDef).
    float useSecondsFor(ItemType type) const;
    ItemType usingItem() const { return m_useSlot >= 0 ? m_inventory.slot(m_useSlot).type : ItemType::None; }
    UseError lastUseError() const { return m_useError; }
    float sinceUseError() const { return m_sinceUseError; }
    // Objetos del mapa que se pueden picar y destruir (indices de world::World::harvestables()).
    float objectHealth(int index) const { return m_objectHealth[index]; }
    bool objectDestroyed(int index) const { return m_objectDestroyed[index] != 0; }
    // Para World::draw (no dibuja los destruidos); nullptr si no hay objetos.
    const u8* destroyedObjects() const { return m_objectDestroyed.empty() ? nullptr : m_objectDestroyed.data(); }
    int destroyedObjectCount() const;
    // Quita vida a un objeto; sin vida desaparece (colision y dibujo). true si se destruye.
    bool damageObject(int index, float amount, const math3d::Vec3& point, int by);
    // Objetos que se pueden picar (los del mundo). Se llama una vez; start() los repone.
    void setHarvestables(const std::vector<world::Harvestable>* list);

    // Autobus y caida.
    AirState airState(int id) const { return m_air[id]; }
    const math3d::Vec3& busPosition() const { return m_config.busPosition; }
    float busTimeLeft() const { return m_config.busSeconds - m_time; }
    // Metros sobre el suelo (caida libre: para el HUD).
    float heightAboveGround(int id) const;

    // Proyectiles y explosiones (parche, fase 8).
    static constexpr int kMaxProjectiles = 24;
    static constexpr int kMaxExplosions = 6;
    const Projectile* projectiles() const { return m_projectiles; }
    const Explosion* explosions() const { return m_explosions; }
    // Explosion: dano decreciente con la distancia (a quien la ve desde el centro), y dano
    // a construcciones, objetos del mapa y la llama. Publico para las pruebas.
    void explode(const math3d::Vec3& at, int owner, float damage, float radius, float structureDamage, int weaponType);
    int spawnProjectile(int owner, const math3d::Vec3& from, const math3d::Vec3& direction, const WeaponConfig& weapon,
                        float damageScale, int weaponType);
    // Lanza una granada de mano hacia la mira (como si el jugador pulsara R con ella).
    bool throwGrenade();
    // Llama de esta partida (indice en containers()) o -1.
    int llama() const { return m_llama; }

    // Trampas: las que lleva el jugador (en la barra de construccion) y las colocadas.
    static constexpr int kMaxTraps = 16;
    int trapStock() const { return m_trapStock; }
    void giveTraps(int n);
    const Trap* traps() const { return m_traps; }
    // Donde iria una trampa apuntando con la mira (y si se puede poner).
    TrapPlacement planTrap(const PlayerAim& aim) const;
    // Coloca una trampa (como R en construccion con la trampa): indice o -1.
    int placeTrap(const PlayerAim& aim);

    // Botin.
    const std::vector<Pickup>& pickups() const { return m_pickups; }
    const std::vector<Container>& containers() const { return m_containers; }
    // Lo que el jugador abriria / recogeria ahora con CUADRADO.
    Interactable playerInteractable() const { return interactableFor(0); }
    Interactable interactableFor(int id) const;
    // Abre o recoge (pruebas y bots; el jugador lo hace con CUADRADO). Si al jugador no le
    // cabe el objeto, lo cambia por el de la ranura seleccionada.
    bool interact(int id, const Interactable& target);
    // Cuantas unidades de un objeto del suelo caben en el inventario del jugador sin
    // sustituir nada (0: hay que elegir que soltar).
    int pickupRoom(int pickup) const;
    // El objeto del suelo sigue ahi y al alcance del jugador (para el selector de cambio).
    bool pickupInReach(int pickup) const;
    // Cambia el objeto del suelo por el de la ranura `slot`, que cae al suelo con su
    // cargador (si es un arma) y se puede volver a recoger. false si no esta al alcance.
    bool swapPickup(int pickup, int slot);
    // Gestion del inventario (L + R a la vez): intercambia dos ranuras (lo que se lleva en la
    // mano sigue en la mano) o suelta una al suelo con su cargador. false si no se puede.
    bool moveSlot(int from, int to);
    bool dropInventorySlot(int slot);
    // Lleva a la mano lo de esa ranura (guarda el pico).
    bool selectSlot(int slot);

    // --- Modo zombi (v0.14, docs/ZOMBIS.md) -------------------------------------------------
    bool zombieMode() const { return m_config.mode == GameMode::Zombies; }
    int day() const { return m_day; }
    bool night() const { return m_night; }
    // s que faltan para el siguiente anochecer (de dia) o amanecer (de noche).
    float timeToChange() const;
    // 0 a pleno dia, 1 de noche (con transicion al anochecer y al amanecer).
    float darkness() const;
    int nightsSurvived() const { return m_nightsSurvived; }
    int zombiesAlive() const;
    int zombieKills() const { return m_zombieKills; }
    int survivorsRescued() const;
    int survivorsAlive() const;  // rescatados y vivos
    int itemsCrafted() const { return m_itemsCrafted; }
    // Bandos: en el battle royale todos contra todos; en el modo zombi, los zombis contra el
    // jugador y los supervivientes rescatados (los que esperan en su campamento, con nadie).
    bool hostile(int a, int b) const;
    bool isZombie(int id) const { return id > 0 && id <= botCount() && m_bots[id - 1].kind == BotKind::Zombie; }
    bool isSurvivor(int id) const { return id > 0 && id <= botCount() && m_bots[id - 1].kind == BotKind::Survivor; }
    // Supervivientes: lo que le falta (1 = un arma, 2 = una curacion; 0 = rescatado) y darle
    // lo de la ranura elegida del jugador (CUADRADO junto a el).
    int survivorNeeds(int botIndex) const;
    bool giveToSurvivor(int botIndex);
    // Crafteo: centro de crafteo al alcance del jugador (contenedor) o -1; si hay material
    // para una receta, y fabricarla (junto a un centro de crafteo).
    int nearbyWorkbench() const;
    bool canCraft(int recipeIndex) const;
    bool craft(int recipeIndex);
    // Planos: encontrados, piezas en pie, completados alguna vez, y construir una pieza
    // fantasma (CUADRADO junto a ella, con material).
    bool blueprintFound(int i) const { return i >= 0 && i < kBlueprintCount && m_bpFound[i]; }
    bool blueprintDone(int i) const { return i >= 0 && i < kBlueprintCount && m_bpDone[i]; }
    int blueprintsDone() const;
    bool pieceBuilt(int bp, int piece) const;
    WallPlacement blueprintPiece(int bp, int piece) const;
    bool buildBlueprintPiece(int bp, int piece);
    // Pruebas: encontrar un plano sin ir a por el.
    void giveBlueprint(int i);
    // Tamano del personaje (los brutos son mas grandes; caja de impacto y dibujo).
    float entityScale(int id) const;

    // --- Modo zombi v0.15 ------------------------------------------------------------------
    // Hambre y descanso del jugador (0..100; 100 = saciado / descansado).
    float food() const { return m_food; }
    float rest() const { return m_rest; }
    void setNeeds(float food, float rest) { m_food = food; m_rest = rest; }  // pruebas
    bool tired() const { return zombieMode() && m_rest < m_config.zombie.tiredLevel; }
    // Durmiendo: 0 no, 1 en una cama, 2 en el saco de dormir. Mientras se duerme el tiempo pasa
    // sleepTimeScale veces mas deprisa (sleepScale()) y el jugador no se mueve.
    int sleeping() const { return m_sleeping; }
    float sleepScale() const { return m_sleeping ? m_config.zombie.sleepTimeScale : 1.0f; }
    // Dormir en la cama `bed` (contenedor) o, con -1, en el saco de dormir. false (y suceso
    // SleepBlocked) si hay monstruos cerca o si el descanso ya esta lleno.
    bool trySleep(int bed);
    void wakeUp(SleepEnd why = SleepEnd::Canceled);
    // R con una cama en la mano: la coloca delante. Devuelve el contenedor o -1.
    int placeBed();
    int bedCount() const;
    // Aliados: gestionar su inventario (CUADRADO junto a uno rescatado). Darle la ranura
    // `slot` del jugador o quitarle la suya (`botSlot`).
    bool allyGive(int botIndex, int slot);
    bool allyTake(int botIndex, int botSlot);
    // Camas (v0.16): cama al alcance del jugador (contenedor) o -1; asignarla a un aliado
    // (indice de bot; -1: libre). De noche, ese aliado duerme en ella pase lo que pase.
    int nearbyBed() const;
    bool assignBed(int bed, int botIndex);
    // Bot aliado que tiene asignada la cama, o -1.
    int bedOwner(int bed) const;
    // Pieza `k` de lo que el aliado levanta junto a la cama `bed` (false: ahi no cabe).
    bool fortPiece(int bed, int k, WallPlacement* out) const;
    // Lugares de interes descubiertos (al entrar en ellos).
    bool siteDiscovered(int i) const { return i >= 0 && i < world::zsites::kSiteCount && m_siteFound[i]; }
    // Objetivos activos y cumplidos.
    const Objective& objective(int i) const { return m_objectives[i]; }
    int objectivesDone() const { return m_objectivesDone; }
    int rescuedTotal() const { return m_rescuedTotal; }
    // Pruebas: fija un objetivo concreto en un hueco.
    void setObjective(int slot, ObjectiveType type, int target);
    // Pruebas: pone el reloj del modo zombi (dia, de noche o no, segundos dentro).
    void setDayClock(int day, bool night, float clock) {
        m_day = day;
        m_night = night;
        m_zClock = clock;
    }
    // Pruebas: aparece un zombi de este tipo cerca de un punto (indice de bot o -1).
    int spawnZombieAt(ZombieType type, const math3d::Vec3& near);
    // Partida guardada (texto, game/zombie_save.cpp). save: false si no se puede escribir
    // (`vbucks`: pavos ganados en ella, para seguir sumando). load, tras start() con
    // MatchConfig::zombies() y la semilla del fichero: false si el fichero no vale.
    bool saveZombies(const char* path, int vbucks = 0) const;
    bool loadZombies(const char* path, int* vbucks = nullptr);
    // Dia y semilla de una partida guardada (false si no hay o no vale) y borrarla.
    static bool peekZombieSave(const char* path, int* day, u32* seed);
    static bool eraseZombieSave(const char* path);

    // Trucos del menu de administrador (se pueden cambiar a mitad de partida desde la pausa).
    void setCheats(const Cheats& cheats) { m_config.cheats = cheats; }
    const Cheats& cheats() const { return m_config.cheats; }
    // Mira del aimbot (truco): rival visible mas centrado respecto a la mira, o -1.
    int aimbotTarget(const math3d::Vec3& eye, const math3d::Vec3& direction, float range) const;
    // Acciones del menu: vida y escudo al maximo, eliminar a todos los bots y llenar
    // municion, materiales y trampas.
    void cheatHeal();
    void cheatEliminateBots();
    void cheatRefill();

    // Da un objeto al jugador (pruebas; en partida, el botin).
    int giveItem(ItemType type, int count, Rarity rarity = Rarity::Common) {
        return m_inventory.add(type, rarity, count);
    }
    void giveAmmo(AmmoType type, int n) { m_inventory.addAmmo(type, n); }
    // Deja un objeto en el suelo (pruebas).
    int dropPickup(const Pickup& pickup);
    int addContainer(ContainerKind kind, const math3d::Vec3& position, float yaw);
    // Arma del bot i (para el HUD de depuracion).
    const char* botWeaponName(int i) const;
    const StructureSystem& structures() const { return m_structures; }
    // Agachado (cruceta abajo): mas lento, caja de impacto mas baja y los bots fallan mas.
    bool playerCrouched() const { return m_playerCrouched; }
    // Caja de impacto de un personaje (mas baja si el jugador esta agachado).
    world::Aabb hitBox(int id) const;
    int playerHeadshots() const { return m_playerHeadshots; }
    // Dispersion actual del arma en la mano (grados): la que tendria el siguiente disparo
    // (la mira del HUD se abre con ella). Incluye el retroceso acumulado.
    float playerSpreadNow() const;
    float recoilBloom() const { return m_recoilBloom; }
    // Material elegido para construir (CUADRADO en construccion lo cambia).
    world::Resource buildMaterial() const { return m_buildMaterial; }
    // Donde iria una pieza ahora mismo (para dibujar la pieza fantasma).
    WallPlacement planWall(float yaw, PieceType type = PieceType::Wall) const;
    // Pared construida bajo la mira y a alcance para quitarla, o -1.
    int targetedWall(const PlayerAim& aim) const;
    const DamageNumber* damageNumbers() const { return m_damageNumbers; }
    // Numero total de sucesos emitidos; event(i) es valido para i en [count - kMaxEvents, count).
    u32 eventCount() const { return m_eventCount; }
    const GameEvent& event(u32 index) const { return m_events[index % kMaxEvents]; }

    const KillEvent& killEvent(int i) const { return m_killFeed[i]; }
    int killEventCount() const { return m_killFeedCount; }
    const Tracer* tracers() const { return m_tracers; }
    float sincePlayerHit() const { return m_sincePlayerHit; }        // el jugador acerto
    float sincePlayerDamaged() const { return m_sincePlayerDamaged; }
    int playerAssistTarget() const { return m_assistTarget; }
    const MatchStats& stats() const { return m_stats; }

    // Bot visible mas centrado respecto a la direccion dada (fijar objetivo con L).
    int findLockTarget(const math3d::Vec3& eye, const math3d::Vec3& forward,
                       float maxAngleDegrees, float maxDistance) const;

    // --- Combate (publico para las pruebas de host) -------------------------------------
    // Disparo instantaneo: el primer obstaculo (kBlocksShots, paredes construidas
    // incluidas) o entidad viva del segmento.
    ShotResult fireShot(int shooter, const math3d::Vec3& origin, const math3d::Vec3& direction,
                        float range, float damage);
    // Disparo del jugador: rayo desde la camara por la mira, comprobado desde el arma.
    // `spreadDegrees`: dispersion aplicada tras la ayuda de punteria; `weapon`: caida del
    // dano con la distancia y tipo para el sonido (nullptr: sin caida). `emitShot`: false
    // en los perdigones 2..N de la escopeta (un solo sonido).
    // Punto donde acaba el rayo de la mira (desde la altura del jugador): la primera pared,
    // objeto o personaje en `range` metros. Es el mismo calculo del disparo del jugador.
    math3d::Vec3 aimPoint(const math3d::Vec3& eye, const math3d::Vec3& direction, float range) const;
    ShotResult playerShot(const PlayerAim& aim, float damage, float range, float spreadDegrees = 0.0f,
                          const WeaponConfig* weapon = nullptr, bool emitShot = true, int weaponType = -1);
    // `ignoreShield`: el dano va directo a la vida (la tormenta, como en Fortnite).
    void applyDamage(int victim, float amount, int attacker, bool ignoreShield = false);
    bool lineOfSight(const math3d::Vec3& from, const math3d::Vec3& to) const;
    math3d::Vec3 eyeOf(int id) const;
    math3d::Vec3 chestOf(int id) const;
    // Coloca una entidad directamente (pruebas).
    void teleport(int id, const math3d::Vec3& position, float yaw);
    // Se llama tras cada disparo (pruebas de host; en el juego no se usa).
    void setShotObserver(ShotObserver observer, void* context) {
        m_shotObserver = observer;
        m_shotObserverContext = context;
    }

    // Pico y consumibles (publico para las pruebas): como si el jugador pulsara R.
    void swingPickaxe();
    bool startUse();
    void giveMaterials(world::Resource r, int n) { m_materials.add(r, n); }

    // Construccion (publico para las pruebas): coloca / quita como haria el jugador.
    int buildWall(float yaw, PieceType type = PieceType::Wall);
    bool removeWallAt(const PlayerAim& aim);
    // Envia un bot a un destino (pruebas de navegacion).
    void commandBotTravel(int botIndex, const math3d::Vec3& destination);
    // Da materiales a un bot (pruebas).
    void giveBotMaterials(int botIndex, world::Resource r, int n) { m_bots[botIndex].materials.add(r, n); }
    // Da un objeto a un bot (pruebas).
    int giveBotItem(int botIndex, ItemType type, int count, Rarity rarity = Rarity::Common) {
        return m_bots[botIndex].bag.add(type, rarity, count);
    }
    // Fija la vida y el escudo de una entidad (pruebas).
    void setHealth(int id, float health, float shield) {
        mutableBody(id).state().health = health;
        mutableBody(id).state().shield = shield;
    }

private:
    CharacterBody& mutableBody(int id);
    ShotResult resolveShot(int shooter, const math3d::Vec3& origin, const math3d::Vec3& end, float damage,
                           const WeaponConfig* weapon = nullptr, bool emitShot = true, int weaponType = -1);
    void fireWeapon(int shooter, const math3d::Vec3& origin, const math3d::Vec3& direction,
                    const WeaponConfig& weapon, float spreadDegrees, float damageScale, int weaponType);
    void playerFire(Weapon& weapon);
    float playerSpread(const WeaponConfig& weapon) const;
    // Autobus, caida y aterrizaje (match_air.cpp).
    void startBus();
    MoveIntent airIntent(int id, const math3d::Vec3& wish, bool openGlider);
    void updateAirState(int id, bool jump);
    void chooseLanding(Bot& bot);
    // Botin (match_loot.cpp).
    void spawnLoot();
    math3d::Vec3 lootSpot(float minSpacing, bool nearPoi);
    void spawnContainerLoot(int container, int opener);
    void dropAround(const math3d::Vec3& center, const Pickup& pickup, int index, int total, float radius = 0.9f,
                    float turn = 0.0f);
    void updateAirdrops(float dt);
    void autoPickupAmmo(int id);
    void spawnLlama();
    void spawnZombieSites();
    void damageContainer(int index, float amount, int by, const math3d::Vec3& point);
    // Cajas de la llama (para disparos, pico y explosiones) y si la llama esta en pie.
    bool llamaStanding() const;
    world::Aabb llamaBox() const;
    void updateProjectiles(float dt);
    void updateTraps(float dt);
    void playerLaunch(const WeaponConfig& weapon, float spreadDegrees, int weaponType);
    bool pickUp(int id, int pickup);
    Pickup randomWeaponPickup(bool chest);
    Pickup randomConsumablePickup();
    AmmoType randomAmmoType();
    Rarity randomRarity(bool chest);
    void dropBotWeapon(int bot);  // al caer: todo su inventario queda en el suelo
    // Deja en el suelo lo que se ha sacado de una ranura (arma con sus balas, consumibles).
    void dropSlot(const InventorySlot& slot, const math3d::Vec3& where);
    // Bots con inventario: que les interesa recoger, que arma sacar y cuando curarse.
    bool botWants(const Bot& bot, const Pickup& pickup) const;
    int botReplacedSlot(const Bot& bot, const Pickup& pickup) const;
    void botEquip(Bot& bot, float distance);
    void botUpdateHealing(int index, float dt);
    // IA de botin, caza y construccion (match_bots.cpp).
    bool findLoot(int index, float maxDistance, bool healsOnly = false);
    void botTakeLoot(int index);
    void botBuildCover(int index, int attacker, bool urgent = false);
    void hearShot(int shooter, const math3d::Vec3& origin);
    float botAttackRange(const Bot& bot) const;
    void damageWall(int wall, float amount, int attacker, const math3d::Vec3& point);
    void addDamageNumber(const math3d::Vec3& position, float amount, bool structure, bool headshot = false,
                         bool shield = false);
    // Objeto del mapa al que pertenece una caja fija (o -1).
    int objectFromCollider(int collider) const;
    // IA v0.13: materiales, picar y huir con poca vida (match_bots.cpp).
    bool botWeak(const Bot& bot) const;
    // Material con el que el bot puede construir una pieza (el que mas tenga), o false.
    bool botPickMaterial(const Bot& bot, world::Resource* out) const;
    bool findHarvest(int index);
    void botHarvest(int index, float dt);
    void startFlee(int index, int threat);
    bool fleePoint(const Bot& bot, const math3d::Vec3& threat, math3d::Vec3* out);
    bool botHasHeals(const Bot& bot) const;
    // Equipos (v0.16, match_teams.cpp): derribar, desangrarse y reanimar, y que los bots
    // vayan juntos y se levanten unos a otros.
    bool teamStanding(int id) const;     // algun companero suyo en pie (ni el ni los derribados)
    void goDown(int victim, int attacker);
    void updateDowned(float dt);
    bool markRevive(int rescuer, int mate);
    void revive(int id, int rescuer);
    void updateDownedBot(int index, float dt);
    int downedMate(int index, float range) const;
    void startBotRevive(int index, int mate);
    void updateBotRevive(int index);
    int teamLeader(int id) const;
    void splitIntoTeams();
    void teamLanding();
    void followPlayerJump(int index);
    void releaseLootClaims(Bot& b);
    // v0.14: tacticas de los bots (match_bots.cpp): acampar airdrops, estructuras pequenas
    // (caja de paredes, rampa) y final de partida.
    bool lateGame() const;
    bool findCamp(int index);
    void startSmallBuild(int index, int kind, float yaw);
    void updateSmallBuild(int index, float dt);
    bool botPlacePiece(int index, PieceType type, float yaw);
    // Modo zombi (match_zombies.cpp).
    void startZombies();
    void updateZombieMode(float dt);
    void updateZombie(int index, float dt);
    void decideZombie(int index);
    void updateSurvivor(int index, float dt);
    void decideSurvivor(int index);
    // Slot del zombi (indice de bot) o -1. `near`: alrededor de ese punto (horda, guardianes).
    int spawnZombie(ZombieType type, const math3d::Vec3* near, float nearRadius = 7.0f);
    ZombieType rollZombieType();
    void spawnHorde();
    void zombieDrops(int index);
    void applyMapState();
    // v0.15 (match_zombies.cpp / zombie_save.cpp).
    void updateNeeds(float dt);
    void updateSleep(float dt);
    void updateSites();
    void spawnSiteGuards(int site);
    bool updateAllyDuty(int index);
    bool allyBuildStep(int index);
    // v0.16: la IA de los aliados (match_allies.cpp).
    bool allyBedtime(const Bot& b) const;
    void allyGoToBed(int index);
    void allyLieDown(int index);
    void allyFollow(int index);
    bool allyStartClimb(int index);
    void allyClimbStep(int index);
    bool allyGoTo(Bot& b, const math3d::Vec3& spot);
    bool allyTasks(int index);
    bool allyLootTask(int index);
    bool allyCraftTask(int index);
    bool allyHarvestTask(int index);
    bool allyFortify(int index);
    bool allyPlacePiece(int index, WallPlacement placement, float healthScale, bool mustPay, bool ownFirst, int* wall);
    int allyRecipe(const Bot& b) const;
    void allyCraft(int index, int recipeIndex);
    void turnFallen(int index);
    void updateFallen(int index, float dt, float targetDistance);
    void restockContainers();
    void newObjective(int slot);
    void objectiveEvent(ObjectiveType type, int amount = 1);
    void updateObjectives(float dt);
    void grantReward(RewardType reward);
    int pieceKeyNear(const math3d::Vec3& p) const;
    bool zombiesNear(const math3d::Vec3& p, float range) const;
    MoveIntent avoidObstacles(int index, math3d::Vec3 desired, float speed, MoveIntent intent);
    void onStructuresChanged();
    void pushEvent(GameEventType type, int entity, int other, const math3d::Vec3& position);
    world::Aabb bodyBounds(int id) const;
    void spawnBots(const math3d::Vec3& playerSpawn);
    void updatePlayer(float dt);
    void updatePlayerItems(float dt, bool reload);
    void cancelUse();
    void harvest(int index, const math3d::Vec3& point);
    void updateBot(int index, float dt);
    void decide(int index);
    int perceive(int index) const;
    void chooseDestination(Bot& bot);
    // Fuera de la zona siguiente con la tormenta cerca: lo primero es entrar.
    bool botNeedsSafety(const math3d::Vec3& pos, int self) const;
    bool safePoint(math3d::Vec3* out);
    // Direccion por la que bajar de lo que hay bajo los pies (hacia el apoyo mas bajo).
    math3d::Vec3 descentDirection(const math3d::Vec3& pos) const;
    bool requestPath(Bot& bot, const math3d::Vec3& to);
    MoveIntent steer(int index, float dt);
    void botShoot(int index);
    void separateCharacters();
    void updateStorm(float dt);
    void eliminate(int victim, int killer);
    void addTracer(const math3d::Vec3& from, const math3d::Vec3& to, bool byPlayer, bool hit);

    MatchConfig m_config;
    world::CollisionWorld* m_world = nullptr;
    NavGrid* m_nav = nullptr;
    std::vector<math3d::Vec3> m_pois;
    core::Random m_rng;
    StormZone m_storm;
    core::Random m_stormRng;

    CharacterBody m_player;
    PlayerInput m_input;
    PlayerAim m_aim;
    PlayerCommands m_commands;
    bool m_pendingJump = false;
    bool m_pendingPlace = false;
    PieceType m_buildPiece = PieceType::Wall;
    world::Resource m_buildMaterial = world::Resource::Wood;
    bool m_playerCrouched = false;
    int m_playerHeadshots = 0;
    bool m_pendingRemove = false;
    bool m_pendingReload = false;
    bool m_pendingPickaxe = false;
    bool m_pendingUse = false;
    int m_pendingCycle = 0;
    int m_pendingSelect = -1;
    int m_pendingSwapPickup = -1, m_pendingSwapSlot = -1;
    float m_recoilBloom = 0.0f;  // grados de dispersion por el retroceso de los ultimos disparos
    bool m_buildMode = false;
    Inventory m_inventory;
    Materials m_materials;
    int m_useSlot = -1;          // ranura del consumible en uso (-1: ninguno)
    float m_useTimer = 0.0f, m_useDuration = 1.0f;
    UseError m_useError = UseError::None;
    float m_sinceUseError = 99.0f;
    float m_swingCooldown = 0.0f;
    const std::vector<world::Harvestable>* m_harvestables = nullptr;
    std::vector<short> m_harvestByCollider;
    std::vector<float> m_objectHealth;
    std::vector<u8> m_objectDestroyed;
    StructureSystem m_structures;
    unsigned m_seenStructureRevision = 0;
    DamageNumber m_damageNumbers[kMaxDamageNumbers];
    GameEvent m_events[kMaxEvents];
    u32 m_eventCount = 0;
    int m_nextDamageNumber = 0;
    bool m_playerAiming = false;
    int m_playerKills = 0;
    int m_assistTarget = -1;

    std::vector<Bot> m_bots;
    char m_names[kMaxBots + 1][16];

    MatchPhase m_phase = MatchPhase::Playing;
    int m_alive = 0;
    int m_winner = -1;
    int m_playerPlace = 0;
    float m_time = 0.0f;
    int m_pathBudget = 0;  // caminos A* que aun se pueden calcular en este paso

    KillEvent m_killFeed[kMaxKillFeed];
    int m_killFeedCount = 0;
    Tracer m_tracers[kMaxTracers];
    int m_nextTracer = 0;
    float m_sincePlayerHit = 99.0f;
    float m_sincePlayerDamaged = 99.0f;

    MatchStats m_stats;
    bool m_inStorm[kMaxBots + 1] = {};
    float m_stormTimer[kMaxBots + 1] = {};
    AirState m_air[kMaxBots + 1] = {};
    std::vector<Pickup> m_pickups;
    std::vector<Container> m_containers;
    int m_nextAirdrop = 0;
    std::vector<math3d::Vec3> m_lootSpots;  // usados al repartir (separacion minima)
    Projectile m_projectiles[kMaxProjectiles];
    Explosion m_explosions[kMaxExplosions];
    int m_nextExplosion = 0;
    float m_throwCooldown = 0.0f;
    int m_llama = -1;
    int m_damageSource = -1;  // ItemType de lo que esta haciendo dano ahora (-1: pico, tormenta...)
    Trap m_traps[kMaxTraps];
    int m_trapStock = 0;
    bool m_pendingTrap = false;
    // Equipos (v0.16): equipo de cada entidad, derribados, lo que pierden por segundo, quien
    // les derribo, cuantas veces han caido y quien les esta reanimando (cada paso lo confirma).
    u8 m_team[kMaxBots + 1] = {};
    bool m_downed[kMaxBots + 1] = {};
    float m_bleedRate[kMaxBots + 1] = {};
    short m_downedBy[kMaxBots + 1] = {};
    u8 m_downCount[kMaxBots + 1] = {};
    short m_reviver[kMaxBots + 1] = {};
    float m_reviveTime[kMaxBots + 1] = {};
    bool m_reviveSeen[kMaxBots + 1] = {};
    int m_playerReviving = -1;
    ShotObserver m_shotObserver = nullptr;
    void* m_shotObserverContext = nullptr;
    // Modo zombi.
    float m_zClock = 0.0f;     // s dentro del dia o de la noche actual
    int m_day = 1;
    bool m_night = false;
    int m_nightsSurvived = 0;
    float m_spawnTimer = 0.0f;
    int m_zombieKills = 0;
    int m_itemsCrafted = 0;
    bool m_bpFound[kBlueprintCount] = {};
    bool m_bpDone[kBlueprintCount] = {};
    short m_bpWall[kBlueprintCount][kMaxBlueprintPieces];
    // v0.15.
    float m_food = 100.0f, m_rest = 100.0f;
    int m_hungerStage = 0, m_tiredStage = 0;   // ultimo aviso dado (0: ninguno)
    float m_needsTimer = 0.0f;                 // dano por hambre / agotamiento
    int m_sleeping = 0;
    int m_sleepBed = -1;
    float m_fumbleTimer = 20.0f;
    float m_stumbleTimer = 0.0f;               // > 0: tropieza (mas lento)
    bool m_siteFound[world::zsites::kSiteCount] = {};
    int m_siteGuardDay[world::zsites::kSiteCount] = {};
    Objective m_objectives[kActiveObjectives];
    int m_objectivesDone = 0;
    ObjectiveType m_lastObjective = ObjectiveType::Count;
    int m_rescuedTotal = 0;
    float m_allyBuildTimer = 0.0f;
    float m_sleepRested = 0.0f;                // descanso ganado en este sueno (objetivo)
};

// Duena de las piezas de los planos: no cuentan en el limite de piezas del jugador.
constexpr int kBlueprintOwner = -5;

}  // namespace game
