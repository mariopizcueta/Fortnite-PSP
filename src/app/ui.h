// Elementos de interfaz: atlas de iconos/rotulos/banners y primitivas 2D.
#pragma once

#include <psptypes.h>

#include "assets/ui_atlas.h"
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

private:
    gfx::Texture m_texture;
};

// Marco de `thickness` pixeles alrededor del rectangulo.
void frame(float x, float y, float w, float h, float thickness, u32 color);

}  // namespace ui
}  // namespace app
