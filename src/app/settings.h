// Ajustes del jugador (parche, fase 8; docs/AJUSTES.md): graficos, sensibilidad, controles
// e idioma. Se guardan en ajustes.txt junto al EBOOT ("clave=valor", como perfil.txt) con
// el mismo guardado seguro, y al cargar se valida cada valor (fuera de rango o un fichero
// roto: el valor por defecto). Borrar los datos del jugador no toca los ajustes.
//
// Todos tienen efecto real y se aplican al momento:
//   - distancia de dibujado: hasta donde llega la niebla (y con ella lo que se dibuja);
//   - detalle de personajes: a partir de que distancia los bots usan el modelo sencillo;
//   - numeros de dano, trazadoras y contador de FPS: se ven o no;
//   - VSync: sincronia vertical del cambio de buffer;
//   - sensibilidad de la camara y de la mirilla, zona muerta del stick e invertir el eje Y;
//   - controles: el boton de cada accion de la partida (sin repetir botones).
#pragma once

#include <psptypes.h>

#include "core/i18n.h"
#include "game/match_config.h"

namespace app {

// Acciones de la partida que se pueden reasignar (mismo orden que ui::Button::Jump...).
enum class Action : u8 { Jump, Build, Interact, Pickaxe, Fire, Aim, Crouch, Emote, Count };
constexpr int kActionCount = static_cast<int>(Action::Count);

// Trucos del menu de administrador (v0.13; ajustes > admin, con contrasena): clave en
// ajustes.txt, textos del mod menu y el interruptor en game::Cheats.
struct CheatDef {
    const char* key;
    i18n::T name;
    i18n::T description;
    bool game::Cheats::*flag;
};
int cheatCount();
const CheatDef& cheatDef(int index);

// Botones que se pueden asignar a las acciones: los 8 de la partida. Fijos (no se
// reasignan): cruceta izquierda/derecha (objeto o pieza), SELECT (mapa), START (pausa), el
// stick y la combinacion de salida L+R+START (siempre con los botones fisicos).
constexpr int kRemappableCount = 8;
extern const u32 kRemappableButtons[kRemappableCount];

struct Settings {
    i18n::Lang language = i18n::Lang::Es;
    int drawDistance = 1;     // 0 cerca, 1 media (la de siempre), 2 lejos
    int characterDetail = 1;  // 0 bajo, 1 medio (la de siempre), 2 alto
    bool damageNumbers = true;
    bool tracers = true;
    bool showFps = true;
    bool vsync = true;
    int cameraSensitivity = 5;  // 1..10 (5 = la de siempre)
    int scopeSensitivity = 5;   // 1..10
    int deadzone = 20;          // % del recorrido del stick (10..35)
    bool invertY = false;
    u32 buttons[kActionCount];  // PSP_CTRL_* de cada accion
    // Menu de administrador: desbloqueado con la contrasena y los trucos activos (se guardan).
    bool admin = false;
    game::Cheats cheats;

    Settings() { resetControls(); }
    void resetControls();
    // Asigna `button` a la accion; si otra accion lo tenia, se intercambian (nunca hay dos
    // acciones en el mismo boton).
    void assign(Action action, u32 button);
    u32 button(Action action) const { return buttons[static_cast<int>(action)]; }
    bool controlsValid() const;

    // Niebla y distancia de dibujado (m) segun drawDistance.
    float fogStart() const;
    float fogEnd() const;
    // Distancia a partir de la que los bots usan el modelo de pocos poligonos.
    float lodDistance() const;
    // Multiplicadores de giro de la camara (apuntando con el stick) y con la mirilla.
    float cameraScale() const { return 0.4f + 0.12f * static_cast<float>(cameraSensitivity); }
    float scopeScale() const { return 0.4f + 0.12f * static_cast<float>(scopeSensitivity); }

    bool load(const char* path);
    bool save(const char* path) const;
};

// Nombre corto de un boton para guardarlo ("X", "CIRCULO", "L"...) y al reves (0 si no).
const char* buttonKey(u32 mask);
u32 buttonFromKey(const char* key);

}  // namespace app
