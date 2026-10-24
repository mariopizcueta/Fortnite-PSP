// Constantes y utilidades compartidas por app.cpp, app_menus.cpp y app_hud.cpp.
#pragma once

#include <stdio.h>

#include "core/config.h"
#include "gfx/debug_text.h"
#include "gfx/graphics.h"
#include "math/math3d.h"

namespace app {
namespace common {

using gfx::rgba;

constexpr u32 kWhite = rgba(255, 255, 255);
constexpr u32 kLightGrey = rgba(200, 200, 200);
constexpr u32 kDimGrey = rgba(140, 140, 140);
constexpr u32 kErrorRed = rgba(255, 90, 80);
constexpr u32 kPanel = rgba(0, 0, 0, 140);
constexpr u32 kBar = rgba(0, 0, 0, 170);
constexpr u32 kButtonOff = rgba(255, 255, 255, 60);
constexpr u32 kButtonOn = config::kColorGold;
constexpr u32 kCrosshair = rgba(255, 255, 255, 220);
constexpr u32 kCrosshairLocked = rgba(255, 70, 60, 240);
constexpr u32 kHealthGreen = rgba(96, 196, 80);
constexpr u32 kShieldBlue = rgba(70, 150, 235);
constexpr u32 kBuildBlue = rgba(30, 90, 190, 170);
constexpr u32 kBuildReticle = rgba(120, 200, 255);
// Azules de la interfaz de Fortnite (Referencias/Lobby.png, loginscreen.png).
constexpr u32 kNavy = rgba(16, 26, 58);
constexpr u32 kFortniteBlue = rgba(36, 92, 196);
constexpr u32 kYellow = rgba(250, 216, 40);
constexpr u32 kYellowDark = rgba(196, 150, 10);

constexpr int kLineHeight = 11;
constexpr float kDamageNumberSeconds = 0.9f;
constexpr float kKillFeedSeconds = 6.0f;
constexpr float kCorpseSeconds = 8.0f;  // los eliminados se ven tumbados este tiempo

inline void printCentered(int y, u32 color, const char* str) {
    gfx::text::print((gfx::kScreenWidth - gfx::text::width(str)) / 2, y, color, "%s", str);
}

inline void printCenteredShadow(int y, u32 color, const char* str) {
    gfx::text::printShadow((gfx::kScreenWidth - gfx::text::width(str)) / 2, y, color, "%s", str);
}

// Texto centrado en `cx` (no en la pantalla).
inline void printCenteredAt(int cx, int y, u32 color, const char* str) {
    gfx::text::printShadow(cx - gfx::text::width(str) / 2, y, color, "%s", str);
}

// Rumbo de 0 a 360 grados (0 = norte, crece hacia el oeste como el yaw).
inline float headingDegrees(float yaw) {
    float d = yaw * (180.0f / math3d::kPi);
    while (d < 0.0f) d += 360.0f;
    while (d >= 360.0f) d -= 360.0f;
    return d;
}

inline void formatTime(float seconds, char* out, size_t size) {
    const int s = seconds > 0.0f ? static_cast<int>(seconds) : 0;
    snprintf(out, size, "%d:%02d", s / 60, s % 60);
}

}  // namespace common
}  // namespace app
