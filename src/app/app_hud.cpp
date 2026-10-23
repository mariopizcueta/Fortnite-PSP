// HUD de la partida (Referencias/Referencia gameplay.png y Referencias/HUD/):
//   arriba izquierda   nombre y registro de eliminaciones
//   arriba centro      brujula; en modo construccion, el rotulo CONSTRUCCION
//   arriba derecha     minimapa con la zona, tiempo de zona, vivos y bajas, materiales
//   abajo izquierda    municion "30 | 120", escudo y vida
//   abajo derecha      ranuras de construccion y de armas (pico + SCAR)
// Ademas, las paginas de depuracion y de controles (SELECT).
#include "app/app.h"

#include <malloc.h>
#include <math.h>
#include <string.h>

#include <initializer_list>

#include "app/app_common.h"
#include "audio/audio_system.h"
#include "world/world_units.h"

namespace app {
namespace {

using namespace common;
using math3d::Vec3;
namespace atlas = assets::ui;

// Minimapa: ventana de 64 texeles (96 m) alrededor del jugador, dibujada en 76x76 px.
constexpr float kMapX = 398.0f, kMapY = 6.0f, kMapSize = 76.0f;
constexpr float kMapWindow = 64.0f;
constexpr float kMapTexels = 128.0f;
constexpr float kMetersPerTexel = 2.0f * world::kMapHalfSize / kMapTexels;

// Ranuras inferiores derechas.
constexpr float kBuildRowY = 196.0f;
constexpr float kBuildSlotX = 326.0f, kBuildSlotStep = 30.0f, kBuildSlotSize = 28.0f;
constexpr float kWeaponRowY = 230.0f;
constexpr float kWeaponSlotX = 254.0f, kWeaponSlotStep = 37.0f, kWeaponSlotSize = 34.0f;
constexpr int kWeaponSlots = 6;

// Barras de escudo y vida.
constexpr float kBarX = 32.0f, kBarW = 144.0f;
constexpr float kShieldY = 239.0f, kHealthY = 253.0f;

// Botones que se muestran en la lista de "Pulsado", en orden de lectura.
constexpr u32 kListedButtons[] = {
    PSP_CTRL_UP,       PSP_CTRL_DOWN,     PSP_CTRL_LEFT,     PSP_CTRL_RIGHT,
    PSP_CTRL_TRIANGLE, PSP_CTRL_CIRCLE,   PSP_CTRL_CROSS,    PSP_CTRL_SQUARE,
    PSP_CTRL_LTRIGGER, PSP_CTRL_RTRIGGER, PSP_CTRL_SELECT,   PSP_CTRL_START,
};

// Mando dibujado en la pagina de controles.
struct ButtonBox {
    u32 mask;
    float x, y, w, h;
};
constexpr ButtonBox kButtonBoxes[] = {
    {PSP_CTRL_LTRIGGER, 320, 44, 44, 10}, {PSP_CTRL_RTRIGGER, 422, 44, 44, 10},
    {PSP_CTRL_UP, 336, 64, 12, 12},       {PSP_CTRL_DOWN, 336, 92, 12, 12},
    {PSP_CTRL_LEFT, 322, 78, 12, 12},     {PSP_CTRL_RIGHT, 350, 78, 12, 12},
    {PSP_CTRL_TRIANGLE, 438, 64, 12, 12}, {PSP_CTRL_CROSS, 438, 92, 12, 12},
    {PSP_CTRL_SQUARE, 424, 78, 12, 12},   {PSP_CTRL_CIRCLE, 452, 78, 12, 12},
    {PSP_CTRL_SELECT, 366, 110, 24, 8},   {PSP_CTRL_START, 396, 110, 24, 8},
};
constexpr float kStickBoxX = 361.0f;
constexpr float kStickBoxY = 130.0f;
constexpr float kStickBoxSize = 56.0f;

// Concatena los nombres de los botones mantenidos sin pasar de `capacity`.
void heldButtonsString(u32 buttons, char* out, size_t capacity) {
    out[0] = '\0';
    size_t len = 0;
    for (u32 mask : kListedButtons) {
        if (!(buttons & mask)) continue;
        const char* name = input::buttonName(mask);
        const size_t need = strlen(name) + (len ? 1 : 0);
        if (len + need + 1 > capacity) {
            if (len + 3 < capacity) strcat(out, "..");
            return;
        }
        if (len) strcat(out, " ");
        strcat(out, name);
        len += need;
    }
    if (len == 0) strcpy(out, "(ninguno)");
}

// Caja de ranura al estilo de la referencia: fondo translucido y borde claro.
void slotBox(float x, float y, float size, u32 fill, u32 border) {
    gfx::fillRect(x, y, size, size, fill);
    ui::frame(x, y, size, size, 1, border);
}

}  // namespace

// --- HUD: parte dibujada con GU -------------------------------------------------------------

void App::renderGameHud() const {
    // Destello rojo al recibir un impacto.
    const float hurt = m_match.sincePlayerDamaged();
    if (hurt < 0.35f) {
        gfx::fillRect(0, 0, gfx::kScreenWidth, gfx::kScreenHeight,
                      rgba(200, 20, 10, static_cast<u32>(110.0f * (1.0f - hurt / 0.35f))));
    }
    if (m_buildMode) {
        // Rotulo CONSTRUCCION sobre una franja azul: el modo se ve siempre.
        gfx::fillRect(150, 18, 180, 22, kBuildBlue);
        if (m_atlas.ready()) m_atlas.drawCentered(atlas::kConstruccion, 240, 20);
    }
    renderCrosshair();
    renderMinimap();
    renderVitals();
    renderSlots();
    renderDamageNumbers();
}

void App::renderCrosshair() const {
    if (!m_match.alive(0) || m_freeCameraActive) return;
    if (m_buildMode) {
        // Reticula de construccion: cuatro esquinas.
        const u32 c = m_wallPlan.valid ? kBuildReticle : kErrorRed;
        for (int sx : {-1, 1}) {
            for (int sy : {-1, 1}) {
                const float cx = 240.0f + sx * 10.0f, cy = 136.0f + sy * 10.0f;
                gfx::fillRect(sx < 0 ? cx : cx - 5.0f, cy, 6, 2, c);
                gfx::fillRect(cx, sy < 0 ? cy : cy - 5.0f, 2, 6, c);
            }
        }
        return;
    }
    // Mira (roja si la ayuda de punteria tiene un objetivo) y marcador de impacto.
    const u32 c = m_match.playerAssistTarget() >= 0 || m_lockTarget > 0 ? kCrosshairLocked : kCrosshair;
    gfx::fillRect(229, 135, 7, 2, c);
    gfx::fillRect(244, 135, 7, 2, c);
    gfx::fillRect(239, 125, 2, 7, c);
    gfx::fillRect(239, 140, 2, 7, c);
    gfx::fillRect(239, 135, 2, 2, c);
    if (m_match.sincePlayerHit() < 0.15f) {
        for (int k = 0; k < 4; ++k) {
            gfx::fillRect(233 - k, 129 - k, 2, 2, kWhite);
            gfx::fillRect(245 + k, 129 - k, 2, 2, kWhite);
            gfx::fillRect(233 - k, 141 + k, 2, 2, kWhite);
            gfx::fillRect(245 + k, 141 + k, 2, 2, kWhite);
        }
    }
}

void App::renderMinimap() const {
    gfx::fillRect(kMapX - 2, kMapY - 2, kMapSize + 4, kMapSize + 4, rgba(0, 0, 0, 150));
    const game::CharacterState followed = renderState(spectateTarget());
    // Ventana centrada en el jugador, sin salirse del mapa.
    auto clampWindow = [](float c) {
        const float half = kMapWindow * 0.5f;
        return c < half ? half : (c > kMapTexels - half ? kMapTexels - half : c);
    };
    const float u0 = clampWindow((followed.position.x + world::kMapHalfSize) / kMetersPerTexel) - kMapWindow * 0.5f;
    const float v0 = clampWindow((followed.position.z + world::kMapHalfSize) / kMetersPerTexel) - kMapWindow * 0.5f;
    if (m_minimap.pixels) {
        gfx::drawTextureRect(m_minimap, u0, v0, kMapWindow, kMapWindow, kMapX, kMapY, kMapSize, kMapSize);
    }
    const float pxPerMeter = kMapSize / (kMapWindow * kMetersPerTexel);
    auto toMap = [&](float x, float z, float* sx, float* sy) {
        *sx = kMapX + ((x + world::kMapHalfSize) / kMetersPerTexel - u0) * (kMapSize / kMapWindow);
        *sy = kMapY + ((z + world::kMapHalfSize) / kMetersPerTexel - v0) * (kMapSize / kMapWindow);
        return *sx >= kMapX && *sx < kMapX + kMapSize - 1 && *sy >= kMapY && *sy < kMapY + kMapSize - 1;
    };

    // Borde de la zona: puntos blancos sobre el circulo.
    const Vec3& center = m_matchConfig.zoneCenter;
    const float radius = m_match.zoneRadius();
    const int points = radius * pxPerMeter > 40.0f ? 96 : 48;
    for (int i = 0; i < points; ++i) {
        const float a = 2.0f * math3d::kPi * i / points;
        float sx, sy;
        if (toMap(center.x + cosf(a) * radius, center.z + sinf(a) * radius, &sx, &sy)) {
            gfx::fillRect(sx, sy, 2, 2, kWhite);
        }
    }

    // Flecha del jugador (o de a quien se sigue tras caer), con el norte arriba.
    float px, py;
    if (toMap(followed.position.x, followed.position.z, &px, &py)) {
        const float fx = -sinf(followed.yaw), fy = -cosf(followed.yaw);  // adelante en el mapa
        const float rx = -fy, ry = fx;
        const u32 c = m_match.alive(0) ? kWhite : kErrorRed;
        const gfx::LineVertex arrow[] = {
            {c, px + fx * 5, py + fy * 5, 0}, {c, px - fx * 3 + rx * 3, py - fy * 3 + ry * 3, 0},
            {c, px - fx * 3 + rx * 3, py - fy * 3 + ry * 3, 0}, {c, px - fx * 1.5f, py - fy * 1.5f, 0},
            {c, px - fx * 1.5f, py - fy * 1.5f, 0}, {c, px - fx * 3 - rx * 3, py - fy * 3 - ry * 3, 0},
            {c, px - fx * 3 - rx * 3, py - fy * 3 - ry * 3, 0}, {c, px + fx * 5, py + fy * 5, 0},
        };
        gfx::drawLines2D(arrow, 8);
        gfx::fillRect(px - 1, py - 1, 2, 2, c);
    }

    // Fila de iconos: zona, vivos y bajas.
    if (m_atlas.ready()) {
        m_atlas.draw(atlas::kIconZona, 360, 86);
        m_atlas.draw(atlas::kIconVivos, 412, 86);
        m_atlas.draw(atlas::kIconBajas, 448, 86);
    }
    // Materiales: madera (paredes que quedan), ladrillo y metal (no se usan).
    gfx::fillRect(398, 106, 78, 30, rgba(0, 0, 0, 90));
    gfx::fillRect(399, 107, 25, 28, rgba(130, 150, 210, 120));
    gfx::fillRect(425, 109, 1, 24, rgba(255, 255, 255, 120));
    if (m_atlas.ready()) {
        m_atlas.draw(atlas::kMatMadera, 403, 108);
        m_atlas.draw(atlas::kMatLadrillo, 429, 108, 1.0f, rgba(255, 255, 255, 150));
        m_atlas.draw(atlas::kMatMetal, 455, 108, 1.0f, rgba(255, 255, 255, 150));
    }
}

void App::renderVitals() const {
    const game::CharacterState& p = m_match.body(0).state();
    // Municion "30 | 120" en grande (como la referencia de gameplay).
    const game::Weapon& w = m_match.playerWeapon();
    const u32 ammoColor = w.ammo() == 0 ? kErrorRed : (w.reloading() ? kLightGrey : kWhite);
    float x = 18.0f;
    x += drawBigNumber(x, 210, w.ammo(), 3.0f, ammoColor) + 7.0f;
    gfx::fillRect(x, 208, 2, 19, kLightGrey);
    x += 8.0f;
    if (w.reserve() >= 0) drawBigNumber(x, 210, w.reserve(), 3.0f, kWhite);
    if (w.reloading()) {
        gfx::fillRect(18, 229, 120, 3, rgba(0, 0, 0, 200));
        gfx::fillRect(18, 229, 120 * w.reloadProgress(), 3, config::kColorGold);
    }

    // Escudo (siempre 0: no hay pociones en esta version) y vida.
    gfx::fillRect(kBarX, kShieldY, kBarW, 10, rgba(0, 0, 0, 150));
    gfx::fillRect(kBarX, kHealthY, kBarW, 10, rgba(0, 0, 0, 150));
    const float ratio = p.maxHealth > 0.0f ? p.health / p.maxHealth : 0.0f;
    if (ratio > 0.0f) {
        gfx::fillRect(kBarX + 1, kHealthY + 1, (kBarW - 2) * ratio, 8, ratio > 0.3f ? kHealthGreen : kErrorRed);
        gfx::fillRect(kBarX + 1, kHealthY + 1, (kBarW - 2) * ratio, 2, rgba(255, 255, 255, 60));
    }
    if (m_atlas.ready()) {
        m_atlas.draw(atlas::kIconEscudo, 16, kShieldY - 1);
        m_atlas.draw(atlas::kIconVida, 16, kHealthY - 1);
    }
}

void App::renderSlots() const {
    // Ranuras de construccion: pared, suelo, rampa y una vacia. Solo la pared funciona.
    const assets::ui::Rect* buildIcons[] = {&atlas::kPared, &atlas::kSuelo, &atlas::kRampa};
    for (int i = 0; i < 5; ++i) {
        const float x = kBuildSlotX + i * kBuildSlotStep + (i == 4 ? 4.0f : 0.0f);
        const bool selected = m_buildMode && i == 0;
        const u32 fill = i < 3 ? rgba(90, 170, 230, m_buildMode ? 150 : 70) : rgba(0, 0, 0, 50);
        slotBox(x, kBuildRowY, kBuildSlotSize, fill, rgba(255, 255, 255, m_buildMode ? 200 : 110));
        if (selected) ui::frame(x - 2, kBuildRowY - 2, kBuildSlotSize + 4, kBuildSlotSize + 4, 2, kWhite);
        if (i < 3 && m_atlas.ready()) {
            const u32 tint = selected ? kWhite : rgba(255, 255, 255, m_buildMode ? 110 : 90);
            m_atlas.draw(*buildIcons[i], x + 1, kBuildRowY + 1, 1.0f, tint);
        }
    }

    // Ranuras de armas: pico y SCAR legendaria (fondo dorado); el resto, vacias.
    for (int i = 0; i < kWeaponSlots; ++i) {
        const bool selected = !m_buildMode && i == 1;
        const float x = kWeaponSlotX + i * kWeaponSlotStep;
        const float y = kWeaponRowY - (selected ? 3.0f : 0.0f);
        u32 fill = rgba(0, 0, 0, 50);
        if (i == 0) fill = rgba(170, 180, 200, 170);
        if (i == 1) fill = rgba(214, 138, 40, 200);
        slotBox(x, y, kWeaponSlotSize, fill, rgba(255, 255, 255, 170));
        if (selected) ui::frame(x - 2, y - 2, kWeaponSlotSize + 4, kWeaponSlotSize + 4, 2, kWhite);
        if (!m_atlas.ready()) continue;
        if (i == 0) m_atlas.draw(atlas::kPico, x + 4, y + 4);
        if (i == 1) m_atlas.draw(atlas::kScar, x + 1, y + 6, 0.66f);
    }
}

void App::renderDamageNumbers() const {
    // Numeros de dano sobre lo que ha alcanzado el jugador: suben y se desvanecen.
    const game::DamageNumber* numbers = m_match.damageNumbers();
    for (int i = 0; i < game::Match::kMaxDamageNumbers; ++i) {
        const game::DamageNumber& d = numbers[i];
        if (d.age >= kDamageNumberSeconds) continue;
        float x, y;
        if (!projectToScreen(m_viewProjection, d.position + Vec3{0.0f, d.age * 0.9f, 0.0f}, &x, &y)) continue;
        const float fade = d.age < 0.55f ? 1.0f : 1.0f - (d.age - 0.55f) / (kDamageNumberSeconds - 0.55f);
        const u32 alpha = static_cast<u32>(255.0f * fade);
        const float scale = d.structure ? 2.0f : 3.0f;
        const u32 color = d.structure ? rgba(200, 220, 255, alpha) : rgba(255, 255, 255, alpha);
        const int value = static_cast<int>(d.amount + 0.5f);
        drawBigNumber(x - bigNumberWidth(value, scale) * 0.5f, y - 2.5f * scale, value, scale, color);
    }
}

// --- HUD: texto -----------------------------------------------------------------------------

void App::overlayGame(const FrameInfo& frame) const {
    switch (m_hud) {
        case Hud::Game: overlayGameHud(frame); break;
        case Hud::Debug: overlayDebug(frame); break;
        case Hud::Controls: overlayControlsPage(frame); break;
        case Hud::Hidden:
        case Hud::Count:
            gfx::text::printShadow(400, 4, kWhite, "%5.1f FPS", frame.timer.fps());
            break;
    }
}

void App::overlayCompass() const {
    // Brujula: rumbos a menos de 70 grados de la camara (el oeste queda a la izquierda).
    static const char* const kNames[] = {"N", "NO", "O", "SO", "S", "SE", "E", "NE"};
    const float camera = headingDegrees(m_freeCameraActive ? 0.0f : m_followCamera.yaw());
    for (int i = 0; i < 8; ++i) {
        float diff = i * 45.0f - camera;
        while (diff > 180.0f) diff -= 360.0f;
        while (diff <= -180.0f) diff += 360.0f;
        if (fabsf(diff) > 70.0f) continue;
        const int x = static_cast<int>(240.0f - diff * 1.5f);
        printCenteredAt(x, 6, i % 2 == 0 ? kWhite : kDimGrey, kNames[i]);
    }
}

void App::overlayGameHud(const FrameInfo& frame) const {
    gfx::text::printShadow(6, 4, kLightGrey, "%s", kPlayerName);
    gfx::text::print(6, 14, kDimGrey, "%4.1f FPS", frame.timer.fps());
    overlayCompass();

    // Construccion: paredes y por que no se puede colocar.
    if (m_buildMode) {
        const game::StructureSystem& s = m_match.structures();
        char banner[48];
        snprintf(banner, sizeof(banner), "Paredes %d/%d", s.count(), s.config().maxWalls);
        printCenteredShadow(44, kWhite, banner);
        if (!m_wallPlan.valid) {
            printCenteredShadow(55, kErrorRed, game::placementErrorName(m_wallPlan.error));
        } else if (m_targetWall >= 0) {
            printCenteredShadow(55, config::kColorGold, "R: colocar  []: quitar esta  O: salir");
        } else {
            printCenteredShadow(55, kLightGrey, "R: colocar pared  O: combate");
        }
    }
    if (!m_match.alive(0)) {
        char line[40];
        snprintf(line, sizeof(line), "ESPECTANDO A %s", m_match.name(spectateTarget()));
        printCenteredShadow(44, kErrorRed, line);
    }

    // Registro de eliminaciones.
    int y = 28;
    for (int i = 0; i < m_match.killEventCount(); ++i) {
        const game::KillEvent& e = m_match.killEvent(i);
        if (m_match.elapsed() - e.time > kKillFeedSeconds) continue;
        const u32 color = (e.killer == 0 || e.victim == 0) ? config::kColorGold : kWhite;
        if (e.killer >= 0) {
            gfx::text::printShadow(6, y, color, "%s elimino a %s", m_match.name(e.killer), m_match.name(e.victim));
        } else {
            gfx::text::printShadow(6, y, color, "%s ha caido", m_match.name(e.victim));
        }
        y += 10;
    }

    // Bajo el minimapa: tiempo hasta que la zona termine de cerrarse, vivos y bajas.
    char time[16];
    formatTime(m_matchConfig.zoneShrinkSeconds - m_match.elapsed(), time, sizeof(time));
    gfx::text::printShadow(376, 89, kWhite, "%s", time);
    gfx::text::printShadow(428, 89, kWhite, "%d", m_match.aliveCount());
    gfx::text::printShadow(464, 89, kWhite, "%d", m_match.playerKills());
    const game::StructureSystem& s = m_match.structures();
    gfx::text::printShadow(400, 126, kWhite, "%2d", s.config().maxWalls - s.count());
    gfx::text::printShadow(431, 126, kLightGrey, "0");
    gfx::text::printShadow(457, 126, kLightGrey, "0");

    // Municion y estado del arma; escudo y vida.
    const game::Weapon& w = m_match.playerWeapon();
    if (w.reloading()) gfx::text::printShadow(18, 198, config::kColorGold, "RECARGANDO");
    else if (w.empty()) gfx::text::printShadow(18, 198, kErrorRed, "SIN BALAS");
    else if (w.ammo() == 0) gfx::text::printShadow(18, 198, kErrorRed, "[] recargar");
    const game::CharacterState& p = m_match.body(0).state();
    gfx::text::print(static_cast<int>(kBarX) + 4, static_cast<int>(kShieldY) + 1, kLightGrey, "0|100");
    gfx::text::print(static_cast<int>(kBarX) + 4, static_cast<int>(kHealthY) + 1, kWhite, "%.0f|%.0f",
                     p.health, p.maxHealth);
    // Balas en reserva sobre la ranura de la SCAR.
    if (w.reserve() >= 0) {
        const int sx = static_cast<int>(kWeaponSlotX + kWeaponSlotStep) + 34 - 8 * (w.reserve() >= 100 ? 3 : 2);
        gfx::text::printShadow(sx - 1, static_cast<int>(kWeaponRowY) + 25 - (m_buildMode ? 0 : 3), kWhite, "%d",
                               w.reserve());
    }
}

void App::overlayDebug(const FrameInfo& frame) const {
    const game::CharacterState& p = m_match.body(0).state();
    const game::MatchStats& s = m_match.stats();
    gfx::text::print(6, 4, config::kColorGold, "%s v%s", config::kGameTitle, config::kVersion);
    gfx::text::print(168, 4, kLightGrey, "%s", config::kPhase);
    gfx::text::print(400, 4, kWhite, "%5.1f FPS", frame.timer.fps());

    int y = 24;
    const int x = 8;
    gfx::text::print(x, y, config::kColorGold, "JUGADOR");
    y += kLineHeight;
    gfx::text::print(x, y, kWhite, "Pos %6.1f %5.1f %6.1f", p.position.x, p.position.y, p.position.z);
    y += kLineHeight;
    gfx::text::print(x, y, kWhite, "Vel %4.2f vert %+5.2f", p.horizontalSpeed(), p.velocity.y);
    y += kLineHeight;
    gfx::text::print(x, y, kWhite, "%s, %s", game::jumpStateName(p.jumpState), game::locomotionName(p));
    y += kLineHeight + 4;
    gfx::text::print(x, y, config::kColorGold, "RENDIMIENTO (%d bots)", m_match.botCount());
    y += kLineHeight;
    gfx::text::print(x, y, kWhite, "Frame %5.2f ms trab %5.2f", frame.timer.frameMsAvg(), frame.workMs);
    y += kLineHeight;
    gfx::text::print(x, y, kWhite, "IA %5.2f ms  fis %5.2f ms", m_aiMsPerFrame, m_physicsMsPerFrame);
    y += kLineHeight;
    gfx::text::print(x, y, kWhite, "Decisiones %3d/s", m_decisionsPerSecond);
    y += kLineHeight;
    gfx::text::print(x, y, kWhite, "Tris %5d lotes %3d", m_stats.trianglesDrawn, m_stats.batchesDrawn);
    y += kLineHeight;
    const struct mallinfo mem = mallinfo();
    gfx::text::print(x, y, kWhite, "Heap %u KB (lobby %u KB)", static_cast<unsigned>(mem.uordblks / 1024),
                     m_startHeapBytes / 1024);
    y += kLineHeight;
    if (m_audioReady) {
        gfx::text::print(x, y, kWhite, "Audio: %d voces, %u perdidas", audio::mixer().activeVoices(),
                         static_cast<unsigned>(audio::mixer().droppedCommands()));
    } else {
        gfx::text::print(x, y, kErrorRed, "Audio: sin canal");
    }
    y += kLineHeight + 4;
    gfx::text::print(x, y, config::kColorGold, "PARTIDA %d", m_matchesPlayed);
    y += kLineHeight;
    gfx::text::print(x, y, kWhite, "Vivos %d/%d  zona %2.0f m", m_match.aliveCount(), m_match.entityCount(),
                     m_match.zoneRadius());
    y += kLineHeight;
    gfx::text::print(x, y, kWhite, "Caminos %d (fallos %d)", s.pathRequests, s.pathFailures);
    y += kLineHeight;
    gfx::text::print(x, y, kWhite, "Max nodos A* %d  atascos %d", s.maxPathExpansions, s.stuckEvents);
    y += kLineHeight;
    gfx::text::print(x, y, kWhite, "Disparos %d imp %d pared %d", s.shotsFired, s.shotsHitEntity,
                     s.shotsBlockedByWorld);
    y += kLineHeight;
    gfx::text::print(x, y, kWhite, "Paredes +%d -%d rotas %d", s.wallsBuilt, s.wallsRemoved, s.wallsDestroyed);

    // Lista de bots: estado, vida, objetivo y distancia.
    y = 24;
    const int bx = 244;
    gfx::text::print(bx, y, config::kColorGold, "BOT ESTADO       VIDA OBJ");
    y += 10;
    const int maxRows = 23;
    for (int i = 0; i < m_match.botCount() && i < maxRows; ++i) {
        const game::Bot& b = m_match.bot(i);
        const bool dead = b.state == game::BotState::Eliminated;
        char target[16] = "-";
        if (!dead && b.target >= 0) snprintf(target, sizeof(target), "%s%d", b.target == 0 ? "J" : "B", b.target);
        gfx::text::print(bx, y, dead ? rgba(130, 130, 130) : (b.state == game::BotState::Attack ? kErrorRed : kWhite),
                         "%2d %-12s %3.0f %s", i + 1, game::botStateName(b.state), b.body.state().health, target);
        y += 10;
    }
    if (m_match.botCount() > maxRows) gfx::text::print(bx, y, kLightGrey, "... (%d mas)", m_match.botCount() - maxRows);
}

// --- Pagina de controles (lobby -> CONTROLES, y SELECT en la partida) -----------------------

void App::renderControlsPage(const FrameInfo& frame) const {
    const u32 buttons = frame.input.rawButtons();

    gfx::fillRect(0, 0, gfx::kScreenWidth, 30, kNavy);
    gfx::fillRect(0, 30, gfx::kScreenWidth, 1, rgba(60, 90, 160));
    if (m_atlas.ready()) m_atlas.drawCentered(atlas::kControles, 240, 4);
    gfx::fillRect(6, 36, 300, 208, rgba(0, 0, 0, 150));
    gfx::fillRect(312, 36, 162, 208, rgba(0, 0, 0, 150));
    gfx::fillRect(0, 250, gfx::kScreenWidth, 22, kNavy);

    for (const ButtonBox& b : kButtonBoxes) {
        gfx::fillRect(b.x, b.y, b.w, b.h, (buttons & b.mask) ? kButtonOn : kButtonOff);
    }

    // Stick analogico: caja, cruz central y punto con la posicion normalizada.
    const float half = kStickBoxSize / 2.0f;
    const float cx = kStickBoxX + half;
    const float cy = kStickBoxY + half;
    gfx::fillRect(kStickBoxX, kStickBoxY, kStickBoxSize, kStickBoxSize, kButtonOff);
    gfx::fillRect(kStickBoxX, cy, kStickBoxSize, 1, kButtonOff);
    gfx::fillRect(cx, kStickBoxY, 1, kStickBoxSize, kButtonOff);
    const float dotX = cx + frame.input.analogX() * (half - 3.0f);
    const float dotY = cy + frame.input.analogY() * (half - 3.0f);
    gfx::fillRect(dotX - 3.0f, dotY - 3.0f, 6, 6, kButtonOn);
}

void App::overlayControlsPage(const FrameInfo& frame) const {
    const input::Input& in = frame.input;
    gfx::text::print(400, 12, kLightGrey, "%5.1f FPS", frame.timer.fps());

    int y = 42;
    const int x = 14;
    const int col = x + 8 * 19;
    gfx::text::print(x, y, config::kColorGold, "COMBATE");
    y += kLineHeight;
    gfx::text::print(x, y, kWhite, "Stick moverse");
    gfx::text::print(col, y, kWhite, "X saltar");
    y += kLineHeight;
    gfx::text::print(x, y, kWhite, "R disparar");
    gfx::text::print(col, y, kWhite, "[] recargar");
    y += kLineHeight;
    gfx::text::print(x, y, kWhite, "L fijar objetivo");
    gfx::text::print(col, y, kWhite, "Cruceta camara");
    y += kLineHeight + 3;
    gfx::text::print(x, y, config::kColorGold, "CONSTRUCCION (O)");
    y += kLineHeight;
    gfx::text::print(x, y, kWhite, "R colocar pared");
    gfx::text::print(col, y, kWhite, "[] quitar pared");
    y += kLineHeight;
    gfx::text::print(x, y, kWhite, "O volver al combate");
    y += kLineHeight + 3;
    gfx::text::print(x, y, config::kColorGold, "PARTIDA");
    y += kLineHeight;
    gfx::text::print(x, y, kWhite, "START pausa");
    gfx::text::print(col, y, kWhite, "SELECT HUD");
    y += kLineHeight;
    gfx::text::print(x, y, kWhite, "^ camara libre (depuracion)");
    y += kLineHeight;
    gfx::text::print(x, y, kWhite, "L+R+START 1 s: salir del juego");
    y += kLineHeight + 3;

    char held[40];
    heldButtonsString(in.rawButtons(), held, 26);
    gfx::text::print(x, y, config::kColorGold, "PRUEBA DEL MANDO");
    y += kLineHeight;
    gfx::text::print(x, y, kWhite, "Pulsado: %s", held);
    y += kLineHeight;
    gfx::text::print(x, y, kWhite, "Ultimo : %s  total %u", m_lastButton, static_cast<unsigned>(m_pressCount));
    y += kLineHeight;
    gfx::text::print(x, y, kWhite, "Stick %+.2f, %+.2f  CPU %d/%d MHz", in.analogX(), in.analogY(), m_cpuMhz,
                     m_busMhz);

    gfx::text::print(339, 45, kWhite, "L");
    gfx::text::print(441, 45, kWhite, "R");
    gfx::text::print(366, 120, kLightGrey, "SEL");
    gfx::text::print(396, 120, kLightGrey, "STA");
    gfx::text::print(357, 192, kLightGrey, "ANALOGICO");

    if (m_screen == Screen::Game) {
        gfx::text::print(8, 257, kWhite, "SELECT: siguiente pagina del HUD");
    } else {
        gfx::text::print(8, 257, kWhite, "O: volver al lobby");
    }
}

}  // namespace app
