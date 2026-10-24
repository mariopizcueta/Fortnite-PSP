// Constantes globales del juego.
#pragma once

#include <pspctrl.h>
#include <psptypes.h>

namespace config {

constexpr const char* kGameTitle = "FortnitePSP";
constexpr const char* kVersion = "0.12.0-b";
constexpr const char* kPhase = "Fase 8 - parche (bloque B)";

// Reloj de CPU/bus en MHz (maximo estandar de la PSP).
constexpr int kCpuClockMhz = 333;
constexpr int kBusClockMhz = 166;

// Combinacion de salida: mantener L + R + START durante kExitHoldSeconds.
// No usa HOME: el boton HOME lo gestiona el sistema operativo.
constexpr u32 kExitCombo = PSP_CTRL_LTRIGGER | PSP_CTRL_RTRIGGER | PSP_CTRL_START;
constexpr float kExitHoldSeconds = 1.0f;

// Zona muerta radial del stick analogico (valor normalizado 0..1). Los sticks de la
// PSP suelen quedar descentrados hasta ~0.15 en reposo.
constexpr float kAnalogDeadzone = 0.2f;

// Camara 3D: proyeccion en perspectiva (metros).
constexpr float kFovYDegrees = 60.0f;
// 0.5 m: la camara se acerca a las paredes dentro de las naves y con 1 m se veria a
// traves de ellas. Precision de profundidad (16 bits) a 60 m: ~0.11 m.
constexpr float kNearPlane = 0.5f;
constexpr float kFarPlane = 140.0f;

// Niebla de distancia (metros desde la camara). Mas alla de kFogEnd todo es color de
// niebla, asi que esa distancia tambien es el limite de dibujado.
constexpr float kFogStart = 30.0f;
constexpr float kFogEnd = 95.0f;

// Colores en formato GU 0xAABBGGRR, muestreados de Referencias/.
// Cielo de "Referencias Primer Mapa" (Dusty Depot): #8CC2D8. Es a la vez el color de
// limpieza y el de la niebla, para que el horizonte se funda sin costura.
constexpr u32 kColorSky = 0xFFD8C28C;
// Hierba de las referencias del mapa: #4E5836.
constexpr u32 kColorGrass = 0xFF36584E;
// Dorado de "Referencias/Armas": #E4B037.
constexpr u32 kColorGold = 0xFF37B0E4;

}  // namespace config
