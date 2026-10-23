#include "app/ui.h"

#include "assets/embedded_assets.h"
#include "gfx/debug_text.h"
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

const assets::ui::Rect& buttonRect(Button button) {
    namespace a = assets::ui;
    switch (button) {
        case Button::Cross: return a::kBtnCross;
        case Button::Circle: return a::kBtnCircle;
        case Button::Square: return a::kBtnSquare;
        case Button::Triangle: return a::kBtnTriangle;
        case Button::L: return a::kBtnL;
        case Button::R: return a::kBtnR;
        case Button::Start: return a::kBtnStart;
        case Button::Select: return a::kBtnSelect;
        case Button::Up: return a::kBtnUp;
        case Button::Down: return a::kBtnDown;
        case Button::Left: return a::kBtnLeft;
        case Button::Right: return a::kBtnRight;
        case Button::Dpad: return a::kBtnDpad;
        case Button::Stick: return a::kBtnStick;
    }
    return a::kBtnCross;
}

namespace {

constexpr int kIconTextGap = 3;   // icono -> su texto
constexpr int kComboGap = 1;      // entre botones de una misma combinacion
constexpr int kHintGap = 10;      // entre ayudas

// Recorre la fila llamando a fn(indice, xIcono, xTexto).
template <typename Fn>
int layoutHints(int x, const Hint* hints, int count, Fn&& fn) {
    for (int i = 0; i < count; ++i) {
        const int iconX = x;
        x += buttonRect(hints[i].button).w;
        const bool hasText = hints[i].text && hints[i].text[0];
        if (hasText) x += kIconTextGap;
        fn(i, iconX, x);
        if (hasText) x += gfx::text::width(hints[i].text) + (i + 1 < count ? kHintGap : 0);
        else if (i + 1 < count) x += kComboGap;
    }
    return x;
}

}  // namespace

int hintsWidth(const Hint* hints, int count) {
    return layoutHints(0, hints, count, [](int, int, int) {});
}

void drawHintIcons(const Atlas& atlas, int x, int y, const Hint* hints, int count, u32 tint) {
    if (!atlas.ready()) return;
    layoutHints(x, hints, count, [&](int i, int iconX, int) {
        const assets::ui::Rect& r = buttonRect(hints[i].button);
        atlas.draw(r, static_cast<float>(iconX), static_cast<float>(y + 4 - r.h / 2), 1.0f, tint);
    });
}

void printHints(int x, int y, u32 color, const Hint* hints, int count) {
    layoutHints(x, hints, count, [&](int i, int, int textX) {
        if (hints[i].text && hints[i].text[0]) gfx::text::printShadow(textX, y, color, "%s", hints[i].text);
    });
}

void frame(float x, float y, float w, float h, float t, u32 color) {
    gfx::fillRect(x, y, w, t, color);
    gfx::fillRect(x, y + h - t, w, t, color);
    gfx::fillRect(x, y + t, t, h - 2 * t, color);
    gfx::fillRect(x + w - t, y + t, t, h - 2 * t, color);
}

}  // namespace ui
}  // namespace app
