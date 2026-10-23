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
    bool placeWall = false;  // R pulsado en modo construccion
    bool removeWall = false; // CUADRADO pulsado en modo construccion
    bool reload = false;     // CUADRADO pulsado en modo combate
};

// Numero de dano flotante (como en Fortnite): solo los impactos del jugador.
struct DamageNumber {
    math3d::Vec3 position;
    float amount = 0.0f;
    float age = 99.0f;
    bool structure = false;  // impacto en una pared construida
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
    float zoneRadius() const;
    const MatchConfig& config() const { return m_config; }

    bool alive(int id) const;
    const CharacterBody& body(int id) const;
    const char* name(int id) const;
    int botCount() const { return static_cast<int>(m_bots.size()); }
    const Bot& bot(int i) const { return m_bots[i]; }
    bool playerAiming() const { return m_playerAiming; }
    bool buildMode() const { return m_buildMode; }
    const Weapon& playerWeapon() const { return m_playerWeapon; }
    const StructureSystem& structures() const { return m_structures; }
    // Donde iria una pared ahora mismo (para dibujar la pared fantasma).
    WallPlacement planWall(float yaw) const;
    // Pared construida bajo la mira y a alcance para quitarla, o -1.
    int targetedWall(const PlayerAim& aim) const;
    const DamageNumber* damageNumbers() const { return m_damageNumbers; }

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
    ShotResult playerShot(const PlayerAim& aim, float damage, float range);
    void applyDamage(int victim, float amount, int attacker);
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

    // Construccion (publico para las pruebas): coloca / quita como haria el jugador.
    int buildWall(float yaw);
    bool removeWallAt(const PlayerAim& aim);
    // Envia un bot a un destino (pruebas de navegacion).
    void commandBotTravel(int botIndex, const math3d::Vec3& destination);

private:
    CharacterBody& mutableBody(int id);
    ShotResult resolveShot(int shooter, const math3d::Vec3& origin, const math3d::Vec3& end, float damage);
    void damageWall(int wall, float amount, int attacker, const math3d::Vec3& point);
    void addDamageNumber(const math3d::Vec3& position, float amount, bool structure);
    void onStructuresChanged();
    world::Aabb bodyBounds(int id) const;
    void spawnBots(const math3d::Vec3& playerSpawn);
    void updatePlayer(float dt);
    void updateBot(int index, float dt);
    void decide(int index);
    int perceive(int index) const;
    void chooseDestination(Bot& bot);
    bool requestPath(Bot& bot, const math3d::Vec3& to);
    MoveIntent steer(int index, float dt);
    void botShoot(int index);
    void separateCharacters();
    void eliminate(int victim, int killer);
    void addTracer(const math3d::Vec3& from, const math3d::Vec3& to, bool byPlayer, bool hit);

    MatchConfig m_config;
    world::CollisionWorld* m_world = nullptr;
    NavGrid* m_nav = nullptr;
    std::vector<math3d::Vec3> m_pois;
    core::Random m_rng;

    CharacterBody m_player;
    PlayerInput m_input;
    PlayerAim m_aim;
    PlayerCommands m_commands;
    bool m_pendingJump = false;
    bool m_pendingPlace = false;
    bool m_pendingRemove = false;
    bool m_pendingReload = false;
    bool m_buildMode = false;
    Weapon m_playerWeapon;
    StructureSystem m_structures;
    unsigned m_seenStructureRevision = 0;
    DamageNumber m_damageNumbers[kMaxDamageNumbers];
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
    ShotObserver m_shotObserver = nullptr;
    void* m_shotObserverContext = nullptr;
};

}  // namespace game
