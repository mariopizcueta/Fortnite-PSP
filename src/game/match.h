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
    bool togglePickaxe = false;  // TRIANGULO: sacar / guardar el pico
    bool toggleCrouch = false;   // cruceta abajo: agacharse / levantarse
    bool useItem = false;    // R pulsado con un consumible en la mano
    // Inventario lleno: cambiar el objeto del suelo `swapPickup` por la ranura `swapSlot`
    // (el jugador la elige en el selector del HUD). -1: nada.
    int swapPickup = -1;
    int swapSlot = -1;
};

// Por que no se ha podido usar un consumible.
enum class UseError : u8 { None, FullHealth, FullShield };
const char* useErrorName(UseError error);

// Sucesos de la partida para sonido y animaciones. La partida los apila en una cola
// circular; quien los consume guarda hasta que numero ha leido (eventCount()).
enum class GameEventType : u8 {
    Shot,          // entity = tirador, position = boca del arma
    HitEntity,     // entity = alcanzado, other = tirador
    HitStructure,  // other = tirador, position = impacto en la pared construida
    PlayerHurt,    // entity = 0, other = atacante
    Eliminated,    // entity = victima, other = quien la elimino
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
};

// Fase de la caida desde el autobus de cada personaje.
enum class AirState : u8 { Bus, Freefall, Glide, Ground };
const char* airStateName(AirState state);

// --- Botin (docs/BOTIN.md) ----------------------------------------------------------------

// Objeto en el suelo: un arma o consumible (con su rareza), municion o materiales de
// construccion (los sueltan los bots al caer y la llama; se recogen solos al pasar).
enum class PickupKind : u8 { Item, Ammo, Materials };
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
// se rompe a disparos o con el pico y suelta materiales, municion y objetos.
enum class ContainerKind : u8 { Chest, AmmoBox, Airdrop, Llama };
struct Container {
    ContainerKind kind = ContainerKind::Chest;
    math3d::Vec3 position;
    float yaw = 0.0f;
    bool opened = false;
    float openedAt = 0.0f;  // s de partida en que se abrio (se ve abierto un instante y desaparece)
    bool landed = true;   // airdrop: false mientras baja con el globo
    int claimedBy = -1;
    float health = 0.0f;  // llama: vida antes de romperse (las demas no se rompen)
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
struct Interactable {
    enum class Kind : u8 { None, Pickup, Container } kind = Kind::None;
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
    void damageContainer(int index, float amount, int by, const math3d::Vec3& point);
    // Cajas de la llama (para disparos, pico y explosiones) y si la llama esta en pie.
    bool llamaStanding() const;
    world::Aabb llamaBox() const;
    void updateProjectiles(float dt);
    void playerLaunch(const WeaponConfig& weapon, float spreadDegrees, int weaponType);
    bool pickUp(int id, int pickup);
    Pickup randomWeaponPickup(bool chest);
    Pickup randomConsumablePickup();
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
    bool findLoot(int index, float maxDistance);
    void botTakeLoot(int index);
    void botBuildCover(int index, int attacker);
    void hearShot(int shooter, const math3d::Vec3& origin);
    float botAttackRange(const Bot& bot) const;
    void damageWall(int wall, float amount, int attacker, const math3d::Vec3& point);
    void addDamageNumber(const math3d::Vec3& position, float amount, bool structure, bool headshot = false);
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
    ShotObserver m_shotObserver = nullptr;
    void* m_shotObserverContext = nullptr;
};

}  // namespace game
