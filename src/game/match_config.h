// Parametros configurables de la partida (bots, IA, armas, zona).
#pragma once

#include "game/inventory.h"
#include "game/structures.h"
#include "game/weapon.h"
#include "game/zombies.h"
#include "math/math3d.h"

#include <psptypes.h>

namespace game {

// Capacidad maxima. v0.14: las partidas empiezan con 30 bots (el usuario los ha medido en
// su PSP-2000). Con menos de kRewardBots la partida es de practica: ni pavos ni logros.
// v0.16: en escuadrones hasta 39 bots (40 personajes: 10 equipos de 4); en solitario siguen
// siendo como mucho 30.
constexpr int kMaxBots = 39;
constexpr int kMaxSoloBots = 30;
constexpr int kDefaultBots = 30;
constexpr int kRewardBots = 30;
constexpr int kDuoBots = 29;    // 30 personajes: 15 parejas
constexpr int kSquadBots = 39;  // 40 personajes: 10 equipos de 4

// Modos de juego (CUADRADO en el lobby). Monodisparo: en el botin solo hay francotiradores,
// armas con mirilla y granadas (y curativos); ni trampas ni el resto de armas. Zombis
// (v0.14, docs/ZOMBIS.md): supervivencia en el mapa en ruinas, sin tormenta ni bots.
enum class GameMode : u8 { BattleRoyale, OneShot, Zombies, Count };
// Submodos del battle royale (v0.16, docs/EQUIPOS.md): en solitario, por parejas o en
// escuadrones de 4. Los equipos son bots, tambien los del jugador.
enum class TeamMode : u8 { Solo, Duo, Squad, Count };
inline int teamSizeOf(TeamMode mode) { return mode == TeamMode::Squad ? 4 : (mode == TeamMode::Duo ? 2 : 1); }
inline int teamBotsOf(TeamMode mode) { return mode == TeamMode::Squad ? kSquadBots : kDuoBots; }

// Trucos del menu de administrador (ajustes > admin, con contrasena). Los marcados (app) los
// aplica la aplicacion al dibujar; el resto, la partida. Salvo los de los bots, todos son
// para el jugador.
struct Cheats {
    bool godMode = false;            // no recibe dano
    bool infiniteAmmo = false;       // el cargador no se gasta (ni las granadas)
    bool noReload = false;           // la recarga es instantanea
    bool rapidFire = false;          // cadencia x3
    bool noSpread = false;           // sin dispersion ni retroceso
    bool aimbot = false;             // cada disparo va a la cabeza del rival visible mas centrado
    bool oneShot = false;            // dano x10 (balas, explosiones y pico)
    bool explosiveAmmo = false;      // cada disparo explota al impactar
    bool wallbang = false;           // las balas atraviesan paredes y objetos
    bool fly = false;                // vuelo: saltar sube, agacharse baja
    bool superSpeed = false;         // corre al doble
    bool superJump = false;          // salta tres veces mas alto
    bool lowGravity = false;         // gravedad lunar
    bool infiniteMaterials = false;  // construir no gasta material (ni trampas)
    bool invisible = false;          // los bots no le ven ni le oyen
    bool noStorm = false;            // la tormenta no le hace dano
    bool freezeBots = false;         // los bots se quedan quietos y no disparan
    bool partyBots = false;          // (app) los bots se quedan bailando
    bool wallhack = false;           // (app) cajas de los rivales a traves de las paredes
    bool lootRadar = false;          // (app) cofres, llama y airdrops en el minimapa
    bool slowMotion = false;         // (app) tiempo bala: todo va a la mitad
    bool alwaysLlama = false;        // llama en todas las partidas (al empezar)
    bool legendaryKit = false;       // empieza con armas legendarias (al empezar)
};

struct MatchConfig {
    int botCount = kDefaultBots;
    u32 seed = 1;
    GameMode mode = GameMode::BattleRoyale;
    Cheats cheats;

