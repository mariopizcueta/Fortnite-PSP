#include "gfx/debug_text.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <pspdebug.h>
#include <pspdisplay.h>

#include "gfx/graphics.h"

namespace gfx {
namespace text {
namespace {

constexpr int kMaxChars = 96;

void putString(int x, int y, u32 color, const char* str) {
    if (y < 0 || y + kGlyphSize > kScreenHeight) return;
    const u32 rgb = color & 0x00FFFFFFu;
    for (const char* c = str; *c; ++c, x += kGlyphSize) {
        if (x + kGlyphSize > kScreenWidth) break;
        if (x < 0 || *c == ' ') continue;
        pspDebugScreenPutChar(x, y, rgb, static_cast<u8>(*c));
    }
}

}  // namespace

void init() {
    // setup=0: no toca el modo de pantalla que ya ha configurado GU.
    // Mismo formato que el framebuffer: la libreria convierte los colores al escribir.
    pspDebugScreenInitEx(nullptr, kRetro16BitFramebuffer ? PSP_DISPLAY_PIXEL_FORMAT_565 : PSP_DISPLAY_PIXEL_FORMAT_8888, 0);
    pspDebugScreenEnableBackColor(0);
}

void setTarget(u32* framebuffer) { pspDebugScreenSetBase(framebuffer); }

void print(int x, int y, u32 color, const char* fmt, ...) {
    char buffer[kMaxChars];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    putString(x, y, color, buffer);
}

void printShadow(int x, int y, u32 color, const char* fmt, ...) {
    char buffer[kMaxChars];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    putString(x + 1, y + 1, 0, buffer);
    putString(x, y, color, buffer);
}

int width(const char* str) { return static_cast<int>(strlen(str)) * kGlyphSize; }

}  // namespace text
}  // namespace gfx
