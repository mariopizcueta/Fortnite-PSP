// Elementos de interfaz: atlas de iconos/rotulos/banners y primitivas 2D.
#pragma once

#include <psptypes.h>

#include "assets/ui_atlas.h"
#include "core/i18n.h"
#include "gfx/texture.h"

namespace app {
namespace ui {

class Atlas {
public:
    bool load();
    void destroy();
    bool ready() const { return m_texture.pixels != nullptr; }
    size_t bytes() const { return gfx::textureBytes(m_texture); }

    // Dibuja un elemento del atlas con la esquina superior izquierda en (x, y).
    void draw(const assets::ui::Rect& r, float x, float y, float scale = 1.0f,
              u32 color = 0xFFFFFFFF) const;
    // Igual, centrado horizontalmente en `cx`.
    void drawCentered(const assets::ui::Rect& r, float cx, float y, float scale = 1.0f,
                      u32 color = 0xFFFFFFFF) const;
    // Sin escalar, recortado al rectangulo [x0, x1) x [y0, y1) (p. ej. el del minimapa).
    void drawClipped(const assets::ui::Rect& r, float x, float y, float x0, float y0, float x1, float y1,
                     u32 color = 0xFFFFFFFF) const;

private:
    gfx::Texture m_texture;
};

// Marco de `thickness` pixeles alrededor del rectangulo.
void frame(float x, float y, float w, float h, float thickness, u32 color);

// --- Ayudas de controles con los botones de la PSP (Recursos/Texturas PSP) ---------------

enum class Button : u8 {
    Cross, Circle, Square, Triangle, L, R, Start, Select, Up, Down, Left, Right, Dpad, Stick,
    // Acciones de la partida que se pueden reasignar en los ajustes (parche, fase 8): se
    // dibujan con el boton que tengan asignado (setActionButton).
    Jump, Build, Interact, Pickaxe, Fire, Aim, Crouch, Emote,
    Count
};
constexpr int kActionCount = 8;
// Boton fisico de una accion (Button::Jump...). Lo fija la aplicacion con los ajustes.
void setActionButton(Button action, Button physical);
Button resolve(Button button);

// Un boton y lo que hace (texto traducido al dibujar; `raw`, un texto ya hecho). Con texto
// vacio, el siguiente boton va pegado (combinaciones como L + R + START o izq./dcha.).
struct Hint {
    Button button;
    i18n::T text;
    const char* raw = nullptr;

    const char* label() const { return raw ? raw : i18n::tr(text); }
};

const assets::ui::Rect& buttonRect(Button button);

// Ancho en pixeles de una fila de ayudas (para centrarla).
int hintsWidth(const Hint* hints, int count);
// Los iconos van en la parte GU del frame y el texto en la de CPU (despues de endFrame):
// las dos funciones calculan la misma disposicion. `y` es la linea del texto (8 px de
// alto); los iconos se centran con ella.
void drawHintIcons(const Atlas& atlas, int x, int y, const Hint* hints, int count, u32 tint = 0xFFFFFFFF);
void printHints(int x, int y, u32 color, const Hint* hints, int count);

}  // namespace ui
}  // namespace app