    // Equipos (v0.16): personajes por equipo (1 = cada uno por su cuenta). Quien se queda sin
    // vida con un companero en pie cae derribado: se arrastra a cuatro patas perdiendo vida
    // hasta que le reaniman o se acaba (downSeconds, menos cada vez que vuelve a caer).
    int teamSize = 1;
    float downSeconds = 30.0f;
    float downCrawlSpeed = 0.26f;   // fraccion de la carrera
    float reviveSeconds = 10.0f;    // como en Fortnite
    float reviveHealth = 30.0f;     // vida con la que se levanta (sin escudo)
    float reviveReach = 2.2f;
    float teamLeash = 22.0f;        // m: un bot mas lejos de su lider vuelve con el

    // IA: cada bot decide cada aiDecisionInterval s (escalonados entre bots); entre
    // decisiones solo sigue su plan (moverse, apuntar), que es barato.
    float aiDecisionInterval = 0.25f;
    float sightRange = 35.0f;     // m: distancia maxima para ver un objetivo
    float fovDegrees = 120.0f;    // campo de vision (buscando: 360)
    float attackRange = 26.0f;    // m: por encima se acerca antes de disparar (fusil)
    float reactionTime = 0.35f;   // s desde que ve un objetivo hasta el primer disparo
    float hearingRange = 50.0f;   // m: los bots oyen los disparos y van a mirar
    float huntRange = 80.0f;      // m: sin objetivo, a veces van a por el rival mas cercano

    // Armas (disparo instantaneo por raycast). Las del botin salen de weaponConfig()
    // (inventory.h). botWeapon solo se usa con botsStartArmed (pruebas de fases anteriores).
    WeaponConfig botWeapon = botRifle();
    bool botsStartArmed = false;          // false: los bots aterrizan sin arma y van a por botin
    float botDamageScale = 0.55f;         // dificultad: fraccion del dano real del arma
    float botSpreadDegrees = 2.0f;        // dispersion base de los bots
    float botSpreadPerMeter = 0.07f;      // + por metro de distancia
    float aimAssistDegrees = 1.0f;        // ayuda de punteria del jugador (0 = ninguna)
    // Con el francotirador y L (mirilla), algo mas de ayuda que con el resto. Era de 4 grados:
    // con el zoom (15 grados de campo) eso desviaba el disparo un cuarto de pantalla del
    // centro de la mira; con 2, el impacto queda cerca de la reticula.
    float sniperAssistDegrees = 2.0f;
    float headshotMultiplier = 2.0f;      // impacto en la cabeza (el ultimo 0.33 m del cuerpo)
    float botHeadshotChance = 0.25f;      // de los bots al jugador solo cuenta 1 de cada 4 (poco probable)
    float crouchSpeed = 0.55f;            // agachado: fraccion de la velocidad normal
    float crouchedSpreadPenalty = 1.5f;   // grados extra de los bots contra el jugador agachado
    // Precision del jugador agachado: la dispersion propia del arma se multiplica por su
    // WeaponConfig::crouchSpreadScale (0 = sin bonificacion, 1 = la de cada arma). El
    // retroceso acumulado no se reduce.
    float crouchAccuracyBonus = 1.0f;
    bool weaponSpread = true;             // dispersion de las armas del jugador (ver docs/BOTIN.md)

    // Construccion (paredes de madera). Los bots levantan una pared cuando les disparan.
    BuildConfig build;
    bool botsBuild = true;
    float botWallCooldown = 9.0f;  // s entre paredes de un mismo bot
    // Materiales de los bots (v0.13): aterrizan sin nada; cada pieza les cuesta como al
    // jugador y, cuando les faltan y no hay nadie cerca, pican objetos del mapa hasta tener
    // botHarvestGoal. Con botsFreeBuild (reglas clasicas de las pruebas) construyen gratis.
    bool botsHarvest = true;
    bool botsFreeBuild = false;
    int botHarvestBelow = 40;   // por debajo de esto (en total) van a picar
    int botHarvestGoal = 120;   // ...hasta tener esto
    float botHarvestRange = 28.0f;
    // Con poca vida (vida + escudo por debajo de botFleeHealth) evitan el combate: huyen a
    // donde no les vean, se cubren con una pared y se curan; si no llevan curas, las buscan.
    // Solo pelean si el rival esta encima (botCorneredRange).
    bool botsFlee = true;
    float botFleeHealth = 45.0f;
    float botRecoveredHealth = 75.0f;
    float botCorneredRange = 5.0f;
    // Las balas tambien rompen los objetos del mapa (paneles de las naves, cajas,
    // contenedores...): fraccion del dano del disparo que se aplica (0 = no los rompen).
    float shotObjectDamageScale = 1.0f;

