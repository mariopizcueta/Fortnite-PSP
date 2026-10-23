// Bot: personaje controlado por una IA de estados sencilla.
//
// Estados (docs/BOTS.md):
//   Explorar        elige un destino dentro de la zona y pide un camino
//   Desplazarse     sigue el camino; si se atasca, se aparta, salta o replanifica
//   BuscarObjetivo  se para y gira mirando alrededor (vision de 360 grados)
//   Atacar          encara al objetivo, se mueve de lado y dispara con dispersion
//   Eliminado       no actua, no colisiona y no se le puede disparar
//
// La IA "piensa" (percepcion + cambios de estado + caminos) solo cada
// MatchConfig::aiDecisionInterval segundos; en los pasos intermedios ejecuta el plan.
#pragma once

#include "game/character.h"
#include "game/weapon.h"

namespace game {

enum class BotState { Explore, Travel, Search, Attack, Eliminated };

const char* botStateName(BotState state);

constexpr int kMaxPathPoints = 48;

struct Bot {
    // Fisica: posicion, orientacion (yaw), velocidad y vida estan en body.state().
    CharacterBody body;

    BotState state = BotState::Explore;
    float decisionTimer = 0.0f;  // segundos hasta la proxima decision
    float stateTime = 0.0f;      // segundos en el estado actual

    // Objetivo.
    int target = -1;             // id de entidad (0 = jugador, 1..N = bots)
    bool targetVisible = false;
    math3d::Vec3 lastSeen;
    float lostTime = 0.0f;
    float reaction = 0.0f;       // espera antes del primer disparo
    Weapon weapon;               // mismo sistema de municion/cadencia/recarga que el jugador
    float strafeTimer = 0.0f;
    float strafeSign = 1.0f;
    float pathRefresh = 0.0f;    // tiempo hasta poder pedir otro camino hacia el objetivo

    // Camino.
    math3d::Vec3 path[kMaxPathPoints];
    int pathLength = 0;
    int pathIndex = 0;
    math3d::Vec3 destination;
    bool repath = false;         // la navegacion ha cambiado (paredes): pedir otro camino

    // Si el objetivo se esconde tras una pared construida, el bot la rompe a tiros.
    bool breakingWall = false;
    math3d::Vec3 wallPoint;

    // Busqueda.
    float searchYaw = 0.0f;
    float searchDuration = 0.0f;

    // Deteccion de atascos.
    math3d::Vec3 progressAnchor;
    float progressTimer = 0.0f;
    int stuckCount = 0;
    float sidestepTimer = 0.0f;
    float sidestepSign = 1.0f;
    bool wantsJump = false;

    // Marcador y aspecto.
    int kills = 0;
    float eliminatedAt = 0.0f;
    int palette = 0;
    bool aiming = false;  // pose de apuntar (para el dibujo)
};

}  // namespace game
