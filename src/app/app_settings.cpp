// Menu de ajustes del lobby (parche, fase 8; docs/AJUSTES.md). START lo abre y lo cierra.
//
// Mientras esta abierto, el lobby no recibe ningun boton (el menu es modal): L / R cambian
// de seccion del menu (graficos, sensibilidad, controles, idioma, datos) y no de seccion
// del lobby; arriba / abajo eligen la opcion; izquierda / derecha cambian su valor; X la
// activa (reasignar un boton, restaurar, idioma, borrar datos); CIRCULO o START cierran y
// guardan. Todo se aplica al momento.
//
// v0.13: seccion ADMIN. Con la contrasena (teclado en pantalla) se desbloquea el mod menu:
// trucos que se guardan en ajustes.txt y se aplican en la siguiente partida o, desde la
// pausa (tercera opcion), en la partida en curso.
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

enum Section { kGraphics, kSensitivity, kControls, kLanguage, kData, kAdmin, kSectionCount };
const T kSectionNames[kSectionCount] = {T::SetGraphics, T::SetSensitivity, T::SetControls,
                                        T::SetLanguage, T::SetData,        T::SetAdmin};
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

// Contrasena del menu de administrador (se compara sin distinguir mayusculas).
constexpr const char* kAdminPassword = "adminpezporno";
// Teclado de la contrasena: 4 filas de 10 teclas y una de botones (BORRAR | ACEPTAR).
constexpr int kPassCols = 10, kPassRows = 5;
const char* const kPassKeys[2][4] = {
    {"abcdefghij", "klmnopqrst", "uvwxyz-_.!", "0123456789"},
    {"ABCDEFGHIJ", "KLMNOPQRST", "UVWXYZ-_.!", "0123456789"},
};
constexpr float kPassX = 120.0f, kPassY = 100.0f, kPassKeyW = 22.0f, kPassKeyH = 18.0f, kPassGap = 2.0f;
const ui::Hint kPassHints[] = {{ui::Button::Cross, T::HintType}, {ui::Button::Circle, T::HintErase},
                               {ui::Button::Triangle, T::HintCase}, {ui::Button::Start, T::HintAccept}};

// Mod menu: filas visibles, posicion y ayudas.
constexpr int kModRows = 9;
constexpr float kModX = 24.0f, kModY = 30.0f, kModW = 432.0f, kModH = 222.0f, kModRowY = 60.0f, kModRowH = 16.0f;
const ui::Hint kModHints[] = {{ui::Button::Up, T::None},          {ui::Button::Down, T::HintSelect},
                              {ui::Button::Cross, T::HintToggle}, {ui::Button::L, T::None},
                              {ui::Button::R, T::HintPage},       {ui::Button::Circle, T::HintClose}};
// Acciones del mod menu (solo en partida, desde la pausa), despues de los trucos.
const T kModActions[] = {T::CheatActionHeal, T::CheatActionRefill, T::CheatActionKillBots};
constexpr int kModActionCount = 3;

