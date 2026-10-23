#include "app/ui.h"

#include "assets/embedded_assets.h"
#include "gfx/graphics.h"

namespace app {
namespace ui {

bool Atlas::load() {
    const assets::Blob png = assets::uiAtlasPng();
    return gfx::loadPngFromMemory(m_texture, png.data, png.size);
}

void Atlas::destroy() { gfx::destroyTexture(m_texture); }

void Atlas::draw(const assets::ui::Rect& r, float x, float y, float scale, u32 color) const {
    gfx::drawTextureRect(m_texture, r.x, r.y, r.w, r.h, x, y, r.w * scale, r.h * scale, color);
}

void Atlas::drawCentered(const assets::ui::Rect& r, float cx, float y, float scale, u32 color) const {
    draw(r, cx - r.w * scale * 0.5f, y, scale, color);
}

void frame(float x, float y, float w, float h, float t, u32 color) {
    gfx::fillRect(x, y, w, t, color);
    gfx::fillRect(x, y + h - t, w, t, color);
    gfx::fillRect(x, y + t, t, h - 2 * t, color);
    gfx::fillRect(x + w - t, y + t, t, h - 2 * t, color);
}

}  // namespace ui
}  // namespace app
