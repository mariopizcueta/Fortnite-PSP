// Pantallas fuera de la partida y superpuestas a ella: inicio de sesion, lobby, controles,
// carga, pausa y resultados (victoria / derrota).
#include "app/app.h"

#include <math.h>
#include <string.h>

#include "app/app_common.h"
#include "core/i18n.h"
#include "audio/audio_system.h"
#include "platform/log.h"

namespace app {
namespace {

using namespace common;
using i18n::T;
using i18n::tr;
namespace atlas = assets::ui;

constexpr float kConnectSeconds = 1.8f;    // "Conectando..." del inicio de sesion
constexpr float kLoadingSeconds = 2.0f;    // pantalla de carga minima antes de la partida
constexpr float kVictoryDelay = 1.0f;      // s desde la ultima baja hasta la pantalla
constexpr float kDefeatDelay = 2.5f;       // s viendo la propia caida antes de la pantalla
constexpr float kResultsInputDelay = 0.8f; // evita saltarse los resultados disparando
constexpr int kMinBots = 5;
constexpr int kStatsX = 4, kStatsY = 44, kStatsW = 122, kStatsH = 78;  // panel del perfil en el lobby

const T kLoginSteps[] = {T::LoginConnecting, T::LoginChecking, T::LoginProfile};

// Consejos de la pantalla de carga: con el boton dibujado cuando hablan de uno.
struct Tip {
    bool hasButton;
    ui::Hint hint;
};
const Tip kTips[] = {
    {true, {ui::Button::Build, T::TipBuild}},
    {true, {ui::Button::Aim, T::TipAim}},
    {false, {ui::Button::Cross, T::TipZone}},
    {true, {ui::Button::Interact, T::TipReload}},
    {false, {ui::Button::Cross, T::TipWalls}},
    {true, {ui::Button::Pickaxe, T::TipPickaxe}},
    {false, {ui::Button::Cross, T::TipTraps}},
    {false, {ui::Button::Cross, T::TipZombies}},
};
constexpr int kTipCount = sizeof(kTips) / sizeof(kTips[0]);
constexpr int kTipX = 100, kTipY = 238;

// Columna derecha del lobby (como el boton JUGAR de Fortnite). v0.13: sin navegar con la
// cruceta; cada boton lleva el suyo: TRIANGULO juega, CUADRADO abre el modo de juego y los
// bots, SELECT los controles y CIRCULO sale.
constexpr float kLobbyColumnX = 312.0f;
constexpr float kLobbyColumnW = 152.0f;
struct LobbyButton {
    float y, h;
};
constexpr LobbyButton kPlayButton{70.0f, 38.0f};
constexpr LobbyButton kModeBox{112.0f, 38.0f};
constexpr LobbyButton kControlsButton{156.0f, 24.0f};
constexpr LobbyButton kExitButton{186.0f, 24.0f};
const ui::Hint kPlayHint[] = {{ui::Button::Triangle, T::None}};
const ui::Hint kModeHint[] = {{ui::Button::Square, T::None}};
const ui::Hint kControlsHint[] = {{ui::Button::Select, T::None}};
const ui::Hint kExitHint[] = {{ui::Button::Circle, T::None}};
// Pavos (arriba a la derecha, bajo la barra, en todas las secciones).
constexpr float kVbucksX = 396.0f, kVbucksY = 34.0f, kVbucksW = 78.0f, kVbucksH = 16.0f;

// Ayudas de controles (iconos de Recursos/Texturas PSP).
const ui::Hint kLobbyHints[] = {{ui::Button::Stick, T::HintRotate}, {ui::Button::L, T::None},
                                {ui::Button::R, T::HintSections},  {ui::Button::Start, T::HintSettings}};
const ui::Hint kExitHints[] = {{ui::Button::Cross, T::HintExit}, {ui::Button::Circle, T::HintCancel}};
const ui::Hint kPauseHints[] = {{ui::Button::Start, T::None}, {ui::Button::Circle, T::HintContinue}};
const ui::Hint kResultsHints[] = {{ui::Button::Cross, T::HintLobby}, {ui::Button::Square, T::HintAnotherMatch},
                                  {ui::Button::Circle, T::HintSpectate}};
constexpr int kLobbyHintCount = 4, kExitHintCount = 2, kPauseHintCount = 2;
constexpr int kHintsY = 258;


// Opciones de la pausa (la tercera, MOD MENU, solo con el menu de administrador).
constexpr T kPauseItems[] = {T::PauseContinue, T::PauseLobby, T::PauseModMenu};

void drawButton(float x, float y, float w, float h, u32 fill, bool selected, float time) {
    gfx::fillRect(x, y, w, h, fill);
    gfx::fillRect(x, y + h - 3, w, 3, rgba(0, 0, 0, 70));  // borde inferior en sombra
    if (selected) {
        // Marco blanco que late suavemente (como la seleccion de los menus de Fortnite).
        const u32 alpha = static_cast<u32>(200.0f + 55.0f * sinf(time * 6.0f));
        ui::frame(x - 2, y - 2, w + 4, h + 4, 2, rgba(255, 255, 255, alpha));
    }
}

}  // namespace

// --- Idioma (primer arranque) -----------------------------------------------------------

// Solo sale cuando no hay ajustes.txt: se elige el idioma y se guarda; despues, el inicio de
// sesion. Se puede cambiar luego en AJUSTES. El titulo va en los dos idiomas.
const ui::Hint kLanguageHints[] = {{ui::Button::Dpad, T::HintSelect}, {ui::Button::Cross, T::HintAccept}};
constexpr float kLangBoxX = 130.0f, kLangBoxY = 70.0f, kLangBoxW = 220.0f, kLangBoxH = 120.0f;
constexpr float kLangRowX = 160.0f, kLangRowY = 112.0f, kLangRowW = 160.0f, kLangRowH = 28.0f, kLangRowGap = 8.0f;

void App::updateLanguage(const input::Input& in) {
    const int step = (in.pressed(PSP_CTRL_DOWN) ? 1 : 0) - (in.pressed(PSP_CTRL_UP) ? 1 : 0);
    if (step != 0) {
        m_langChoice = (m_langChoice + 2 + step) % 2;
        m_settings.language = m_langChoice == 1 ? i18n::Lang::En : i18n::Lang::Es;
        i18n::setLanguage(m_settings.language);
        playSfx(audio::Sfx::MenuMove, 0.6f);
    }
    if (in.pressed(PSP_CTRL_CROSS) || in.pressed(PSP_CTRL_START)) {
        if (!m_settings.save(m_settingsPath)) platform::log::write("aviso: no se pudo guardar %s", m_settingsPath);
        playSfx(audio::Sfx::MenuAccept);
        m_screen = Screen::Login;
    }
}

void App::renderLanguage() const {
    if (m_login.pixels) gfx::drawTexture(m_login, 0.0f, 0.0f);
    gfx::fillRect(0, 0, gfx::kScreenWidth, gfx::kScreenHeight, rgba(0, 0, 0, 130));
    gfx::fillRect(kLangBoxX, kLangBoxY, kLangBoxW, kLangBoxH, rgba(8, 18, 48, 235));
    ui::frame(kLangBoxX, kLangBoxY, kLangBoxW, kLangBoxH, 2, rgba(90, 160, 255));
    for (int i = 0; i < 2; ++i) {
        drawButton(kLangRowX, kLangRowY + i * (kLangRowH + kLangRowGap), kLangRowW, kLangRowH,
                   i == m_langChoice ? kYellow : rgba(30, 60, 130, 230), i == m_langChoice, m_blinkTime);
    }
    gfx::fillRect(0, 252, gfx::kScreenWidth, 20, rgba(10, 18, 44, 220));
    ui::drawHintIcons(m_atlas, centeredHintsX(kLanguageHints, 2), kHintsY, kLanguageHints, 2);
}

void App::overlayLanguage() const {
    printCentered(82, kWhite, "ELIGE IDIOMA");
    printCentered(94, kLightGrey, "CHOOSE LANGUAGE");
    const char* const names[2] = {"ESPANOL", "ENGLISH"};
    for (int i = 0; i < 2; ++i) {
        printCentered(static_cast<int>(kLangRowY + i * (kLangRowH + kLangRowGap) + 10),
                      i == m_langChoice ? kNavy : kWhite, names[i]);
    }
    ui::printHints(centeredHintsX(kLanguageHints, 2), kHintsY, kLightGrey, kLanguageHints, 2);
}

// --- Inicio de sesion -------------------------------------------------------------------

void App::updateLogin(const input::Input& in, float dt) {
    if (m_loginStage == 0) {
        if (in.pressed(PSP_CTRL_CROSS) || in.pressed(PSP_CTRL_START)) {
            m_loginStage = 1;
            m_loginTimer = 0.0f;
            playSfx(audio::Sfx::MenuAccept);
        }
        return;
    }
    m_loginTimer += dt;
    if (m_loginTimer >= kConnectSeconds) {
        playSfx(audio::Sfx::MenuAccept, 0.8f);
        if (m_profile.hasName()) {
            goToLobby();
        } else {
            // Primera vez: elegir el nombre.
            m_screen = Screen::Nickname;
            m_nickBuffer[0] = '\0';
            m_nickRow = m_nickCol = 0;
            m_nickLower = false;
        }
    }
}

// --- Nombre (primera vez) ----------------------------------------------------------------

// Teclado en pantalla: 4 filas de 10 caracteres y una de botones (BORRAR | ACEPTAR).
constexpr int kNickCols = 10, kNickRows = 5;
const char* const kNickKeys[2][4] = {
    {"ABCDEFGHIJ", "KLMNOPQRST", "UVWXYZ-_.!", "0123456789"},
    {"abcdefghij", "klmnopqrst", "uvwxyz-_.!", "0123456789"},
};
constexpr float kNickKeyX = 120.0f, kNickKeyY = 104.0f, kNickKeyW = 22.0f, kNickKeyH = 20.0f, kNickGap = 2.0f;
const ui::Hint kNickHints[] = {{ui::Button::Cross, T::HintType}, {ui::Button::Circle, T::HintErase},
                               {ui::Button::Triangle, T::HintCase}, {ui::Button::Start, T::HintAccept}};

void App::updateNickname(const input::Input& in) {
    const int dx = (in.pressed(PSP_CTRL_RIGHT) ? 1 : 0) - (in.pressed(PSP_CTRL_LEFT) ? 1 : 0);
    const int dy = (in.pressed(PSP_CTRL_DOWN) ? 1 : 0) - (in.pressed(PSP_CTRL_UP) ? 1 : 0);
    if (dx || dy) {
        m_nickRow = (m_nickRow + dy + kNickRows) % kNickRows;
        m_nickCol = (m_nickCol + dx + kNickCols) % kNickCols;
        playSfx(audio::Sfx::MenuMove, 0.5f);
    }
    if (in.pressed(PSP_CTRL_TRIANGLE)) m_nickLower = !m_nickLower;
    const int len = static_cast<int>(strlen(m_nickBuffer));
    bool erase = in.pressed(PSP_CTRL_CIRCLE);
    bool accept = in.pressed(PSP_CTRL_START);
    if (in.pressed(PSP_CTRL_CROSS)) {
        if (m_nickRow == kNickRows - 1) {
            (m_nickCol < kNickCols / 2 ? erase : accept) = true;
        } else if (len < Profile::kMaxName) {
            m_nickBuffer[len] = kNickKeys[m_nickLower ? 1 : 0][m_nickRow][m_nickCol];
            m_nickBuffer[len + 1] = '\0';
            playSfx(audio::Sfx::MenuMove, 0.6f);
        }
    }
    if (erase && len > 0) {
        m_nickBuffer[len - 1] = '\0';
        playSfx(audio::Sfx::MenuMove, 0.5f);
    }
    if (accept && m_nickBuffer[0] != '\0') {
        snprintf(m_profile.name, sizeof(m_profile.name), "%s", m_nickBuffer);
        if (!m_profile.save(m_profilePath)) platform::log::write("aviso: no se pudo guardar %s", m_profilePath);
        playSfx(audio::Sfx::MenuAccept);
        goToLobby();
    }
}

void App::renderNickname() const {
    if (m_lobbyReady) m_lobbyScene.drawBackground(m_lobbyTime);
    else gfx::fillRect(0, 0, gfx::kScreenWidth, gfx::kScreenHeight, kNavy);
    gfx::fillRect(100, 36, 280, 210, rgba(8, 18, 48, 230));
    ui::frame(100, 36, 280, 210, 2, rgba(90, 160, 255));
    gfx::fillRect(150, 68, 180, 22, rgba(0, 0, 0, 160));  // casilla del nombre
    for (int r = 0; r < kNickRows - 1; ++r) {
        for (int c = 0; c < kNickCols; ++c) {
            const bool selected = r == m_nickRow && c == m_nickCol;
            gfx::fillRect(kNickKeyX + c * (kNickKeyW + kNickGap), kNickKeyY + r * (kNickKeyH + kNickGap), kNickKeyW,
                          kNickKeyH, selected ? kYellow : rgba(30, 60, 130, 230));
        }
    }
    const float y = kNickKeyY + (kNickRows - 1) * (kNickKeyH + kNickGap);
    const float half = (kNickCols / 2) * (kNickKeyW + kNickGap) - kNickGap;
    const bool last = m_nickRow == kNickRows - 1;
    gfx::fillRect(kNickKeyX, y, half, kNickKeyH, last && m_nickCol < kNickCols / 2 ? kYellow : rgba(150, 40, 40, 230));
    gfx::fillRect(kNickKeyX + half + kNickGap, y, half, kNickKeyH,
                  last && m_nickCol >= kNickCols / 2 ? kYellow : rgba(40, 130, 60, 230));
    gfx::fillRect(0, 252, gfx::kScreenWidth, 20, rgba(10, 18, 44, 220));
    ui::drawHintIcons(m_atlas, 8, kHintsY, kNickHints, 4);
}

void App::overlayNickname() const {
    printCentered(46, kWhite, tr(T::NickTitle));
    char shown[Profile::kMaxName + 2];
    const bool cursor = fmodf(m_lobbyTime, 1.0f) < 0.5f && strlen(m_nickBuffer) < Profile::kMaxName;
    snprintf(shown, sizeof(shown), "%s%s", m_nickBuffer, cursor ? "_" : " ");
    printCentered(75, config::kColorGold, shown);
    for (int r = 0; r < kNickRows - 1; ++r) {
        for (int c = 0; c < kNickCols; ++c) {
            const bool selected = r == m_nickRow && c == m_nickCol;
            char key[2] = {kNickKeys[m_nickLower ? 1 : 0][r][c], '\0'};
            gfx::text::print(static_cast<int>(kNickKeyX + c * (kNickKeyW + kNickGap) + 7),
                             static_cast<int>(kNickKeyY + r * (kNickKeyH + kNickGap) + 6), selected ? kNavy : kWhite,
                             "%s", key);
        }
    }
    const int y = static_cast<int>(kNickKeyY + (kNickRows - 1) * (kNickKeyH + kNickGap) + 6);
    const int half = static_cast<int>((kNickCols / 2) * (kNickKeyW + kNickGap) - kNickGap);
    printCenteredAt(static_cast<int>(kNickKeyX) + half / 2, y, kWhite, tr(T::NickErase));
    printCenteredAt(static_cast<int>(kNickKeyX) + half + 2 + half / 2, y, kWhite, tr(T::NickAccept));
    printCentered(226, kLightGrey, tr(T::NickSaved));
    ui::printHints(8, kHintsY, kLightGrey, kNickHints, 4);
}

void App::recordMatch() {
    // Suma la partida al perfil (una vez por partida: al ver los resultados o al abandonar).
    // Las bajas, los disparos a la cabeza y el resto de contadores de los logros ya se han
    // sumado durante la partida (App::processEvents).
    if (m_matchRecorded) return;
    m_matchRecorded = true;
    // Partida de practica (menos de 30 bots, v0.14): no cuenta para nada (ni pavos ni logros).
    m_matchVbucks = 0;
    if (!m_rewardsActive) {
        platform::log::write("partida de practica (%d bots): sin pavos ni logros", m_botCount);
        return;
    }
    if (m_match.zombieMode()) {
        // Supervivencia: sus propios contadores (noches, zombis...), no los del battle royale.
        ++m_profile.matches;
        if (!m_match.alive(0)) ++m_profile.deaths;
        m_profile.seconds += static_cast<int>(m_match.elapsed());
        recordZombieMatch();
        checkAchievements();
        saveProfile();
        return;
    }
    const bool won = m_match.phase() == game::MatchPhase::Finished && m_match.winner() == 0;
    const int kills = m_match.playerKills();
    ++m_profile.matches;
    if (won) ++m_profile.wins;
    // Pavos por la victoria magistral (v0.13).
    m_matchVbucks = won ? kVbucksPerWin : 0;
    m_profile.vbucks += m_matchVbucks;
    if (!m_match.alive(0)) ++m_profile.deaths;
    if (kills > m_profile.mostKills) m_profile.mostKills = kills;
    const int place = won ? 1 : m_match.playerPlace();
    if (place > 0 && (m_profile.bestPlace == 0 || place < m_profile.bestPlace)) m_profile.bestPlace = place;
    if (place > 0 && place <= 5) ++m_profile.top5;
    m_profile.seconds += static_cast<int>(m_match.elapsed());
    checkAchievements();
    saveProfile();
}

void App::saveProfile() {
    if (m_profile.save(m_profilePath)) {
        m_profileDirty = false;
    } else {
        platform::log::write("aviso: no se pudo guardar %s", m_profilePath);
    }
}

void App::addStat(Stat stat, int amount) {
    if (amount <= 0 || !m_profile.hasName()) return;
    if (m_screen == Screen::Game && !m_rewardsActive) return;  // partida de practica: nada cuenta
    m_profile.mutableStat(stat) += amount;
    m_profileDirty = true;
    checkAchievements();
}

void App::checkAchievements() {
    AchievementId fresh[kAchievementCount];
    const int n = m_profile.unlockReached(todayDate(), fresh, kAchievementCount);
    if (n <= 0) return;
    for (int i = 0; i < n; ++i) {
        platform::log::write("logro desbloqueado: %s", achievementDef(fresh[i]).key);
        if (m_toastCount < kMaxToasts) m_toasts[m_toastCount++] = fresh[i];
    }
    playSfx(audio::Sfx::MenuAccept, 1.0f);
    playSfx(audio::Sfx::Pickup, 0.8f);
    // Se guarda en el momento: un logro no se pierde aunque se apague la consola.
    saveProfile();
}

// Aviso de logro desbloqueado: arriba en el centro, 3 s cada uno.
constexpr float kToastSeconds = 3.0f;
constexpr float kToastX = 120.0f, kToastY = 62.0f, kToastW = 240.0f, kToastH = 30.0f;

void App::updateAchievementToasts(float dt) {
    if (m_toastCount == 0) return;
    m_toastTime += dt;
    if (m_toastTime < kToastSeconds) return;
    m_toastTime = 0.0f;
    for (int i = 1; i < m_toastCount; ++i) m_toasts[i - 1] = m_toasts[i];
    --m_toastCount;
}

// En los resultados va mas abajo (arriba esta el rotulo de victoria o derrota).
float toastBaseY(bool results) { return results ? 196.0f : kToastY; }

void App::renderAchievementToast() const {
    if (m_toastCount == 0) return;
    // Entra deslizandose desde arriba.
    const float in = fminf(1.0f, m_toastTime / 0.2f);
    const float y = toastBaseY(m_results != Results::None) - (1.0f - in) * 40.0f;
    gfx::fillRect(kToastX, y, kToastW, kToastH, rgba(8, 18, 48, 235));
    gfx::fillRect(kToastX, y, 6, kToastH, kYellow);
    ui::frame(kToastX, y, kToastW, kToastH, 1, kYellow);
}

void App::overlayAchievementToast() const {
    if (m_toastCount == 0) return;
    const float in = fminf(1.0f, m_toastTime / 0.2f);
    const int y = static_cast<int>(toastBaseY(m_results != Results::None) - (1.0f - in) * 40.0f);
    printCenteredAt(243, y + 5, kYellow, tr(T::AchievementUnlocked));
    printCenteredAt(243, y + 17, kWhite, tr(achievementDef(m_toasts[0]).name));
}

// Pantalla de inicio: la imagen de Recursos/loginscreen.png, "Pulsa (X) para iniciar" bajo
// el logotipo y, al pulsar, una barra de carga (la conexion es simulada).
constexpr int kLoginCenterX = 158;  // centro del logotipo en la imagen
constexpr int kLoginTextY = 236;
const ui::Hint kLoginHint[] = {{ui::Button::Cross, T::LoginToStart}};

int loginPromptX() {
    return kLoginCenterX - (gfx::text::width(tr(T::LoginPress)) + ui::hintsWidth(kLoginHint, 1)) / 2;
}

void App::renderLogin() const {
    if (m_login.pixels) gfx::drawTexture(m_login, 0.0f, 0.0f);
    gfx::fillRect(0, 224, gfx::kScreenWidth, 36, rgba(8, 18, 48, 170));
    if (m_loginStage == 0) {
        // Parpadea suavemente, como el "pulsa para empezar" de las consolas.
        if (fmodf(m_blinkTime, 1.2f) < 0.85f) {
            ui::drawHintIcons(m_atlas, loginPromptX() + gfx::text::width(tr(T::LoginPress)), kLoginTextY, kLoginHint, 1);
        }
    } else {
        const float t = m_loginTimer / kConnectSeconds;
        gfx::fillRect(40, 250, 236, 5, rgba(0, 0, 0, 140));
        gfx::fillRect(40, 250, 236 * (t < 1.0f ? t : 1.0f), 5, kYellow);
    }
}

void App::overlayLogin() const {
    gfx::text::printShadow(6, 4, kLightGrey, "v%s", config::kVersion);
    if (m_loginStage == 0) {
        if (fmodf(m_blinkTime, 1.2f) < 0.85f) {
            const int x = loginPromptX();
            gfx::text::printShadow(x, kLoginTextY, kWhite, "%s", tr(T::LoginPress));
            ui::printHints(x + gfx::text::width(tr(T::LoginPress)), kLoginTextY, kWhite, kLoginHint, 1);
        }
    } else {
        int step = static_cast<int>(m_loginTimer / (kConnectSeconds / 3.0f));
        if (step > 2) step = 2;
        printCenteredAt(kLoginCenterX, 234, kWhite, tr(kLoginSteps[step]));
    }
}

// --- Lobby ------------------------------------------------------------------------------

void App::updateLobby(const input::Input& in, float dt) {
    m_lobbyTime += dt;
    if (m_lobbyMessageTime > 0.0f) m_lobbyMessageTime -= dt;
    // El stick gira el personaje sobre la plataforma.
    m_lobbyYaw -= in.analogX() * 2.6f * dt;
    game::CharacterState idle;
    idle.yaw = m_lobbyYaw;
    m_lobbyAnim.update(idle, false, true, dt);

    if (m_confirmExit) {
        if (in.pressed(PSP_CTRL_CROSS)) {
            m_exitRequested = true;
        } else if (in.pressed(PSP_CTRL_CIRCLE)) {
            m_confirmExit = false;
            playSfx(audio::Sfx::MenuMove);
        }
        return;
    }

    // START: menu de ajustes (modal: mientras esta abierto el lobby no recibe botones). Con
    // L o R pulsados no: es la combinacion de salida L+R+START.
    if (m_settingsOpen) {
        updateSettings(in);
        return;
    }
    if (m_modeOpen) {
        updateModePanel(in);
        return;
    }
    if (in.pressed(PSP_CTRL_START) && !in.held(PSP_CTRL_LTRIGGER) && !in.held(PSP_CTRL_RTRIGGER)) {
        openSettings();
        return;
    }
    // L / R: seccion del lobby (jugar, logros, tienda, taquilla). En la partida L y R son
    // apuntar y disparar: no hay conflicto, el lobby solo existe fuera de ella.
    const int tabs = static_cast<int>(LobbyTab::Count);
    const int tabStep = (in.pressed(PSP_CTRL_RTRIGGER) ? 1 : 0) - (in.pressed(PSP_CTRL_LTRIGGER) ? 1 : 0);
    if (tabStep != 0 && !in.held(PSP_CTRL_START) && !m_shopConfirm) {
        m_lobbyTab = static_cast<LobbyTab>((static_cast<int>(m_lobbyTab) + tabs + tabStep) % tabs);
        m_lockerChoosing = false;
        playSfx(audio::Sfx::MenuMove);
        return;
    }
    switch (m_lobbyTab) {
        case LobbyTab::Play: updateLobbyPlay(in); break;
        case LobbyTab::Achievements: updateAchievementsTab(in); break;
        case LobbyTab::Shop: updateShopTab(in); break;
        case LobbyTab::Locker: updateLockerTab(in); break;
        case LobbyTab::Count: break;
    }
}

void App::showLobbyMessage(T text, bool error) {
    m_lobbyMessage = text;
    m_lobbyMessageError = error;
    m_lobbyMessageTime = 2.5f;
}

void App::updateLobbyPlay(const input::Input& in) {
    // Sin navegar con la cruceta: cada accion tiene su boton.
    if (in.pressed(PSP_CTRL_TRIANGLE)) {
        if (!m_worldReady) return;
        playSfx(audio::Sfx::MenuAccept);
        startLoading();
    } else if (in.pressed(PSP_CTRL_SQUARE)) {
        m_modeOpen = true;
        m_modeRow = 0;
        playSfx(audio::Sfx::MenuAccept, 0.7f);
    } else if (in.pressed(PSP_CTRL_SELECT)) {
        playSfx(audio::Sfx::MenuAccept);
        m_screen = Screen::Controls;
    } else if (in.pressed(PSP_CTRL_CIRCLE)) {
        playSfx(audio::Sfx::MenuAccept);
        m_confirmExit = true;
    }
}

// Panel del modo de juego y de los bots (CUADRADO en JUGAR): arriba / abajo elige la fila,
// izquierda / derecha cambia el valor; CUADRADO, X o CIRCULO lo cierran.
constexpr float kModeX = 106.0f, kModeY = 64.0f, kModeW = 268.0f, kModeH = 150.0f;
constexpr float kModeRowY[2] = {96.0f, 160.0f};
const ui::Hint kModeHints[] = {{ui::Button::Up, T::None},   {ui::Button::Down, T::HintSelect},
                               {ui::Button::Left, T::None}, {ui::Button::Right, T::HintChange},
                               {ui::Button::Square, T::HintClose}};

T modeName(game::GameMode mode) {
    return mode == game::GameMode::OneShot   ? T::ModeOneShot
           : mode == game::GameMode::Zombies ? T::ModeZombies
                                             : T::ModeBattleRoyale;
}
T modeDescription(game::GameMode mode, int line) {
    if (mode == game::GameMode::OneShot) return line == 0 ? T::ModeOneShotDesc1 : T::ModeOneShotDesc2;
    if (mode == game::GameMode::Zombies) return line == 0 ? T::ModeZombiesDesc1 : T::ModeZombiesDesc2;
    return line == 0 ? T::ModeBattleRoyaleDesc1 : T::ModeBattleRoyaleDesc2;
}

void App::updateModePanel(const input::Input& in) {
    if (in.pressed(PSP_CTRL_SQUARE) || in.pressed(PSP_CTRL_CROSS) || in.pressed(PSP_CTRL_CIRCLE)) {
        m_modeOpen = false;
        playSfx(audio::Sfx::MenuAccept, 0.7f);
        return;
    }
    if (in.pressed(PSP_CTRL_UP) || in.pressed(PSP_CTRL_DOWN)) {
        m_modeRow = 1 - m_modeRow;
        playSfx(audio::Sfx::MenuMove, 0.6f);
    }
    const int change = (in.pressed(PSP_CTRL_RIGHT) ? 1 : 0) - (in.pressed(PSP_CTRL_LEFT) ? 1 : 0);
    if (change == 0) return;
    if (m_modeRow == 0) {
        const int modes = static_cast<int>(game::GameMode::Count);
        m_gameMode = static_cast<game::GameMode>((static_cast<int>(m_gameMode) + modes + change) % modes);
        playSfx(audio::Sfx::MenuMove);
    } else if (m_gameMode != game::GameMode::Zombies) {  // en el modo zombi no hay bots
        const int before = m_botCount;
        if (change < 0 && m_botCount > kMinBots) m_botCount -= 5;
        if (change > 0 && m_botCount < game::kMaxBots) m_botCount += 5;
        if (before != m_botCount) playSfx(audio::Sfx::MenuMove);
    }
}

void App::renderModePanel() const {
    gfx::fillRect(0, 0, gfx::kScreenWidth, gfx::kScreenHeight, rgba(0, 0, 0, 140));
    gfx::fillRect(kModeX, kModeY, kModeW, kModeH, rgba(10, 20, 52, 245));
    ui::frame(kModeX, kModeY, kModeW, kModeH, 2, rgba(90, 160, 255));
    gfx::fillRect(kModeX, kModeY, kModeW, 20, kFortniteBlue);
    for (int r = 0; r < 2; ++r) {
        const float h = r == 0 ? 54.0f : 22.0f;
        gfx::fillRect(kModeX + 10, kModeRowY[r] - 4, kModeW - 20, h, r == m_modeRow ? rgba(36, 92, 196, 230) : rgba(20, 34, 80, 200));
        if (r == m_modeRow) ui::frame(kModeX + 9, kModeRowY[r] - 5, kModeW - 18, h + 2, 1, kWhite);
    }
    ui::drawHintIcons(m_atlas, centeredHintsX(kModeHints, 5), static_cast<int>(kModeY + kModeH) - 16, kModeHints, 5);
}

void App::overlayModePanel() const {
    printCenteredAt(240, static_cast<int>(kModeY) + 6, kWhite, tr(T::ModeTitle));
    char line[64];
    snprintf(line, sizeof(line), "< %s >", tr(modeName(m_gameMode)));
    printCenteredAt(240, static_cast<int>(kModeRowY[0]), kYellow, line);
    printCenteredAt(240, static_cast<int>(kModeRowY[0]) + 14, kLightGrey, tr(modeDescription(m_gameMode, 0)));
    printCenteredAt(240, static_cast<int>(kModeRowY[0]) + 25, kLightGrey, tr(modeDescription(m_gameMode, 1)));
    if (m_gameMode == game::GameMode::Zombies) {
        printCenteredAt(240, static_cast<int>(kModeRowY[1]) + 3, kDimGrey, tr(T::ModeBotsZombies));
    } else {
        // Con menos de 30 bots, partida de practica: sin pavos ni logros (v0.14).
        snprintf(line, sizeof(line), tr(T::ModeBots), m_botCount);
        const bool practice = m_botCount < game::kRewardBots;
        printCenteredAt(240, static_cast<int>(kModeRowY[1]) + 3, practice ? kErrorRed : kWhite, line);
        if (practice) printCenteredAt(240, static_cast<int>(kModeRowY[1]) + 22, kErrorRed, tr(T::BotsWarning));
    }
    ui::printHints(centeredHintsX(kModeHints, 5), static_cast<int>(kModeY + kModeH) - 16, kLightGrey, kModeHints, 5);
}

// Logros: lista con su progreso (arriba / abajo la recorren; X recoge la recompensa).
constexpr int kAchievementRows = 6;
constexpr float kListX = 14.0f, kListY = 52.0f, kListW = 452.0f, kRowH = 31.0f;

void App::updateAchievementsTab(const input::Input& in) {
    if (in.pressed(PSP_CTRL_CROSS)) {
        // Recompensa de un logro completado (tambien los de antes de la v0.13).
        if (m_profile.claim(static_cast<AchievementId>(m_achievementCursor))) {
            saveProfile();
            playSfx(audio::Sfx::MenuAccept);
            playSfx(audio::Sfx::Pickup, 0.8f);
            showLobbyMessage(T::RewardClaimed, false);
        } else {
            playSfx(audio::Sfx::MenuMove, 0.4f);
        }
        return;
    }
    const int step = (in.pressed(PSP_CTRL_DOWN) ? 1 : 0) - (in.pressed(PSP_CTRL_UP) ? 1 : 0);
    if (step == 0) return;
    m_achievementCursor = (m_achievementCursor + kAchievementCount + step) % kAchievementCount;
    playSfx(audio::Sfx::MenuMove, 0.6f);
}

// --- Tienda (v0.13; docs/TIENDA.md) -------------------------------------------------------------
// Los trajes de los personajes de la CPU, poco comunes, por 800 pavos. Arriba / abajo eligen
// (el personaje del lobby lo muestra), X compra (con confirmacion) o, si ya es tuyo, lo equipa.
constexpr float kShopX = 262.0f, kShopY = 56.0f, kShopW = 208.0f, kShopCardH = 58.0f, kShopGap = 6.0f;
const ui::Hint kShopHints[] = {{ui::Button::Up, T::None},       {ui::Button::Down, T::HintSelect},
                               {ui::Button::Cross, T::HintBuy}, {ui::Button::L, T::None},
                               {ui::Button::R, T::HintSections}};
const ui::Hint kShopConfirmHints[] = {{ui::Button::Cross, T::HintBuy}, {ui::Button::Circle, T::HintCancel}};

u32 rarityFill(game::Rarity rarity) {
    switch (rarity) {
        case game::Rarity::Uncommon: return rgba(54, 140, 46, 235);
        case game::Rarity::Rare: return rgba(40, 110, 200, 235);
        case game::Rarity::Epic: return rgba(130, 60, 190, 235);
        case game::Rarity::Legendary: return rgba(200, 120, 30, 235);
        default: return rgba(110, 116, 126, 235);
    }
}

const game::CharacterModel& App::modelFor(const Cosmetic& outfit) const {
    if (outfit.palette == &game::palettes::kCommando) return m_models[1];
    if (outfit.palette == &game::palettes::kOrange) return m_models[2];
    if (outfit.palette == &game::palettes::kRanger) return m_models[3];
    return m_models[0];
}

void App::equipOutfit(const Cosmetic& item) {
    const Cosmetic* before = m_profile.equippedIn(item.slot);
    m_profile.equip(item);
    saveProfile();
    // Otro traje: el modelo del jugador se rehace con su paleta.
    if (item.slot == CosmeticSlot::Outfit && item.palette && before != &item) {
        m_models[0].destroy();
        m_modelsReady &= m_models[0].build(*item.palette);
    }
}

void App::updateShopTab(const input::Input& in) {
    int items[8];
    const int n = shopItems(items, 8);
    if (n == 0) return;
    if (m_shopCursor >= n) m_shopCursor = 0;
    const Cosmetic& item = cosmetic(items[m_shopCursor]);
    if (m_shopConfirm) {
        if (in.pressed(PSP_CTRL_CROSS)) {
            m_shopConfirm = false;
            if (m_profile.buy(item)) {
                saveProfile();
                playSfx(audio::Sfx::MenuAccept);
                playSfx(audio::Sfx::Pickup, 0.9f);
                showLobbyMessage(T::ShopBought, false);
                platform::log::write("tienda: comprado %s (quedan %d pavos)", item.key, m_profile.vbucks);
            } else {
                showLobbyMessage(T::ShopNoVbucks, true);
            }
        } else if (in.pressed(PSP_CTRL_CIRCLE)) {
            m_shopConfirm = false;
            playSfx(audio::Sfx::MenuMove);
        }
        return;
    }
    const int step = (in.pressed(PSP_CTRL_DOWN) ? 1 : 0) - (in.pressed(PSP_CTRL_UP) ? 1 : 0);
    if (step != 0) {
        m_shopCursor = (m_shopCursor + n + step) % n;
        playSfx(audio::Sfx::MenuMove, 0.6f);
    }
    if (!in.pressed(PSP_CTRL_CROSS)) return;
    if (m_profile.owns(item)) {
        // Ya es tuyo: X lo equipa.
        equipOutfit(item);
        playSfx(audio::Sfx::MenuAccept);
        showLobbyMessage(T::ShopEquipped, false);
    } else if (m_profile.vbucks < item.price) {
        playSfx(audio::Sfx::MenuMove, 0.5f);
        showLobbyMessage(T::ShopNoVbucks, true);
    } else {
        m_shopConfirm = true;
        playSfx(audio::Sfx::MenuAccept, 0.7f);
    }
}

void App::renderShopTab() const {
    int items[8];
    const int n = shopItems(items, 8);
    gfx::fillRect(kShopX - 8, 36, kShopW + 12, 214, rgba(8, 18, 48, 200));
    for (int i = 0; i < n; ++i) {
        const Cosmetic& c = cosmetic(items[i]);
        const float y = kShopY + i * (kShopCardH + kShopGap);
        gfx::fillRect(kShopX, y, kShopW, kShopCardH, rarityFill(c.rarity));
        gfx::fillRect(kShopX, y + kShopCardH - 18, kShopW, 18, rgba(0, 0, 0, 110));
        // Silueta del traje: los colores de su paleta en franjas (la vista previa grande es el
        // personaje del lobby).
        if (c.palette) {
            gfx::fillRect(kShopX + 6, y + 6, 10, 10, c.palette->shirt);
            gfx::fillRect(kShopX + 6, y + 18, 10, 10, c.palette->pants);
            gfx::fillRect(kShopX + 6, y + 30, 10, 6, c.palette->skin);
        }
        if (!m_profile.owns(c) && m_atlas.ready()) m_atlas.draw(atlas::kPavos, kShopX + kShopW - 52, y + kShopCardH - 16);
        const bool selected = i == m_shopCursor;
        ui::frame(kShopX - 1, y - 1, kShopW + 2, kShopCardH + 2, selected ? 2 : 1, selected ? kYellow : rgba(255, 255, 255, 120));
    }
    if (m_shopConfirm) {
        gfx::fillRect(0, 0, gfx::kScreenWidth, gfx::kScreenHeight, rgba(0, 0, 0, 140));
        gfx::fillRect(100, 92, 280, 86, kNavy);
        ui::frame(100, 92, 280, 86, 2, kYellow);
        if (m_atlas.ready()) m_atlas.draw(atlas::kPavosBig, 128, 120);
        ui::drawHintIcons(m_atlas, centeredHintsX(kShopConfirmHints, 2), 160, kShopConfirmHints, 2);
    } else {
        ui::drawHintIcons(m_atlas, 8, kHintsY, kShopHints, 5);
    }
}

void App::overlayShopTab() const {
    int items[8];
    const int n = shopItems(items, 8);
    gfx::text::printShadow(static_cast<int>(kShopX), 40, kWhite, "%s", tr(T::ShopTitle));
    char line[64];
    // Con la confirmacion abierta solo su texto (el de las tarjetas la atravesaria).
    for (int i = 0; i < n && !m_shopConfirm; ++i) {
        const Cosmetic& c = cosmetic(items[i]);
        const int y = static_cast<int>(kShopY + i * (kShopCardH + kShopGap));
        gfx::text::printShadow(static_cast<int>(kShopX) + 22, y + 8, kWhite, "%s", tr(c.name));
        snprintf(line, sizeof(line), "%s | %s", game::rarityName(c.rarity), tr(cosmeticSlotName(c.slot)));
        gfx::text::print(static_cast<int>(kShopX) + 22, y + 22, rgba(225, 240, 225), "%s", line);
        const bool owned = m_profile.owns(c);
        const bool equipped = m_profile.equippedIn(c.slot) == &c;
        if (owned) {
            gfx::text::printShadow(static_cast<int>(kShopX) + 8, y + static_cast<int>(kShopCardH) - 13,
                                   equipped ? kYellow : kWhite, "%s", tr(equipped ? T::ShopEquippedTag : T::ShopOwned));
        } else {
            snprintf(line, sizeof(line), "%d", c.price);
            gfx::text::printShadow(static_cast<int>(kShopX + kShopW) - 34, y + static_cast<int>(kShopCardH) - 13,
                                   m_profile.vbucks >= c.price ? kWhite : kErrorRed, "%s", line);
        }
    }
    if (m_shopConfirm && m_shopCursor < n) {
        const Cosmetic& c = cosmetic(items[m_shopCursor]);
        snprintf(line, sizeof(line), tr(T::ShopConfirm), tr(c.name));
        printCentered(102, kWhite, line);
        snprintf(line, sizeof(line), tr(T::ShopPrice), c.price, m_profile.vbucks - c.price);
        gfx::text::printShadow(158, 128, kYellow, "%s", line);
        ui::printHints(centeredHintsX(kShopConfirmHints, 2), 160, kWhite, kShopConfirmHints, 2);
    } else {
        ui::printHints(8, kHintsY, kLightGrey, kShopHints, 5);
    }
}

// Taquilla: rejilla de 3 x 2 categorias (como Referencias/locker.jpg); X entra en una y
// elige objeto; X equipa; CIRCULO vuelve. Solo sale lo que se tiene (lo gratis y lo comprado).
constexpr int kLockerCols = 3;
constexpr float kLockerX = 262.0f, kLockerY = 64.0f, kLockerSlot = 60.0f, kLockerGap = 8.0f;

int App::lockerItems(CosmeticSlot slot, int* out, int max) const {
    int all[16];
    const int n = cosmeticsIn(slot, all, 16);
    int k = 0;
    for (int i = 0; i < n && k < max; ++i) {
        if (m_profile.owns(cosmetic(all[i]))) out[k++] = all[i];
    }
    return k;
}

void App::updateLockerTab(const input::Input& in) {
    const CosmeticSlot slot = static_cast<CosmeticSlot>(m_lockerSlot);
    int items[16];
    const int n = lockerItems(slot, items, 16);
    if (m_lockerChoosing) {
        const int step = (in.pressed(PSP_CTRL_DOWN) ? 1 : 0) - (in.pressed(PSP_CTRL_UP) ? 1 : 0);
        if (step != 0 && n > 0) {
            m_lockerItem = (m_lockerItem + n + step) % n;
            playSfx(audio::Sfx::MenuMove, 0.6f);
        }
        if (in.pressed(PSP_CTRL_CROSS) && n > 0) {
            equipOutfit(cosmetic(items[m_lockerItem]));
            playSfx(audio::Sfx::MenuAccept);
            m_lockerChoosing = false;
        } else if (in.pressed(PSP_CTRL_CIRCLE)) {
            m_lockerChoosing = false;
            playSfx(audio::Sfx::MenuMove);
        }
        return;
    }
    const int dx = (in.pressed(PSP_CTRL_RIGHT) ? 1 : 0) - (in.pressed(PSP_CTRL_LEFT) ? 1 : 0);
    const int dy = (in.pressed(PSP_CTRL_DOWN) ? 1 : 0) - (in.pressed(PSP_CTRL_UP) ? 1 : 0);
    if (dx || dy) {
        int col = m_lockerSlot % kLockerCols, row = m_lockerSlot / kLockerCols;
        col = (col + kLockerCols + dx) % kLockerCols;
        row = (row + 2 + dy) % 2;
        m_lockerSlot = row * kLockerCols + col;
        playSfx(audio::Sfx::MenuMove, 0.6f);
    }
    if (in.pressed(PSP_CTRL_CROSS)) {
        // Dentro de la categoria, empezando por lo equipado.
        m_lockerChoosing = true;
        m_lockerItem = 0;
        const Cosmetic* on = m_profile.equippedIn(slot);
        for (int i = 0; i < n; ++i) {
            if (&cosmetic(items[i]) == on) m_lockerItem = i;
        }
        playSfx(audio::Sfx::MenuAccept, 0.7f);
    }
}

// Pestanas en la barra superior (como PLAY / LOCKER en Fortnite), con L y R a los lados.
constexpr float kTabsX = 118.0f, kTabGap = 14.0f, kTabY = 8.0f;
const ui::Hint kTabLeft[] = {{ui::Button::L, T::None}};
const ui::Hint kTabRight[] = {{ui::Button::R, T::None}};
const ui::Hint kAchievementHints[] = {{ui::Button::Up, T::None},         {ui::Button::Down, T::HintScroll},
                                      {ui::Button::Cross, T::HintClaim}, {ui::Button::L, T::None},
                                      {ui::Button::R, T::HintSections}};
const ui::Hint kLockerHints[] = {{ui::Button::Dpad, T::HintSelect}, {ui::Button::Cross, T::HintOpen},
                                 {ui::Button::L, T::None}, {ui::Button::R, T::HintSections}};
const ui::Hint kLockerChooseHints[] = {{ui::Button::Up, T::None}, {ui::Button::Down, T::HintSelect},
                                       {ui::Button::Cross, T::HintEquip}, {ui::Button::Circle, T::HintBack}};

const assets::ui::Rect& tabLabel(int tab) {
    switch (tab) {
        case 1: return label(atlas::kTabLogros, atlas::kTabAchievements);
        case 2: return label(atlas::kTabTienda, atlas::kTabShop);
        case 3: return label(atlas::kTabTaquilla, atlas::kTabLocker);
        default: return label(atlas::kTabJugar, atlas::kTabPlay);
    }
}

// Recorre las pestanas: fn(indice, x, ancho).
template <typename Fn>
void forEachTab(Fn&& fn) {
    float x = kTabsX + ui::hintsWidth(kTabLeft, 1) + 8.0f;
    for (int i = 0; i < 4; ++i) {
        const assets::ui::Rect& r = tabLabel(i);
        fn(i, x, static_cast<float>(r.w));
        x += r.w + kTabGap;
    }
}

void App::renderLobbyTabs() const {
    gfx::fillRect(0, 0, gfx::kScreenWidth, 30, kNavy);
    gfx::fillRect(0, 30, gfx::kScreenWidth, 1, rgba(60, 90, 160));
    if (!m_atlas.ready()) return;
    m_atlas.draw(atlas::kLogoSmall, 6, 4, 0.62f);
    float end = 0.0f;
    const bool rewards = m_profile.claimableCount() > 0;
    forEachTab([&](int i, float x, float w) {
        const bool on = static_cast<int>(m_lobbyTab) == i;
        if (on) {
            gfx::fillRect(x - 5, 4, w + 10, 22, rgba(255, 255, 255, 235));
            m_atlas.draw(tabLabel(i), x, kTabY, 1.0f, rgba(16, 26, 58));
        } else {
            m_atlas.draw(tabLabel(i), x, kTabY, 1.0f, rgba(170, 190, 230));
        }
        // Recompensas por recoger: un punto amarillo en LOGROS.
        if (i == 1 && rewards) gfx::fillRect(x + w + 1, 6, 5, 5, kYellow);
        end = x + w;
    });
    ui::drawHintIcons(m_atlas, static_cast<int>(kTabsX), 11, kTabLeft, 1);
    ui::drawHintIcons(m_atlas, static_cast<int>(end + 10.0f), 11, kTabRight, 1);
}

void App::overlayLobbyTabs() const {
    if (!m_modeOpen) {
        gfx::text::printShadow(static_cast<int>(kVbucksX) + 20, static_cast<int>(kVbucksY) + 4, kWhite, "%d", m_profile.vbucks);
    }
    if (m_lobbyMessageTime > 0.0f) printCentered(220, m_lobbyMessageError ? kErrorRed : kYellow, tr(m_lobbyMessage));
}

void App::renderLobby() {
    // Composicion de Referencias/Lobby.png: barra superior azul marino con el logotipo y las
    // secciones, fondo azul con reticula, suelo de arena y el personaje en su plataforma. En
    // la tienda, el personaje es el traje elegido (vista previa).
    if (m_lobbyReady) m_lobbyScene.drawBackground(m_lobbyTime);
    if (m_lobbyReady && m_modelsReady) {
        const game::CharacterModel* model = &m_models[0];
        if (m_lobbyTab == LobbyTab::Shop) {
            int items[8];
            const int n = shopItems(items, 8);
            if (n > 0) model = &modelFor(cosmetic(items[m_shopCursor < n ? m_shopCursor : 0]));
        }
        m_lobbyScene.drawScene(*model, m_lobbyAnim, m_world.textures(), m_lobbyYaw, m_lobbyTime, &m_lobbyViewProjection);
        gfx::begin2D();
    }
    renderLobbyTabs();
    gfx::fillRect(0, 252, gfx::kScreenWidth, 20, rgba(10, 18, 44, 220));
    if (m_lobbyMessageTime > 0.0f) gfx::fillRect(0, 216, gfx::kScreenWidth, 16, rgba(0, 0, 0, 170));
    // Pavos, bajo la barra a la derecha (encima del panel de cada seccion).
    auto vbucksPill = [&]() {
        gfx::fillRect(kVbucksX, kVbucksY, kVbucksW, kVbucksH, rgba(8, 18, 48, 235));
        ui::frame(kVbucksX, kVbucksY, kVbucksW, kVbucksH, 1, rgba(90, 160, 255));
        if (m_atlas.ready()) m_atlas.draw(atlas::kPavos, kVbucksX + 2, kVbucksY + 1);
    };
    if (m_lobbyTab == LobbyTab::Achievements) {
        renderAchievementsTab();
        vbucksPill();
    } else if (m_lobbyTab == LobbyTab::Shop) {
        renderShopTab();
        vbucksPill();
    } else if (m_lobbyTab == LobbyTab::Locker) {
        renderLockerTab();
        vbucksPill();
    } else {
        vbucksPill();
        if (m_atlas.ready()) {
            const atlas::Rect& map = label(atlas::kMapa, atlas::kMapaEn);
            m_atlas.draw(map, kLobbyColumnX + (kLobbyColumnW - map.w * 0.82f) * 0.5f, 53, 0.82f);
        }
        // Columna: JUGAR (amarillo, late), modo y bots, CONTROLES y SALIR; cada uno con su boton.
        drawButton(kLobbyColumnX, kPlayButton.y, kLobbyColumnW, kPlayButton.h, kYellow, !m_confirmExit && !m_modeOpen,
                   m_lobbyTime);
        gfx::fillRect(kLobbyColumnX, kModeBox.y, kLobbyColumnW, kModeBox.h, rgba(0, 0, 0, 150));
        ui::frame(kLobbyColumnX, kModeBox.y, kLobbyColumnW, kModeBox.h, 1, rgba(90, 160, 255));
        drawButton(kLobbyColumnX, kControlsButton.y, kLobbyColumnW, kControlsButton.h, rgba(20, 44, 100, 220), false,
                   m_lobbyTime);
        drawButton(kLobbyColumnX, kExitButton.y, kLobbyColumnW, kExitButton.h, rgba(20, 44, 100, 220), false, m_lobbyTime);
        if (m_atlas.ready()) {
            const float cx = kLobbyColumnX + kLobbyColumnW * 0.5f + 8.0f;
            m_atlas.drawCentered(label(atlas::kJugar, atlas::kPlayEn), cx, kPlayButton.y + 4);
            m_atlas.drawCentered(label(atlas::kControles, atlas::kControlsEn), cx, kControlsButton.y + 1, 0.9f);
            m_atlas.drawCentered(label(atlas::kSalir, atlas::kExitEn), cx, kExitButton.y + 1, 0.9f);
        }
        const int bx = static_cast<int>(kLobbyColumnX) + 6;
        ui::drawHintIcons(m_atlas, bx, static_cast<int>(kPlayButton.y) + 16, kPlayHint, 1);
        ui::drawHintIcons(m_atlas, static_cast<int>(kLobbyColumnX + kLobbyColumnW) - 20, static_cast<int>(kModeBox.y) + 15,
                          kModeHint, 1);
        ui::drawHintIcons(m_atlas, bx, static_cast<int>(kControlsButton.y) + 8, kControlsHint, 1);
        ui::drawHintIcons(m_atlas, bx, static_cast<int>(kExitButton.y) + 8, kExitHint, 1);
        ui::drawHintIcons(m_atlas, 8, kHintsY, kLobbyHints, kLobbyHintCount);
        // Panel de estadisticas del perfil (el texto, en overlayLobby).
        gfx::fillRect(kStatsX, kStatsY, kStatsW, kStatsH, rgba(8, 18, 48, 200));
        ui::frame(kStatsX, kStatsY, kStatsW, kStatsH, 1, rgba(90, 160, 255));
        if (m_modeOpen) renderModePanel();
    }

    if (m_confirmExit) {
        gfx::fillRect(0, 0, gfx::kScreenWidth, gfx::kScreenHeight, rgba(0, 0, 0, 140));
        gfx::fillRect(120, 100, 240, 72, kNavy);
        ui::frame(120, 100, 240, 72, 2, rgba(90, 160, 255));
        ui::drawHintIcons(m_atlas, centeredHintsX(kExitHints, kExitHintCount), 140, kExitHints, kExitHintCount);
    }
    if (m_settingsOpen) {
        if (m_modMenuOpen) renderModMenu();
        else renderSettings();
    }
}

void App::overlayLobby() const {
    // Con los ajustes abiertos solo se escribe su texto (el del lobby lo atravesaria: el
    // texto va por CPU encima de todo lo dibujado con GU).
    if (m_settingsOpen) {
        if (m_modMenuOpen) overlayModMenu();
        else overlaySettings();
        return;
    }
    if (m_lobbyTab == LobbyTab::Achievements) {
        overlayAchievementsTab();
    } else if (m_lobbyTab == LobbyTab::Shop) {
        overlayShopTab();
    } else if (m_lobbyTab == LobbyTab::Locker) {
        overlayLockerTab();
    } else if (m_modeOpen) {
        overlayModePanel();
    } else {
        // Nombre sobre la cabeza del personaje, como en la referencia.
        float x = 0.0f, y = 0.0f;
        if (m_modelsReady && projectToScreen(m_lobbyViewProjection, LobbyScene::namePoint(), &x, &y)) {
            printCenteredAt(static_cast<int>(x), static_cast<int>(y), kWhite, m_profile.name);
        }
        // Estadisticas del perfil (perfil.txt), en el panel de la izquierda.
        gfx::text::printShadow(kStatsX + 4, kStatsY + 4, config::kColorGold, "%s", m_profile.name);
        const struct {
            T label;
            int value;
        } stats[] = {{T::StatWins, m_profile.wins},   {T::StatMatches, m_profile.matches},
                     {T::StatKills, m_profile.kills}, {T::StatHeadshots, m_profile.headshots},
                     {T::StatMostKills, m_profile.mostKills}};
        for (int k = 0; k < 5; ++k) {
            gfx::text::print(kStatsX + 4, kStatsY + 16 + 10 * k, kLightGrey, "%s", tr(stats[k].label));
            gfx::text::print(kStatsX + 86, kStatsY + 16 + 10 * k, kWhite, "%4d", stats[k].value);
        }
        gfx::text::print(kStatsX + 4, kStatsY + 66, kLightGrey, "%s", tr(T::StatBestPlace));
        if (m_profile.bestPlace > 0) gfx::text::print(kStatsX + 94, kStatsY + 66, kWhite, "#%d", m_profile.bestPlace);
        else gfx::text::print(kStatsX + 110, kStatsY + 66, kWhite, "-");
        // Modo y bots elegidos (CUADRADO los cambia).
        const int cx = static_cast<int>(kLobbyColumnX + kLobbyColumnW * 0.5f) - 6;
        printCenteredAt(cx, static_cast<int>(kModeBox.y) + 7, kYellow, tr(modeName(m_gameMode)));
        char bots[32];
        if (m_gameMode == game::GameMode::Zombies) snprintf(bots, sizeof(bots), "%s", tr(T::BotsZombies));
        else snprintf(bots, sizeof(bots), tr(T::BotsSelector), m_botCount);
        const bool practice = m_gameMode != game::GameMode::Zombies && m_botCount < game::kRewardBots;
        printCenteredAt(cx, static_cast<int>(kModeBox.y) + 21, practice ? kErrorRed : kWhite, bots);
        if (practice) printCenteredAt(240, 222, kErrorRed, tr(T::PracticeShort));
        if (!m_worldReady) printCenteredAt(cx + 6, 232, kErrorRed, tr(T::NoMapError));
        ui::printHints(8, kHintsY, kLightGrey, kLobbyHints, kLobbyHintCount);
    }
    overlayLobbyTabs();
    gfx::text::print(400, 258, kDimGrey, "v%s", config::kVersion);

    if (m_confirmExit) {
        printCentered(114, kWhite, tr(T::ExitQuestion));
        ui::printHints(centeredHintsX(kExitHints, kExitHintCount), 140, config::kColorGold, kExitHints, kExitHintCount);
    }
}

// --- Logros -------------------------------------------------------------------------------

int achievementScroll(int cursor) {
    // La lista se desplaza para que el elegido siempre se vea.
    int top = cursor - kAchievementRows / 2;
    if (top > kAchievementCount - kAchievementRows) top = kAchievementCount - kAchievementRows;
    return top < 0 ? 0 : top;
}

void App::renderAchievementsTab() const {
    gfx::fillRect(kListX - 4, 36, kListW + 8, 214, rgba(8, 18, 48, 225));
    ui::frame(kListX - 4, 36, kListW + 8, 214, 1, rgba(90, 160, 255));
    const int top = achievementScroll(m_achievementCursor);
    for (int r = 0; r < kAchievementRows; ++r) {
        const int i = top + r;
        const AchievementState& st = m_profile.achievements[i];
        const AchievementDef& def = achievementDef(static_cast<AchievementId>(i));
        const float y = kListY + r * kRowH;
        const bool on = i == m_achievementCursor;
        gfx::fillRect(kListX, y, kListW, kRowH - 3, on ? rgba(36, 92, 196, 230) : rgba(20, 34, 80, 200));
        if (on) ui::frame(kListX - 1, y - 1, kListW + 2, kRowH - 1, 1, kWhite);
        // Insignia: dorada si esta desbloqueado.
        gfx::fillRect(kListX + 4, y + 4, 20, 20, st.unlocked ? kYellow : rgba(70, 80, 100));
        gfx::fillRect(kListX + 8, y + 8, 12, 12, st.unlocked ? rgba(255, 240, 160) : rgba(40, 46, 60));
        // Recompensa: pavos por recoger (amarillo) o ya recogidos (atenuado).
        if (m_atlas.ready()) {
            m_atlas.draw(atlas::kPavos, kListX + 254, y + 4, 1.0f, st.claimed ? rgba(255, 255, 255, 90) : kWhite);
        }
        if (st.unlocked && !st.claimed) gfx::fillRect(kListX + 252, y + 2, 46, 18, rgba(250, 216, 40, 70));
        // Barra de progreso.
        const int value = m_profile.stat(def.stat);
        const float progress = st.unlocked ? 1.0f : math3d::clampf(static_cast<float>(value) / def.goal, 0.0f, 1.0f);
        gfx::fillRect(kListX + 300, y + 17, 140, 6, rgba(0, 0, 0, 160));
        gfx::fillRect(kListX + 300, y + 17, 140 * progress, 6, st.unlocked ? kYellow : kFortniteBlue);
    }
    // Barra de desplazamiento.
    const float h = 186.0f * kAchievementRows / kAchievementCount;
    gfx::fillRect(kListX + kListW + 1, kListY + 186.0f * top / kAchievementCount, 2, h, rgba(255, 255, 255, 160));
    ui::drawHintIcons(m_atlas, 8, kHintsY, kAchievementHints, 5);
}

void App::overlayAchievementsTab() const {
    char line[64];
    snprintf(line, sizeof(line), tr(T::AchievementsTitle), m_profile.unlockedCount(), kAchievementCount);
    gfx::text::printShadow(static_cast<int>(kListX) + 2, 40, config::kColorGold, "%s", line);
    if (m_profile.claimableCount() > 0) {
        snprintf(line, sizeof(line), tr(T::RewardsPending), m_profile.claimableCount());
        gfx::text::printShadow(200, 40, kYellow, "%s", line);
    }
    const int top = achievementScroll(m_achievementCursor);
    for (int r = 0; r < kAchievementRows; ++r) {
        const int i = top + r;
        const AchievementState& st = m_profile.achievements[i];
        const AchievementDef& def = achievementDef(static_cast<AchievementId>(i));
        const int y = static_cast<int>(kListY + r * kRowH);
        gfx::text::printShadow(static_cast<int>(kListX) + 30, y + 5, st.unlocked ? kWhite : kLightGrey, "%s", tr(def.name));
        gfx::text::print(static_cast<int>(kListX) + 30, y + 16, st.unlocked ? kLightGrey : kDimGrey, "%s",
                         tr(def.description));
        gfx::text::printShadow(static_cast<int>(kListX) + 270, y + 7, st.claimed ? kDimGrey : (st.unlocked ? kYellow : kWhite),
                               "+%d", kVbucksPerAchievement);
        if (st.unlocked) {
            if (st.date > 0) {
                snprintf(line, sizeof(line), "%02d/%02d/%04d", st.date % 100, (st.date / 100) % 100, st.date / 10000);
            } else {
                snprintf(line, sizeof(line), "%s", tr(T::AchievementDone));
            }
        } else {
            const int value = m_profile.stat(def.stat);
            snprintf(line, sizeof(line), "%d / %d", value < def.goal ? value : def.goal, def.goal);
        }
        gfx::text::printShadow(static_cast<int>(kListX) + 440 - gfx::text::width(line), y + 5,
                               st.unlocked ? kYellow : kWhite, "%s", line);
    }
    ui::printHints(8, kHintsY, kLightGrey, kAchievementHints, 5);
}

// --- Taquilla -----------------------------------------------------------------------------

void App::renderLockerTab() const {
    // Rejilla de categorias a la derecha (el personaje, a la izquierda, hace de vista previa).
    gfx::fillRect(kLockerX - 8, 36, 214, 214, rgba(8, 18, 48, 200));
    gfx::fillRect(kLockerX - 8, 40, 58, 14, rgba(36, 92, 196));
    for (int i = 0; i < kCosmeticSlotCount; ++i) {
        const float x = kLockerX + (i % kLockerCols) * (kLockerSlot + kLockerGap);
        const float y = kLockerY + (i / kLockerCols) * (kLockerSlot + kLockerGap + 10.0f);
        const Cosmetic* on = m_profile.equippedIn(static_cast<CosmeticSlot>(i));
        gfx::fillRect(x, y, kLockerSlot, kLockerSlot, on ? rgba(60, 120, 210, 230) : rgba(40, 70, 130, 170));
        // Vacia: una cruz "+" como las casillas sin objeto de la referencia.
        if (!on) {
            gfx::fillRect(x + kLockerSlot - 16, y + kLockerSlot - 10, 10, 2, kWhite);
            gfx::fillRect(x + kLockerSlot - 12, y + kLockerSlot - 14, 2, 10, kWhite);
        }
        if (static_cast<CosmeticSlot>(i) == CosmeticSlot::Pickaxe && m_atlas.ready()) {
            m_atlas.draw(atlas::kPico, x + 17, y + 14);
        }
        if (static_cast<CosmeticSlot>(i) == CosmeticSlot::Outfit && m_atlas.ready()) {
            if (on && on->palette && on->price > 0) {
                // Traje comprado: los colores de su paleta.
                gfx::fillRect(x + 22, y + 12, 16, 14, on->palette->shirt);
                gfx::fillRect(x + 22, y + 28, 16, 14, on->palette->pants);
            } else {
                m_atlas.draw(atlas::kIconVivos, x + 16, y + 16, 2.0f);
            }
        }
        const bool selected = i == m_lockerSlot;
        ui::frame(x - 1, y - 1, kLockerSlot + 2, kLockerSlot + 2, selected ? 2 : 1,
                  selected ? kYellow : rgba(255, 255, 255, 120));
    }
    // Detalle (como "COMUN | PICO" de la referencia), abajo a la izquierda.
    gfx::fillRect(8, 196, 230, 50, rgba(160, 170, 190, 200));
    if (m_lockerChoosing) {
        // La lista de la categoria tapa la rejilla mientras se elige.
        int items[16];
        const int n = lockerItems(static_cast<CosmeticSlot>(m_lockerSlot), items, 16);
        gfx::fillRect(kLockerX - 4, kLockerY - 4, 206, 144, rgba(10, 20, 50, 245));
        ui::frame(kLockerX - 4, kLockerY - 4, 206, 144, 1, kYellow);
        if (n > 0) gfx::fillRect(kLockerX, kLockerY + 14 + m_lockerItem * 14.0f, 198, 12, rgba(36, 92, 196));
        ui::drawHintIcons(m_atlas, 8, kHintsY, kLockerChooseHints, 4);
    } else {
        ui::drawHintIcons(m_atlas, 8, kHintsY, kLockerHints, 4);
    }
}

void App::overlayLockerTab() const {
    gfx::text::printShadow(static_cast<int>(kLockerX) - 4, 43, kWhite, "%s", tr(T::LockerGear));
    for (int i = 0; i < kCosmeticSlotCount && !m_lockerChoosing; ++i) {
        const int x = static_cast<int>(kLockerX + (i % kLockerCols) * (kLockerSlot + kLockerGap));
        const int y = static_cast<int>(kLockerY + (i / kLockerCols) * (kLockerSlot + kLockerGap + 10.0f));
        printCenteredAt(x + static_cast<int>(kLockerSlot) / 2, y + static_cast<int>(kLockerSlot) + 2, kLightGrey,
                        i18n::tr(cosmeticSlotName(static_cast<CosmeticSlot>(i))));
    }
    const CosmeticSlot slot = static_cast<CosmeticSlot>(m_lockerSlot);
    const Cosmetic* on = m_profile.equippedIn(slot);
    char line[64];
    if (on) {
        snprintf(line, sizeof(line), "%s | %s", game::rarityName(on->rarity), i18n::tr(cosmeticSlotName(slot)));
        gfx::text::print(14, 202, rgba(40, 40, 50), "%s", line);
        gfx::text::print(14, 216, rgba(16, 26, 58), "%s", tr(on->name));
        gfx::text::print(14, 230, rgba(60, 60, 70), "%s", tr(on->price > 0 ? T::LockerBought : T::LockerDefault));
    } else {
        gfx::text::print(14, 202, rgba(40, 40, 50), "%s", i18n::tr(cosmeticSlotName(slot)));
        // Categoria sin objetos todavia: mensaje en vez de una lista vacia.
        gfx::text::print(14, 216, rgba(16, 26, 58), "%s", tr(T::LockerEmptyLine1));
        gfx::text::print(14, 230, rgba(60, 60, 70), "%s", tr(T::LockerEmptyLine2));
    }
    if (m_lockerChoosing) {
        int items[16];
        const int n = lockerItems(slot, items, 16);
        const int listY = static_cast<int>(kLockerY);
        gfx::text::printShadow(static_cast<int>(kLockerX), listY, kYellow, "%s", i18n::tr(cosmeticSlotName(slot)));
        if (n == 0) {
            gfx::text::print(static_cast<int>(kLockerX), listY + 16, kLightGrey, "%s", tr(T::LockerEmptyList1));
            gfx::text::print(static_cast<int>(kLockerX), listY + 28, kLightGrey, "%s", tr(T::LockerEmptyList2));
            gfx::text::print(static_cast<int>(kLockerX), listY + 40, kLightGrey, "%s", tr(T::LockerEmptyList3));
        }
        for (int i = 0; i < n; ++i) {
            // Lo equipado, en amarillo y con una marca.
            const Cosmetic& c = cosmetic(items[i]);
            gfx::text::print(static_cast<int>(kLockerX) + 4, listY + 16 + i * 14, &c == on ? kYellow : kWhite, "%s%s",
                             &c == on ? "* " : "  ", tr(c.name));
        }
        ui::printHints(8, kHintsY, kLightGrey, kLockerChooseHints, 4);
    } else {
        ui::printHints(8, kHintsY, kLightGrey, kLockerHints, 4);
    }
}

// --- Controles --------------------------------------------------------------------------

void App::updateControlsScreen(const input::Input& in) {
    if (in.pressed(PSP_CTRL_CIRCLE) || in.pressed(PSP_CTRL_START)) {
        playSfx(audio::Sfx::MenuMove);
        m_screen = Screen::Lobby;
    }
}

void App::renderControlsScreen(const FrameInfo& frame) const {
    if (m_lobbyReady) m_lobbyScene.drawBackground(m_lobbyTime);
    renderControlsPage(frame);
}

void App::overlayControlsScreen(const FrameInfo& frame) const { overlayControlsPage(frame); }

// --- Carga ------------------------------------------------------------------------------

void App::startLoading() {
    m_screen = Screen::Loading;
    m_loadingTimer = 0.0f;
    m_loadingFrames = 0;
    m_loadingStarted = false;
    m_paused = false;
    m_results = Results::None;
    m_tip = (m_tip + 1) % kTipCount;
}

void App::updateLoading(float dt) {
    m_loadingTimer += dt;
    ++m_loadingFrames;
    // La partida se prepara cuando la pantalla de carga ya se ha mostrado (frame 2).
    if (!m_loadingStarted && m_loadingFrames >= 2) {
        startMatch();
        m_loadingStarted = true;
    }
    if (m_loadingStarted && m_loadingTimer >= kLoadingSeconds) {
        m_screen = Screen::Game;
        if (m_audioReady) audio::mixer().playMusic(false);
    }
}

void App::renderLoading() const {
    if (m_loadscreen.pixels) gfx::drawTexture(m_loadscreen, 0.0f, 0.0f);
    gfx::fillRect(0, 232, gfx::kScreenWidth, 40, rgba(8, 14, 36, 220));
    if (m_atlas.ready()) m_atlas.draw(label(atlas::kCargando, atlas::kCargandoEn), 12, 244, 0.85f);
    if (kTips[m_tip].hasButton) ui::drawHintIcons(m_atlas, kTipX, kTipY, &kTips[m_tip].hint, 1);
    float t = m_loadingTimer / kLoadingSeconds;
    if (t > 1.0f) t = 1.0f;
    if (!m_loadingStarted && t > 0.4f) t = 0.4f;
    gfx::fillRect(100, 252, 368, 6, rgba(255, 255, 255, 50));
    gfx::fillRect(100, 252, 368 * t, 6, kYellow);
}

void App::overlayLoading() const {
    const Tip& tip = kTips[m_tip];
    if (tip.hasButton) ui::printHints(kTipX, kTipY, kLightGrey, &tip.hint, 1);
    else gfx::text::printShadow(kTipX, kTipY, kLightGrey, "%s", tip.hint.label());
    if (m_gameMode == game::GameMode::Zombies) gfx::text::printShadow(100, 262, kDimGrey, "%s", tr(T::LoadingZombies));
    else gfx::text::printShadow(100, 262, kDimGrey, tr(T::LoadingMap), m_botCount);
}

// --- Pausa ------------------------------------------------------------------------------

void App::updatePause(const input::Input& in) {
    // Mod menu abierto desde la pausa: los trucos se aplican a la partida al momento.
    if (m_modMenuOpen) {
        updateModMenu(in);
        return;
    }
    const int items = m_settings.admin ? 3 : 2;
    if (m_pauseItem >= items) m_pauseItem = 0;
    const int step = (in.pressed(PSP_CTRL_DOWN) ? 1 : 0) - (in.pressed(PSP_CTRL_UP) ? 1 : 0);
    if (step != 0) {
        m_pauseItem = (m_pauseItem + items + step) % items;
        playSfx(audio::Sfx::MenuMove);
    }
    const bool resume = in.pressed(PSP_CTRL_START) || in.pressed(PSP_CTRL_CIRCLE) ||
                        (in.pressed(PSP_CTRL_CROSS) && m_pauseItem == 0);
    if (resume) {
        m_paused = false;
        playSfx(audio::Sfx::MenuMove);
    } else if (in.pressed(PSP_CTRL_CROSS) && m_pauseItem == 2) {
        openModMenu();
    } else if (in.pressed(PSP_CTRL_CROSS)) {
        playSfx(audio::Sfx::MenuAccept);
        recordMatch();  // abandonar tambien cuenta
        goToLobby();
    }
}

void App::renderPause() const {
    if (m_modMenuOpen) {
        renderModMenu();
        return;
    }
    const int items = m_settings.admin ? 3 : 2;
    const float h = 50.0f + 34.0f * items;
    gfx::fillRect(0, 0, gfx::kScreenWidth, gfx::kScreenHeight, rgba(0, 0, 0, 150));
    gfx::fillRect(140, 74, 200, h, kNavy);
    gfx::fillRect(140, 74, 200, 22, kFortniteBlue);
    for (int i = 0; i < items; ++i) {
        const bool selected = m_pauseItem == i;
        const u32 idle = i == 2 ? rgba(70, 20, 110) : rgba(20, 44, 100);
        drawButton(156, 110 + i * 34, 168, 24, selected ? kYellow : idle, selected, m_blinkTime);
    }
    ui::drawHintIcons(m_atlas, centeredHintsX(kPauseHints, kPauseHintCount), static_cast<int>(74 + h) - 16, kPauseHints,
                      kPauseHintCount);
}

void App::overlayPause() const {
    if (m_modMenuOpen) {
        overlayModMenu();
        return;
    }
    const int items = m_settings.admin ? 3 : 2;
    const float h = 50.0f + 34.0f * items;
    printCentered(81, kWhite, tr(T::Pause));
    for (int i = 0; i < items; ++i) {
        printCentered(118 + i * 34, m_pauseItem == i ? kNavy : kWhite, tr(kPauseItems[i]));
    }
    ui::printHints(centeredHintsX(kPauseHints, kPauseHintCount), static_cast<int>(74 + h) - 16, kLightGrey, kPauseHints,
                   kPauseHintCount);
}

// --- Resultados -------------------------------------------------------------------------

void App::updateResults(const input::Input& in, float dt) {
    const game::MatchPhase phase = m_match.phase();
    if (m_results == Results::None) {
        // Quien siguio mirando tras caer vuelve a ver los resultados al acabar la partida.
        if (m_spectating && phase == game::MatchPhase::Finished) {
            m_spectating = false;
            m_resultsDismissed = false;
            m_resultsDelay = kDefeatDelay;
        }
        if (m_resultsDismissed) return;
        const bool won = phase == game::MatchPhase::Finished && m_match.winner() == 0;
        if (!won && m_match.alive(0)) return;
        m_resultsDelay += dt;
        if (m_resultsDelay < (won ? kVictoryDelay : kDefeatDelay)) return;
        m_results = won ? Results::Victory : Results::Defeat;
        m_resultsTime = 0.0f;
        m_buildMode = false;
        m_freeCameraActive = false;
        // Un solo sonido de resultado por partida (no se repite al volver de espectador) y
        // sin efectos de la partida debajo (zumbido de cofre, disparos lejanos...).
        if (m_audioReady) audio::mixer().stopAll();
        m_chestHumOn = false;
        if (!m_resultSoundPlayed) playSfx(won ? audio::Sfx::Victory : audio::Sfx::Defeat);
        recordMatch();
        m_resultSoundPlayed = true;
        return;
    }

    m_resultsTime += dt;
    if (m_resultsTime < kResultsInputDelay) return;
    if (in.pressed(PSP_CTRL_CROSS)) {
        playSfx(audio::Sfx::MenuAccept);
        goToLobby();
    } else if (in.pressed(PSP_CTRL_SQUARE)) {
        playSfx(audio::Sfx::MenuAccept);
        startLoading();
    } else if (in.pressed(PSP_CTRL_CIRCLE) && m_results == Results::Defeat &&
               phase != game::MatchPhase::Finished) {
        playSfx(audio::Sfx::MenuMove);
        m_results = Results::None;
        m_spectating = true;
        m_resultsDismissed = true;
    }
}

int App::resultsHintCount() const {
    // "Seguir mirando" solo en la derrota mientras la partida sigue.
    return m_results == Results::Defeat && m_match.phase() != game::MatchPhase::Finished ? 3 : 2;
}

void App::renderResults() const {
    if (m_match.zombieMode()) {
        renderZombieResults();
        gfx::fillRect(0, 222, gfx::kScreenWidth, 30, rgba(0, 0, 0, 150));
        if (m_resultsTime >= kResultsInputDelay) ui::drawHintIcons(m_atlas, centeredHintsX(kResultsHints, 2), 233, kResultsHints, 2);
        return;
    }
    const bool victory = m_results == Results::Victory;
    gfx::fillRect(0, 0, gfx::kScreenWidth, gfx::kScreenHeight, rgba(0, 0, 0, 90));
    gfx::fillRect(0, 34, gfx::kScreenWidth, 82, victory ? rgba(20, 44, 120, 200) : rgba(70, 8, 8, 200));
    // El rotulo entra grande y se asienta (escala 2.2 -> 1.5 en 0.25 s).
    if (m_atlas.ready()) {
        const float t = m_resultsTime / 0.25f;
        const float scale = t >= 1.0f ? 1.5f : 2.2f - 0.7f * t;
        const atlas::Rect& banner = victory ? atlas::kVictory : label(atlas::kDefeat, atlas::kDefeatEn);
        m_atlas.drawCentered(banner, 240.0f, 75.0f - banner.h * scale * 0.5f, scale);
    }

    // Tres columnas: puesto, bajas y tiempo.
    gfx::fillRect(84, 124, 312, 64, kPanel);
    // Pavos ganados (victoria magistral), bajo las bajas.
    if (victory && m_matchVbucks > 0 && m_atlas.ready()) {
        char line[48];
        snprintf(line, sizeof(line), tr(T::ResultVbucks), m_matchVbucks);
        m_atlas.draw(atlas::kPavos, 258 - gfx::text::width(line) / 2 - 17, 173);
    }
    for (int i = 1; i < 3; ++i) gfx::fillRect(84 + 104 * i, 130, 1, 52, rgba(255, 255, 255, 60));
    const int place = victory ? 1 : m_match.playerPlace();
    const u32 gold = config::kColorGold;
    int w = bigNumberWidth(place, 4.0f);
    drawBigNumber(136 - w * 0.5f + 6, 148, place, 4.0f, victory ? gold : kWhite);
    w = bigNumberWidth(m_match.playerKills(), 4.0f);
    drawBigNumber(240 - w * 0.5f, 148, m_match.playerKills(), 4.0f, kWhite);

    // Opciones.
    gfx::fillRect(0, 222, gfx::kScreenWidth, 30, rgba(0, 0, 0, 150));
    if (m_resultsTime >= kResultsInputDelay) {
        const int n = resultsHintCount();
        ui::drawHintIcons(m_atlas, centeredHintsX(kResultsHints, n), 233, kResultsHints, n);
    }
}

void App::overlayResults() const {
    if (m_match.zombieMode()) {
        overlayZombieResults();
        if (m_resultsTime >= kResultsInputDelay) ui::printHints(centeredHintsX(kResultsHints, 2), 233, kWhite, kResultsHints, 2);
        return;
    }
    const bool victory = m_results == Results::Victory;
    const game::MatchPhase phase = m_match.phase();
    gfx::text::printShadow(98, 130, kLightGrey, "%s", tr(T::ResultPlace));
    gfx::text::printShadow(118, 152, victory ? config::kColorGold : kWhite, "#");
    gfx::text::printShadow(218, 130, kLightGrey, "%s", tr(T::ResultKills));
    gfx::text::printShadow(318, 130, kLightGrey, "%s", tr(T::ResultTime));
    char time[16];
    formatTime(m_match.elapsed(), time, sizeof(time));
    printCenteredAt(344, 154, kWhite, time);
    char line[64];
    snprintf(line, sizeof(line), tr(T::ResultOfPlayers), m_match.entityCount());
    printCenteredAt(136, 176, kDimGrey, line);

    if (victory) {
        printCentered(196, config::kColorGold, tr(T::ResultSurvived));
        if (m_matchVbucks > 0) {
            snprintf(line, sizeof(line), tr(T::ResultVbucks), m_matchVbucks);
            gfx::text::printShadow(258 - gfx::text::width(line) / 2, 176, kYellow, "%s", line);
        }
    } else {
        if (m_playerKiller > 0) {
            snprintf(line, sizeof(line), tr(T::ResultKilledBy), m_match.name(m_playerKiller));
        } else if (m_playerKiller == game::kStormKiller) {
            snprintf(line, sizeof(line), "%s", tr(T::ResultKilledByStorm));
        } else {
            snprintf(line, sizeof(line), "%s", tr(T::ResultFell));
        }
        printCentered(194, kErrorRed, line);
        if (phase == game::MatchPhase::Finished) {
            snprintf(line, sizeof(line), tr(T::ResultWinner),
                     m_match.winner() >= 0 ? m_match.name(m_match.winner()) : tr(T::ResultNobody));
        } else {
            snprintf(line, sizeof(line), tr(T::ResultAlive), m_match.aliveCount());
        }
        printCentered(206, kWhite, line);
    }

    if (!m_rewardsActive) printCentered(214, kErrorRed, tr(T::PracticeMatch));  // menos de 30 bots
    if (m_resultsTime < kResultsInputDelay) return;
    const int n = resultsHintCount();
    ui::printHints(centeredHintsX(kResultsHints, n), 233, kWhite, kResultsHints, n);
}

}  // namespace app
