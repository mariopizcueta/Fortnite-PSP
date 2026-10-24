// Parametros configurables de la partida (bots, IA, armas, zona).
#pragma once

#include "game/inventory.h"
#include "game/structures.h"
#include "game/weapon.h"
#include "math/math3d.h"

#include <psptypes.h>

namespace game {

// Capacidad maxima. La partida empieza con 10 bots (MatchConfig::botCount); 30 solo
// tras medir el rendimiento en la PSP (ver docs/BOTS.md).
constexpr int kMaxBots = 30;
constexpr int kDefaultBots = 10;

struct MatchConfig {
    int botCount = kDefaultBots;
    u32 seed = 1;

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

    // Inventario inicial (como en Fortnite: nada, solo el pico).
    struct StartItem {
        ItemType type;
        int count;
        Rarity rarity;
    };
    StartItem startItems[Inventory::kSlots] = {};
    int startMaterials[static_cast<int>(world::Resource::Count)] = {0, 0, 0};  // madera, piedra, metal
    int startAmmo[static_cast<int>(AmmoType::Count)] = {0, 0, 0, 0};

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
        c.startItems[0] = {ItemType::AssaultRifle, 1, Rarity::Legendary};
        c.startItems[1] = {ItemType::Bandage, 5, Rarity::Common};
        c.startItems[2] = {ItemType::SmallShield, 3, Rarity::Uncommon};
        c.startMaterials[0] = 30;
        c.startAmmo[static_cast<int>(AmmoType::Medium)] = 120;
        return c;
    }
};

}  // namespace game