    // Inventario inicial (como en Fortnite: nada, solo el pico).
    struct StartItem {
        ItemType type;
        int count;
        Rarity rarity;
    };
    StartItem startItems[Inventory::kSlots] = {};
    int startMaterials[static_cast<int>(world::Resource::Count)] = {0, 0, 0};  // madera, piedra, metal
    int startAmmo[static_cast<int>(AmmoType::Count)] = {};

    // Autobus de batalla: quieto en el cielo; se salta con X (o al acabar el tiempo).
    bool startInBus = true;
    math3d::Vec3 busPosition{0.0f, 90.0f, 8.0f};
    float busSeconds = 25.0f;        // salto automatico
    float freefallSpeed = 13.0f;     // m/s en horizontal
    float freefallFallSpeed = 20.0f;
    float glideSpeed = 9.0f;
    float glideFallSpeed = 6.0f;
    float glideAutoHeight = 26.0f;   // m sobre el suelo: el ala delta se abre sola

    // Botin (docs/BOTIN.md): cofres, objetos en el suelo, cajas de municion y airdrops.
    bool loot = true;
    int chestCount = 22;
    int floorLootCount = 55;
    int ammoBoxCount = 16;
    float airdropTimes[2] = {75.0f, 165.0f};  // s de partida en que cae un airdrop
    float airdropFallSpeed = 3.5f;
    // Llama de suministros: se tira UNA vez al empezar cada partida (con su semilla).
    float llamaChance = 0.05f;
    bool forceLlama = false;   // pruebas: siempre hay llama
    float llamaHealth = 250.0f;
    // Los bots sueltan materiales al caer (madera siempre; piedra y metal a veces).
    bool botsDropMaterials = true;

    // Trampas de pinchos: dano a cada personaje dentro al saltar, espera hasta poder
    // volver a saltar, alcance para colocarlas y cuantas se pueden llevar.
    float trapDamage = 100.0f;
    float trapCooldown = 3.0f;
    float trapReach = 6.0f;
    int maxTrapsCarried = 9;

    // Consumibles: segundos de uso de cada uno (0 = el de ItemDef: vendas 3.5, botiquin 10,
    // minipocion 2, pocion de escudo 5). Mientras se usa no se puede mover ni saltar.
    float useSeconds[static_cast<int>(ItemType::Count)] = {};
    // Recibir dano interrumpe el consumible (sin gastarlo). En Fortnite no: false.
    bool interruptUseOnDamage = false;

    // Pico (TRIANGULO): golpe cada swingInterval s mientras se mantiene R.
    float pickaxeInterval = 0.5f;
    float pickaxeReach = 2.2f;           // m desde el pecho
    float pickaxeDamage = 20.0f;         // a personajes
    float pickaxeStructureDamage = 50.0f;  // a paredes construidas
    float pickaxeObjectDamage = 50.0f;     // a los objetos del mapa (world/harvest.h)

    // Tormenta que se cierra (game/storm_zone.h): cada stormPhaseSeconds empieza un
    // cierre de stormShrinkSeconds hacia un circulo menor; fuera hace dano (sube por fase).
    bool stormCloses = true;
    float stormFirstClose = 60.0f;  // s (el autobus y la caida van antes)
    float stormPhaseSeconds = 40.0f;
    float stormShrinkSeconds = 20.0f;
    // Sin cierres (reglas clasicas), "zona" simplificada: los bots exploran dentro de un
    // circulo que se reduce con el tiempo, asi la partida converge y termina.
    math3d::Vec3 zoneCenter{0.0f, 0.0f, -6.0f};
    float zoneStartRadius = 80.0f;  // dentro de la zona segura (la tormenta del borde, a 88 m)
    float zoneEndRadius = 22.0f;
    float zoneShrinkSeconds = 240.0f;

