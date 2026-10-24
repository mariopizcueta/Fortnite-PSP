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

void Atlas::drawClipped(const assets::ui::Rect& r, float x, float y, float x0, float y0, float x1, float y1,
                        u32 color) const {
    const float left = x < x0 ? x0 : x, top = y < y0 ? y0 : y;
    const float right = x + r.w > x1 ? x1 : x + r.w, bottom = y + r.h > y1 ? y1 : y + r.h;
    if (right <= left || bottom <= top) return;
    gfx::drawTextureRect(m_texture, r.x + (left - x), r.y + (top - y), right - left, bottom - top, left, top,
                         right - left, bottom - top, color);
}

namespace {
// Boton fisico de cada accion (por defecto, los de siempre).
Button g_actions[kActionCount] = {Button::Cross, Button::Circle, Button::Square, Button::Triangle,
                                  Button::R,     Button::L,      Button::Down,   Button::Up};
}  // namespace

void setActionButton(Button action, Button physical) {
    const int i = static_cast<int>(action) - static_cast<int>(Button::Jump);
    if (i >= 0 && i < kActionCount) g_actions[i] = physical;
}

Button resolve(Button button) {
    const int i = static_cast<int>(button) - static_cast<int>(Button::Jump);
    return i >= 0 && i < kActionCount ? g_actions[i] : button;
}

const assets::ui::Rect& buttonRect(Button button) {
    namespace a = assets::ui;
    switch (resolve(button)) {
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
        default: break;
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
        const char* label = hints[i].label();
        const bool hasText = label && label[0];
        if (hasText) x += kIconTextGap;
        fn(i, iconX, x);
        if (hasText) x += gfx::text::width(label) + (i + 1 < count ? kHintGap : 0);
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
        const char* label = hints[i].label();
        if (label && label[0]) gfx::text::printShadow(textX, y, color, "%s", label);
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
