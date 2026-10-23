// Parametros configurables de la partida (bots, IA, armas, zona).
#pragma once

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
    float attackRange = 26.0f;    // m: por encima se acerca antes de disparar
    float reactionTime = 0.35f;   // s desde que ve un objetivo hasta el primer disparo

    // Armas (disparo instantaneo por raycast). El jugador lleva la SCAR legendaria:
    // 36 de dano, 30 balas, 5.5 disparos/s, 2 s de recarga (WeaponConfig por defecto).
    WeaponConfig playerWeapon;
    WeaponConfig botWeapon = botRifle();
    float botSpreadDegrees = 2.0f;        // dispersion base de los bots
    float botSpreadPerMeter = 0.07f;      // + por metro de distancia
    float aimAssistDegrees = 3.0f;        // ayuda de punteria del jugador (0 = ninguna)

    // Construccion (paredes de madera).
    BuildConfig build;

    // "Zona" simplificada: los bots exploran dentro de un circulo que se cierra con el
    // tiempo, asi la partida converge hacia el deposito y termina.
    math3d::Vec3 zoneCenter{0.0f, 0.0f, -6.0f};
    float zoneStartRadius = 85.0f;
    float zoneEndRadius = 22.0f;
    float zoneShrinkSeconds = 240.0f;

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
};

}  // namespace game