    // Tormenta del limite del mapa (world/storm.h): dano directo a la vida (no al escudo,
    // como en Fortnite) cada stormTickSeconds mientras se esta dentro.
    float stormDamage = 30.0f;
    float stormTickSeconds = 1.0f;

    bool invulnerablePlayer = false;  // solo para pruebas

    // v0.14: tacticas de los bots del battle royale. Acampan los airdrops que caen cerca
    // (como mucho botCampers por airdrop), levantan estructuras pequenas (caja de paredes al
    // recibir disparos, rampa para ganar altura contra quien esta mas alto) y, al final de la
    // partida (lateGameAlive vivos o menos), rotan antes a la zona y mantienen la posicion.
    bool botsCampAirdrops = true;
    int botCampers = 3;
    float botCampRange = 60.0f;
    bool botsSmallBuilds = true;
    int lateGameAlive = 8;

    // Modo zombi (GameMode::Zombies, ver zombies()).
    ZombieConfig zombie;

    static WeaponConfig botRifle() {
        WeaponConfig w;
        w.name = "Fusil";
        w.rarity = "Comun";
        w.damage = 10.0f;
        w.fireInterval = 0.45f;
        w.magazineSize = 30;
        w.reserveAmmo = -1;  // infinita
        w.reloadTime = 2.5f;
        w.range = 60.0f;
        return w;
    }

    // Supervivencia zombi (v0.14): sin autobus, sin tormenta que se cierre y sin bots del
    // battle royale; zombis y supervivientes (botCount = sus entidades), botin propio y
    // planos. El jugador despierta en la base (world/zombie_sites.h).
    static MatchConfig zombies() {
        MatchConfig c;
        c.mode = GameMode::Zombies;
        c.botCount = kZombieSlots + kSurvivorCount;
        c.startInBus = false;
        c.stormCloses = false;
        c.zoneStartRadius = c.zoneEndRadius = 300.0f;
        c.botsBuild = false;
        c.botsHarvest = false;
        c.botsFlee = false;
        c.botsCampAirdrops = false;
        c.botsSmallBuilds = false;
        c.airdropTimes[0] = c.airdropTimes[1] = 1.0e9f;
        c.llamaChance = 0.0f;
        c.chestCount = 18;
        c.floorLootCount = 46;
        c.ammoBoxCount = 14;
        // v0.15: sin limite de piezas (solo el tecnico): lo bonito es construir.
        c.build.maxWalls = kMaxWallCapacity;
        c.build.maxBotWalls = kMaxWallCapacity;
        c.build.maxWallsPerBot = 24;  // v0.16: los aliados se cubren con lo que construyen
        c.startItems[0] = {ItemType::SleepingBag, 1, Rarity::Uncommon};
        return c;
    }

    // Reglas de las fases 4-6 (para las pruebas de regresion): sin autobus ni botin, el
    // jugador empieza en el suelo con la SCAR, vendas, minipociones y 30 de madera; los
    // bots, con su fusil y dano completo; ayuda de punteria de 3 grados y sin dispersion.
    static MatchConfig classic() {
        MatchConfig c;
        c.stormCloses = false;
        c.startInBus = false;
        c.loot = false;
        c.botsStartArmed = true;
        c.botDamageScale = 1.0f;
        c.aimAssistDegrees = 3.0f;
        c.weaponSpread = false;
        c.botsBuild = false;
        c.botsHarvest = false;
        c.botsFreeBuild = true;
        c.botsFlee = false;
        c.botsCampAirdrops = false;
        c.botsSmallBuilds = false;
        c.lateGameAlive = 0;
        c.shotObjectDamageScale = 0.0f;
        c.startItems[0] = {ItemType::AssaultRifle, 1, Rarity::Legendary};
        c.startItems[1] = {ItemType::Bandage, 5, Rarity::Common};
        c.startItems[2] = {ItemType::SmallShield, 3, Rarity::Uncommon};
        c.startMaterials[0] = 30;
        c.startAmmo[static_cast<int>(AmmoType::Medium)] = 120;
        return c;
    }
};

}  // namespace game