bool samePassword(const char* a, const char* b) {
    for (; *a && *b; ++a, ++b) {
        const char x = (*a >= 'A' && *a <= 'Z') ? *a - 'A' + 'a' : *a;
        const char y = (*b >= 'A' && *b <= 'Z') ? *b - 'A' + 'a' : *b;
        if (x != y) return false;
    }
    return *a == *b;
}

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
        case kAdmin: return m_settings.admin ? 2 : 1;
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
    if (m_modMenuOpen) {
        updateModMenu(in);
        return;
    }
    if (m_passwordOpen) {
        updatePassword(in);
        return;
    }
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
        case kAdmin:
            if (!activate) return;
            if (!st.admin) {
                // Contrasena: teclado en pantalla.
                m_passwordOpen = true;
                m_passBuffer[0] = '\0';
                m_passRow = m_passCol = 0;
                m_passUpper = false;
            } else if (m_settingsItem == 0) {
                openModMenu();
                return;
            } else {
                // Bloquear de nuevo: sin trucos.
                st.admin = false;
                st.cheats = game::Cheats{};
                m_settingsItem = 0;
                m_settings.save(m_settingsPath);
            }
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
    if (m_passwordOpen) renderPassword();
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
    if (m_passwordOpen) {
        overlayPassword();
        return;
    }
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
            case kAdmin:
                name = !st.admin ? tr(T::SetAdminPassword) : (i == 0 ? tr(T::SetAdminOpen) : tr(T::SetAdminLock));
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
    } else if (m_settingsSection == kAdmin) {
        gfx::text::print(static_cast<int>(kRowX), noteY, kLightGrey, "%s", tr(st.admin ? T::SetAdminInfoOn : T::SetAdminInfo1));
        if (!st.admin) gfx::text::print(static_cast<int>(kRowX), noteY + 11, kLightGrey, "%s", tr(T::SetAdminInfo2));
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

// --- Menu de administrador (v0.13) ------------------------------------------------------------

void App::updatePassword(const input::Input& in) {
    const int dx = (in.pressed(PSP_CTRL_RIGHT) ? 1 : 0) - (in.pressed(PSP_CTRL_LEFT) ? 1 : 0);
    const int dy = (in.pressed(PSP_CTRL_DOWN) ? 1 : 0) - (in.pressed(PSP_CTRL_UP) ? 1 : 0);
    if (dx || dy) {
        m_passRow = (m_passRow + dy + kPassRows) % kPassRows;
        m_passCol = (m_passCol + dx + kPassCols) % kPassCols;
        playSfx(audio::Sfx::MenuMove, 0.5f);
    }
    if (in.pressed(PSP_CTRL_TRIANGLE)) m_passUpper = !m_passUpper;
    const int len = static_cast<int>(strlen(m_passBuffer));
    bool erase = in.pressed(PSP_CTRL_CIRCLE);
    bool accept = in.pressed(PSP_CTRL_START);
    if (in.pressed(PSP_CTRL_CROSS)) {
        if (m_passRow == kPassRows - 1) {
            (m_passCol < kPassCols / 2 ? erase : accept) = true;
        } else if (len < static_cast<int>(sizeof(m_passBuffer)) - 1) {
            m_passBuffer[len] = kPassKeys[m_passUpper ? 1 : 0][m_passRow][m_passCol];
            m_passBuffer[len + 1] = '\0';
            playSfx(audio::Sfx::MenuMove, 0.6f);
        }
    }
    if (erase) {
        if (len > 0) {
            m_passBuffer[len - 1] = '\0';
            playSfx(audio::Sfx::MenuMove, 0.5f);
        } else {
            m_passwordOpen = false;  // CIRCULO sin nada escrito: salir
            playSfx(audio::Sfx::MenuMove);
        }
        return;
    }
    if (!accept) return;
    m_passwordOpen = false;
    if (samePassword(m_passBuffer, kAdminPassword)) {
        m_settings.admin = true;
        if (!m_settings.save(m_settingsPath)) platform::log::write("aviso: no se pudo guardar %s", m_settingsPath);
        platform::log::write("menu de administrador desbloqueado");
        playSfx(audio::Sfx::MenuAccept);
        playSfx(audio::Sfx::Pickup, 0.8f);
        openModMenu();
    } else {
        m_settingsMessage = T::SetAdminWrong;
        m_settingsMessageError = true;
        m_settingsMessageTime = 2.5f;
        playSfx(audio::Sfx::Hurt, 0.6f);
    }
    m_passBuffer[0] = '\0';
}

void App::renderPassword() const {
    gfx::fillRect(96, 64, 288, 168, rgba(8, 14, 40, 250));
    ui::frame(96, 64, 288, 168, 2, kYellow);
    gfx::fillRect(150, 80, 180, 16, rgba(0, 0, 0, 170));
    for (int r = 0; r < kPassRows - 1; ++r) {
        for (int c = 0; c < kPassCols; ++c) {
            const bool selected = r == m_passRow && c == m_passCol;
            gfx::fillRect(kPassX + c * (kPassKeyW + kPassGap), kPassY + r * (kPassKeyH + kPassGap), kPassKeyW, kPassKeyH,
                          selected ? kYellow : rgba(30, 60, 130, 230));
        }
    }
    const float y = kPassY + (kPassRows - 1) * (kPassKeyH + kPassGap);
    const float half = (kPassCols / 2) * (kPassKeyW + kPassGap) - kPassGap;
    const bool last = m_passRow == kPassRows - 1;
    gfx::fillRect(kPassX, y, half, kPassKeyH, last && m_passCol < kPassCols / 2 ? kYellow : rgba(150, 40, 40, 230));
    gfx::fillRect(kPassX + half + kPassGap, y, half, kPassKeyH, last && m_passCol >= kPassCols / 2 ? kYellow : rgba(40, 130, 60, 230));
    ui::drawHintIcons(m_atlas, centeredHintsX(kPassHints, 4), 216, kPassHints, 4);
}

void App::overlayPassword() const {
    printCentered(68, kWhite, tr(T::SetAdminPasswordTitle));
    // La contrasena no se ve: un asterisco por letra.
    char shown[sizeof(m_passBuffer) + 1];
    const int len = static_cast<int>(strlen(m_passBuffer));
    for (int i = 0; i < len; ++i) shown[i] = '*';
    shown[len] = fmodf(m_blinkTime, 1.0f) < 0.5f ? '_' : ' ';
    shown[len + 1] = '\0';
    printCentered(84, config::kColorGold, shown);
    for (int r = 0; r < kPassRows - 1; ++r) {
        for (int c = 0; c < kPassCols; ++c) {
            const bool selected = r == m_passRow && c == m_passCol;
            char key[2] = {kPassKeys[m_passUpper ? 1 : 0][r][c], '\0'};
            gfx::text::print(static_cast<int>(kPassX + c * (kPassKeyW + kPassGap) + 7),
                             static_cast<int>(kPassY + r * (kPassKeyH + kPassGap) + 5), selected ? kNavy : kWhite, "%s", key);
        }
    }
    const int y = static_cast<int>(kPassY + (kPassRows - 1) * (kPassKeyH + kPassGap) + 5);
    const int half = static_cast<int>((kPassCols / 2) * (kPassKeyW + kPassGap) - kPassGap);
    printCenteredAt(static_cast<int>(kPassX) + half / 2, y, kWhite, tr(T::NickErase));
    printCenteredAt(static_cast<int>(kPassX) + half + 2 + half / 2, y, kWhite, tr(T::NickAccept));
    ui::printHints(centeredHintsX(kPassHints, 4), 216, kLightGrey, kPassHints, 4);
}

void App::openModMenu() {
    if (!m_settings.admin) return;
    m_modMenuOpen = true;
    m_modCursor = 0;
    playSfx(audio::Sfx::MenuAccept, 0.8f);
}

int App::modMenuCount() const {
    // Las acciones (curarse, municion, eliminar bots) solo tienen sentido en una partida.
    return cheatCount() + (m_screen == Screen::Game ? kModActionCount : 0);
}

void App::applyCheats() {
    if (!m_settings.admin) m_settings.cheats = game::Cheats{};
    if (m_screen == Screen::Game) m_match.setCheats(m_settings.cheats);
}

void App::updateModMenu(const input::Input& in) {
    const int count = modMenuCount();
    if (in.pressed(PSP_CTRL_CIRCLE) || in.pressed(PSP_CTRL_START)) {
        m_modMenuOpen = false;
        if (!m_settings.save(m_settingsPath)) platform::log::write("aviso: no se pudo guardar %s", m_settingsPath);
        playSfx(audio::Sfx::MenuMove);
        return;
    }
    int step = (in.pressed(PSP_CTRL_DOWN) ? 1 : 0) - (in.pressed(PSP_CTRL_UP) ? 1 : 0);
    step += (in.pressed(PSP_CTRL_RTRIGGER) ? kModRows : 0) - (in.pressed(PSP_CTRL_LTRIGGER) ? kModRows : 0);
    if (step != 0) {
        m_modCursor = math3d::clampf(static_cast<float>(m_modCursor + step), 0.0f, static_cast<float>(count - 1));
        playSfx(audio::Sfx::MenuMove, 0.5f);
    }
    if (!in.pressed(PSP_CTRL_CROSS)) return;
    if (m_modCursor < cheatCount()) {
        bool& flag = m_settings.cheats.*cheatDef(m_modCursor).flag;
        flag = !flag;
        platform::log::write("truco %s: %s", cheatDef(m_modCursor).key, flag ? "si" : "no");
        applyCheats();
        playSfx(flag ? audio::Sfx::MenuAccept : audio::Sfx::MenuMove, 0.8f);
        return;
    }
    switch (m_modCursor - cheatCount()) {
        case 0: m_match.cheatHeal(); break;
        case 1: m_match.cheatRefill(); break;
        default: m_match.cheatEliminateBots(); break;
    }
    playSfx(audio::Sfx::Pickup, 0.9f);
}

void App::renderModMenu() const {
    gfx::fillRect(0, 0, gfx::kScreenWidth, gfx::kScreenHeight, rgba(0, 0, 0, 160));
    gfx::fillRect(kModX, kModY, kModW, kModH, rgba(14, 8, 30, 248));
    ui::frame(kModX, kModY, kModW, kModH, 2, rgba(60, 255, 120));
    gfx::fillRect(kModX, kModY, kModW, 24, rgba(40, 10, 70));
    if (m_atlas.ready()) m_atlas.drawCentered(atlas::kModMenu, 240, kModY + 1, 1.0f, rgba(120, 255, 160));
    const int count = modMenuCount();
    int top = m_modCursor - kModRows / 2;
    if (top > count - kModRows) top = count - kModRows;
    if (top < 0) top = 0;
    for (int r = 0; r < kModRows && top + r < count; ++r) {
        const int i = top + r;
        const float y = kModRowY + r * kModRowH;
        if (i == m_modCursor) {
            gfx::fillRect(kModX + 8, y - 3, kModW - 16, kModRowH - 2, rgba(40, 120, 70, 230));
            ui::frame(kModX + 7, y - 4, kModW - 14, kModRowH, 1, kWhite);
        }
        if (i < cheatCount()) {
            const bool on = m_settings.cheats.*cheatDef(i).flag;
            gfx::fillRect(kModX + kModW - 52, y - 1, 36, 10, on ? rgba(40, 200, 90) : rgba(120, 30, 40));
        }
    }
    // Barra de desplazamiento.
    const float h = 144.0f * kModRows / count;
    gfx::fillRect(kModX + kModW - 6, kModRowY - 2 + 144.0f * top / count, 2, h, rgba(120, 255, 160, 180));
    gfx::fillRect(kModX + 8, kModY + kModH - 42, kModW - 16, 1, rgba(60, 255, 120, 160));
    ui::drawHintIcons(m_atlas, centeredHintsX(kModHints, 6), static_cast<int>(kModY + kModH) - 14, kModHints, 6);
}

void App::overlayModMenu() const {
    const int count = modMenuCount();
    int top = m_modCursor - kModRows / 2;
    if (top > count - kModRows) top = count - kModRows;
    if (top < 0) top = 0;
    for (int r = 0; r < kModRows && top + r < count; ++r) {
        const int i = top + r;
        const int y = static_cast<int>(kModRowY + r * kModRowH);
        if (i < cheatCount()) {
            const bool on = m_settings.cheats.*cheatDef(i).flag;
            gfx::text::printShadow(static_cast<int>(kModX) + 16, y, kWhite, "%s", tr(cheatDef(i).name));
            gfx::text::print(static_cast<int>(kModX + kModW) - 46, y, kWhite, "%s", tr(on ? T::SetOn : T::SetOff));
        } else {
            gfx::text::printShadow(static_cast<int>(kModX) + 16, y, kYellow, "> %s", tr(kModActions[i - cheatCount()]));
        }
    }
    // Descripcion de lo elegido y donde se aplica.
    const int noteY = static_cast<int>(kModY + kModH) - 36;
    if (m_modCursor < cheatCount()) {
        gfx::text::print(static_cast<int>(kModX) + 12, noteY, kLightGrey, "%s", tr(cheatDef(m_modCursor).description));
    } else {
        gfx::text::print(static_cast<int>(kModX) + 12, noteY, kLightGrey, "%s", tr(T::CheatActionInfo));
    }
    gfx::text::print(static_cast<int>(kModX) + 12, noteY + 11, rgba(120, 255, 160), "%s",
                     tr(m_screen == Screen::Game ? T::CheatAppliesNow : T::CheatAppliesNext));
    ui::printHints(centeredHintsX(kModHints, 6), static_cast<int>(kModY + kModH) - 14, kLightGrey, kModHints, 6);
}

}  // namespace app
