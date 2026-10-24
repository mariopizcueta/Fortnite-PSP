// Menu de ajustes del lobby (parche, fase 8; docs/AJUSTES.md). START lo abre y lo cierra.
//
// Mientras esta abierto, el lobby no recibe ningun boton (el menu es modal): L / R cambian
// de seccion del menu (graficos, sensibilidad, controles, idioma, datos) y no de seccion
// del lobby; arriba / abajo eligen la opcion; izquierda / derecha cambian su valor; X la
// activa (reasignar un boton, restaurar, idioma, borrar datos); CIRCULO o START cierran y
// guardan. Todo se aplica al momento.
#include "app/app.h"

#include <math.h>
#include <string.h>

#include "app/app_common.h"
#include "audio/audio_system.h"
#include "core/i18n.h"
#include "platform/log.h"

namespace app {
namespace {

using namespace common;
using i18n::T;
using i18n::tr;
namespace atlas = assets::ui;

enum Section { kGraphics, kSensitivity, kControls, kLanguage, kData, kSectionCount };
const T kSectionNames[kSectionCount] = {T::SetGraphics, T::SetSensitivity, T::SetControls, T::SetLanguage, T::SetData};
constexpr int kGraphicsItems = 6, kSensitivityItems = 4, kControlsItems = kActionCount + 1, kLanguageItems = 2,
              kDataItems = 1;
const T kActionNames[kActionCount] = {T::ActJump, T::ActBuild,  T::ActInteract, T::ActPickaxe,
                                      T::ActFire, T::ActAim,    T::ActCrouch,   T::ActEmote};

constexpr float kPanelX = 24.0f, kPanelY = 34.0f, kPanelW = 432.0f, kPanelH = 216.0f;
constexpr float kRowX = 36.0f, kRowW = 408.0f, kRowY = 80.0f, kRowH = 17.0f;
constexpr float kSectionY = 58.0f;

const ui::Hint kSettingsHints[] = {{ui::Button::Up, T::None},   {ui::Button::Down, T::HintSelect},
                                   {ui::Button::Left, T::None}, {ui::Button::Right, T::HintChange},
                                   {ui::Button::L, T::None},    {ui::Button::R, T::HintSections},
                                   {ui::Button::Circle, T::HintClose}};
const ui::Hint kDeleteHints[] = {{ui::Button::Cross, T::HintDelete}, {ui::Button::Circle, T::HintCancel}};

// Boton de la interfaz de un boton de la PSP (para dibujar su icono).
ui::Button uiButton(u32 mask) {
    switch (mask) {
        case PSP_CTRL_CROSS: return ui::Button::Cross;
        case PSP_CTRL_CIRCLE: return ui::Button::Circle;
        case PSP_CTRL_SQUARE: return ui::Button::Square;
        case PSP_CTRL_TRIANGLE: return ui::Button::Triangle;
        case PSP_CTRL_LTRIGGER: return ui::Button::L;
        case PSP_CTRL_RTRIGGER: return ui::Button::R;
        case PSP_CTRL_UP: return ui::Button::Up;
        case PSP_CTRL_DOWN: return ui::Button::Down;
        default: return ui::Button::Cross;
    }
}

// Seccion de cada pestana del menu: x de su texto (centradas en el panel).
int sectionX(int i) {
    int total = 0;
    for (int k = 0; k < kSectionCount; ++k) total += gfx::text::width(tr(kSectionNames[k])) + (k ? 14 : 0);
    int x = static_cast<int>(kPanelX + (kPanelW - total) * 0.5f);
    for (int k = 0; k < i; ++k) x += gfx::text::width(tr(kSectionNames[k])) + 14;
    return x;
}

}  // namespace

void App::applySettings() {
    i18n::setLanguage(m_settings.language);
    m_vsync = m_settings.vsync;
    input::setDeadzone(static_cast<float>(m_settings.deadzone) / 100.0f);
    for (int a = 0; a < kActionCount; ++a) {
        ui::setActionButton(static_cast<ui::Button>(static_cast<int>(ui::Button::Jump) + a),
                            uiButton(m_settings.buttons[a]));
    }
}

int App::settingsItemCount() const {
    switch (m_settingsSection) {
        case kGraphics: return kGraphicsItems;
        case kSensitivity: return kSensitivityItems;
        case kControls: return kControlsItems;
        case kLanguage: return kLanguageItems;
        default: return kDataItems;
    }
}

void App::openSettings() {
    m_settingsOpen = true;
    m_settingsSection = 0;
    m_settingsItem = 0;
    m_remapAction = -1;
    m_confirmDelete = false;
    m_settingsMessage = T::None;
    playSfx(audio::Sfx::MenuAccept, 0.7f);
}

void App::closeSettings() {
    m_settingsOpen = false;
    m_remapAction = -1;
    m_confirmDelete = false;
    if (!m_settings.save(m_settingsPath)) platform::log::write("aviso: no se pudo guardar %s", m_settingsPath);
    playSfx(audio::Sfx::MenuMove);
}

void App::deletePlayerData() {
    // Solo el perfil del juego (perfil.txt y su temporal, en la carpeta del EBOOT): nombre,
    // estadisticas, logros y taquilla. Ni los ajustes, ni el juego, ni otras aplicaciones.
    if (!Profile::erase(m_profilePath)) {
        platform::log::write("ERROR: no se pudo borrar %s", m_profilePath);
        m_settingsMessage = T::SetDeleteFailed;
        m_settingsMessageError = true;
        m_settingsMessageTime = 3.0f;
        m_confirmDelete = false;  // el perfil sigue intacto
        return;
    }
    platform::log::write("datos del jugador borrados (%s)", m_profilePath);
    m_profile = Profile{};
    m_profileDirty = false;
    m_toastCount = 0;
    // El traje vuelve a ser el de por defecto.
    m_models[0].destroy();
    m_modelsReady &= m_models[0].build(game::palettes::kPlayer);
    m_confirmDelete = false;
    m_settingsOpen = false;
    m_settings.save(m_settingsPath);
    // Como la primera vez: se pide el nombre.
    m_screen = Screen::Nickname;
    m_nickBuffer[0] = '\0';
    m_nickRow = m_nickCol = 0;
    m_nickLower = false;
    playSfx(audio::Sfx::MenuAccept);
}

void App::updateSettings(const input::Input& in) {
    if (m_settingsMessageTime > 0.0f) m_settingsMessageTime -= 1.0f / 60.0f;
    // Confirmacion de borrar datos: X borra, CIRCULO cancela.
    if (m_confirmDelete) {
        if (in.pressed(PSP_CTRL_CROSS)) deletePlayerData();
        else if (in.pressed(PSP_CTRL_CIRCLE) || in.pressed(PSP_CTRL_START)) {
            m_confirmDelete = false;
            playSfx(audio::Sfx::MenuMove);
        }
        return;
    }
    // Reasignando: el siguiente boton de la lista de reasignables; START cancela.
    if (m_remapAction >= 0) {
        if (in.pressed(PSP_CTRL_START)) {
            m_remapAction = -1;
            playSfx(audio::Sfx::MenuMove);
            return;
        }
        for (u32 b : kRemappableButtons) {
            if (!in.pressed(b)) continue;
            m_settings.assign(static_cast<Action>(m_remapAction), b);
            applySettings();
            m_remapAction = -1;
            playSfx(audio::Sfx::MenuAccept);
            break;
        }
        return;
    }
    if (in.pressed(PSP_CTRL_START) || in.pressed(PSP_CTRL_CIRCLE)) {
        closeSettings();
        return;
    }
    const int tab = (in.pressed(PSP_CTRL_RTRIGGER) ? 1 : 0) - (in.pressed(PSP_CTRL_LTRIGGER) ? 1 : 0);
    if (tab != 0) {
        m_settingsSection = (m_settingsSection + kSectionCount + tab) % kSectionCount;
        m_settingsItem = 0;
        playSfx(audio::Sfx::MenuMove, 0.6f);
        return;
    }
    const int count = settingsItemCount();
    const int step = (in.pressed(PSP_CTRL_DOWN) ? 1 : 0) - (in.pressed(PSP_CTRL_UP) ? 1 : 0);
    if (step != 0) {
        m_settingsItem = (m_settingsItem + count + step) % count;
        playSfx(audio::Sfx::MenuMove, 0.5f);
    }
    const int change = (in.pressed(PSP_CTRL_RIGHT) ? 1 : 0) - (in.pressed(PSP_CTRL_LEFT) ? 1 : 0);
    const bool activate = in.pressed(PSP_CTRL_CROSS);
    if (change == 0 && !activate) return;
    Settings& st = m_settings;
    auto cycle = [&](int* v, int lo, int hi, int by) {
        const int d = change != 0 ? change : 1;
        *v += d * by;
        if (*v > hi) *v = lo;
        if (*v < lo) *v = hi;
    };
    switch (m_settingsSection) {
        case kGraphics:
            switch (m_settingsItem) {
                case 0: cycle(&st.drawDistance, 0, 2, 1); break;
                case 1: cycle(&st.characterDetail, 0, 2, 1); break;
                case 2: st.damageNumbers = !st.damageNumbers; break;
                case 3: st.tracers = !st.tracers; break;
                case 4: st.showFps = !st.showFps; break;
                default: st.vsync = !st.vsync; break;
            }
            break;
        case kSensitivity:
            switch (m_settingsItem) {
                case 0: st.cameraSensitivity = static_cast<int>(math3d::clampf(st.cameraSensitivity + (change ? change : 1), 1, 10)); break;
                case 1: st.scopeSensitivity = static_cast<int>(math3d::clampf(st.scopeSensitivity + (change ? change : 1), 1, 10)); break;
                case 2: st.deadzone = static_cast<int>(math3d::clampf(st.deadzone + 5 * (change ? change : 1), 10, 35)); break;
                default: st.invertY = !st.invertY; break;
            }
            break;
        case kControls:
            if (!activate) return;
            if (m_settingsItem < kActionCount) {
                m_remapAction = m_settingsItem;  // espera el boton
            } else {
                st.resetControls();
            }
            break;
        case kLanguage:
            if (change != 0) m_settingsItem = (m_settingsItem + 1) % kLanguageItems;
            st.language = m_settingsItem == 1 ? i18n::Lang::En : i18n::Lang::Es;
            break;
        case kData:
            if (activate) m_confirmDelete = true;
            break;
        default:
            break;
    }
    applySettings();
    playSfx(audio::Sfx::MenuMove, 0.6f);
}

void App::renderSettings() const {
    gfx::fillRect(0, 0, gfx::kScreenWidth, gfx::kScreenHeight, rgba(0, 0, 0, 150));
    gfx::fillRect(kPanelX, kPanelY, kPanelW, kPanelH, rgba(10, 20, 52, 245));
    ui::frame(kPanelX, kPanelY, kPanelW, kPanelH, 2, rgba(90, 160, 255));
    gfx::fillRect(kPanelX, kPanelY, kPanelW, 22, kFortniteBlue);
    if (m_atlas.ready()) m_atlas.drawCentered(label(atlas::kAjustes, atlas::kAjustesEn), 240, kPanelY);
    // Pestanas de las secciones (L / R).
    const int sx = sectionX(m_settingsSection);
    gfx::fillRect(sx - 4, kSectionY - 3, gfx::text::width(tr(kSectionNames[m_settingsSection])) + 8, 13, kYellow);
    gfx::fillRect(kPanelX + 6, kSectionY + 12, kPanelW - 12, 1, rgba(90, 160, 255));
    const int count = settingsItemCount();
    for (int i = 0; i < count; ++i) {
        const float y = kRowY + i * kRowH;
        if (i == m_settingsItem) {
            gfx::fillRect(kRowX - 4, y - 3, kRowW, kRowH - 2, rgba(36, 92, 196, 230));
            ui::frame(kRowX - 5, y - 4, kRowW + 2, kRowH, 1, kWhite);
        }
        if (m_settingsSection == kControls && i < kActionCount) {
            const ui::Hint h{uiButton(m_settings.buttons[i]), T::None};
            ui::drawHintIcons(m_atlas, static_cast<int>(kRowX + 330), static_cast<int>(y), &h, 1);
        }
        if (m_settingsSection == kSensitivity && i < 3) {
            // Barra con el valor.
            const float v = i == 2 ? (m_settings.deadzone - 10) / 25.0f
                                   : ((i == 0 ? m_settings.cameraSensitivity : m_settings.scopeSensitivity) - 1) / 9.0f;
            gfx::fillRect(kRowX + 250, y + 2, 100, 5, rgba(0, 0, 0, 160));
            gfx::fillRect(kRowX + 250, y + 2, 100 * v, 5, kYellow);
        }
    }
    ui::drawHintIcons(m_atlas, 30, 238, kSettingsHints, 7);
    if (m_remapAction >= 0 || m_confirmDelete) {
        gfx::fillRect(70, 100, 340, 84, rgba(8, 14, 40, 250));
        ui::frame(70, 100, 340, 84, 2, m_confirmDelete ? kErrorRed : kYellow);
        if (m_confirmDelete) ui::drawHintIcons(m_atlas, 160, 166, kDeleteHints, 2);
    }
}

void App::overlaySettings() const {
    for (int k = 0; k < kSectionCount; ++k) {
        gfx::text::print(sectionX(k), static_cast<int>(kSectionY), k == m_settingsSection ? kNavy : kLightGrey, "%s",
                         tr(kSectionNames[k]));
    }
    const Settings& st = m_settings;
    auto onOff = [](bool v) { return tr(v ? T::SetOn : T::SetOff); };
    const T threeDist[3] = {T::SetNear, T::SetMedium, T::SetFar};
    const T threeLevel[3] = {T::SetLow, T::SetMid, T::SetHigh};
    const int count = settingsItemCount();
    const bool dialog = m_remapAction >= 0 || m_confirmDelete;
    for (int i = 0; i < count; ++i) {
        const int y = static_cast<int>(kRowY + i * kRowH);
        if (dialog && y > 92 && y < 188) continue;  // tapado por el dialogo
        const int vx = static_cast<int>(kRowX + 330);
        const char* name = "";
        char value[32] = "";
        switch (m_settingsSection) {
            case kGraphics: {
                const T names[kGraphicsItems] = {T::SetDrawDistance, T::SetCharacterDetail, T::SetDamageNumbers,
                                                 T::SetTracers,      T::SetFps,             T::SetVsync};
                name = tr(names[i]);
                if (i == 0) snprintf(value, sizeof(value), "< %s >", tr(threeDist[st.drawDistance]));
                else if (i == 1) snprintf(value, sizeof(value), "< %s >", tr(threeLevel[st.characterDetail]));
                else snprintf(value, sizeof(value), "< %s >", onOff(i == 2 ? st.damageNumbers : i == 3 ? st.tracers : i == 4 ? st.showFps : st.vsync));
                break;
            }
            case kSensitivity: {
                const T names[kSensitivityItems] = {T::SetCamera, T::SetScope, T::SetDeadzone, T::SetInvertY};
                name = tr(names[i]);
                if (i == 0) snprintf(value, sizeof(value), "%d", st.cameraSensitivity);
                else if (i == 1) snprintf(value, sizeof(value), "%d", st.scopeSensitivity);
                else if (i == 2) snprintf(value, sizeof(value), "%d%%", st.deadzone);
                else snprintf(value, sizeof(value), "< %s >", onOff(st.invertY));
                break;
            }
            case kControls:
                name = i < kActionCount ? tr(kActionNames[i]) : tr(T::SetRestoreControls);
                if (i < kActionCount) snprintf(value, sizeof(value), "%s", buttonKey(st.buttons[i]));
                break;
            case kLanguage:
                name = i == 0 ? tr(T::SetSpanish) : tr(T::SetEnglish);
                snprintf(value, sizeof(value), "%s", (i == 1) == (st.language == i18n::Lang::En) ? "*" : "");
                break;
            default:
                name = tr(T::SetDeleteData);
                break;
        }
        gfx::text::printShadow(static_cast<int>(kRowX), y, kWhite, "%s", name);
        if (value[0]) {
            const int x = m_settingsSection == kControls ? vx + 20 : (m_settingsSection == kSensitivity && i < 3 ? vx + 30 : vx);
            gfx::text::printShadow(x, y, kYellow, "%s", value);
        }
    }
    // Notas al pie de la seccion.
    const int noteY = static_cast<int>(kRowY + count * kRowH + 4);
    if (dialog) {
        // (el dialogo ocupa su sitio)
    } else if (m_settingsSection == kControls) {
        gfx::text::print(static_cast<int>(kRowX), noteY, kDimGrey, "%s", tr(T::SetFixedButtons));
    } else if (m_settingsSection == kData) {
        gfx::text::print(static_cast<int>(kRowX), noteY, kLightGrey, "%s", tr(T::SetDeleteInfo1));
        gfx::text::print(static_cast<int>(kRowX), noteY + 11, kLightGrey, "%s", tr(T::SetDeleteInfo2));
        gfx::text::print(static_cast<int>(kRowX), noteY + 22, kLightGrey, "%s", tr(T::SetDeleteInfo3));
    } else {
        gfx::text::print(static_cast<int>(kRowX), noteY, kDimGrey, "%s", tr(T::SetAppliesNow));
    }
    if (m_settingsMessageTime > 0.0f) {
        printCentered(224, m_settingsMessageError ? kErrorRed : kYellow, tr(m_settingsMessage));
    }
    ui::printHints(30, 238, kLightGrey, kSettingsHints, 7);
    if (m_remapAction >= 0) {
        char line[64];
        snprintf(line, sizeof(line), tr(T::SetPressButton), tr(kActionNames[m_remapAction]));
        printCentered(126, kWhite, line);
        printCentered(146, kLightGrey, tr(T::SetPressCancel));
    } else if (m_confirmDelete) {
        printCentered(110, kErrorRed, tr(T::SetDeleteConfirm));
        printCentered(126, kLightGrey, tr(T::SetDeleteInfo1));
        printCentered(137, kLightGrey, tr(T::SetDeleteInfo2));
        printCentered(148, kLightGrey, tr(T::SetDeleteInfo3));
        ui::printHints(160, 166, kWhite, kDeleteHints, 2);
    }
}

}  // namespace app
