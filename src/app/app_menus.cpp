// Pantallas fuera de la partida y superpuestas a ella: inicio de sesion, lobby, controles,
// carga, pausa y resultados (victoria / derrota).
#include "app/app.h"

#include <math.h>
#include <string.h>

#include "app/app_common.h"
#include "audio/audio_system.h"
#include "platform/log.h"

namespace app {
namespace {

using namespace common;
namespace atlas = assets::ui;

constexpr float kConnectSeconds = 1.8f;    // "Conectando..." del inicio de sesion
constexpr float kLoadingSeconds = 2.0f;    // pantalla de carga minima antes de la partida
constexpr float kVictoryDelay = 1.0f;      // s desde la ultima baja hasta la pantalla
constexpr float kDefeatDelay = 2.5f;       // s viendo la propia caida antes de la pantalla
constexpr float kResultsInputDelay = 0.8f; // evita saltarse los resultados disparando
constexpr int kMinBots = 5;
constexpr int kStatsX = 4, kStatsY = 44, kStatsW = 122, kStatsH = 78;  // panel del perfil en el lobby

const char* const kLoginSteps[] = {"Conectando...", "Comprobando la version...", "Cargando el perfil..."};

// Consejos de la pantalla de carga: con el boton dibujado cuando hablan de uno.
struct Tip {
    bool hasButton;
    ui::Hint hint;
};
const Tip kTips[] = {
    {true, {ui::Button::Circle, "construir: una pared te cubre al instante."}},
    {true, {ui::Button::L, "mantenido: fija al enemigo mas cercano."}},
    {false, {ui::Button::Cross, "La zona se cierra hacia el deposito: no te quedes fuera."}},
    {true, {ui::Button::Square, "recarga la SCAR: 30 balas por cargador."}},
    {false, {ui::Button::Cross, "Las paredes paran las balas, pero se pueden romper."}},
    {true, {ui::Button::Triangle, "saca el pico: los arboles dan madera."}},
};
constexpr int kTipCount = sizeof(kTips) / sizeof(kTips[0]);
constexpr int kTipX = 100, kTipY = 238;

// Botones del lobby (columna derecha, como el boton JUGAR de Fortnite).
constexpr float kLobbyColumnX = 312.0f;
constexpr float kLobbyColumnW = 152.0f;
struct LobbyButton {
    float y, h;
};
constexpr LobbyButton kLobbyButtons[] = {{70.0f, 42.0f}, {146.0f, 28.0f}, {180.0f, 28.0f}};
constexpr float kBotsSelectorY = 116.0f;

// Ayudas de controles (iconos de Recursos/Texturas PSP).
const ui::Hint kLobbyHints[] = {{ui::Button::Dpad, "elegir"}, {ui::Button::Cross, "aceptar"}, {ui::Button::Stick, "girar"}};
const ui::Hint kExitHints[] = {{ui::Button::Cross, "salir"}, {ui::Button::Circle, "cancelar"}};
const ui::Hint kPauseHints[] = {{ui::Button::Start, ""}, {ui::Button::Circle, "continuar"}};
const ui::Hint kResultsHints[] = {{ui::Button::Cross, "lobby"}, {ui::Button::Square, "otra partida"},
                                  {ui::Button::Circle, "seguir mirando"}};
constexpr int kLobbyHintCount = 3, kExitHintCount = 2, kPauseHintCount = 2;
constexpr int kHintsY = 258;

int centeredHintsX(const ui::Hint* hints, int count) { return (gfx::kScreenWidth - ui::hintsWidth(hints, count)) / 2; }

// Opciones de la pausa.
constexpr const char* kPauseItems[] = {"CONTINUAR", "VOLVER AL LOBBY"};

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
const ui::Hint kNickHints[] = {{ui::Button::Cross, "escribir"}, {ui::Button::Circle, "borrar"},
                               {ui::Button::Triangle, "mayus/minus"}, {ui::Button::Start, "aceptar"}};

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
    printCentered(46, kWhite, "ELIGE TU NOMBRE DE JUGADOR");
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
    printCenteredAt(static_cast<int>(kNickKeyX) + half / 2, y, kWhite, "BORRAR");
    printCenteredAt(static_cast<int>(kNickKeyX) + half + 2 + half / 2, y, kWhite, "ACEPTAR");
    printCentered(226, kLightGrey, "Se guarda tu nombre y tus estadisticas");
    ui::printHints(8, kHintsY, kLightGrey, kNickHints, 4);
}

void App::recordMatch() {
    // Suma la partida al perfil (una vez por partida: al ver los resultados o al abandonar).
    // Las bajas, los disparos a la cabeza y el resto de contadores de los logros ya se han
    // sumado durante la partida (App::processEvents).
    if (m_matchRecorded) return;
    m_matchRecorded = true;
    const bool won = m_match.phase() == game::MatchPhase::Finished && m_match.winner() == 0;
    const int kills = m_match.playerKills();
    ++m_profile.matches;
    if (won) ++m_profile.wins;
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
    m_profile.mutableStat(stat) += amount;
    m_profileDirty = true;
    checkAchievements();
}

void App::checkAchievements() {
    AchievementId fresh[kAchievementCount];
    const int n = m_profile.unlockReached(todayDate(), fresh, kAchievementCount);
    if (n <= 0) return;
    for (int i = 0; i < n; ++i) {
        platform::log::write("logro desbloqueado: %s", achievementDef(fresh[i]).name);
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

void App::renderAchievementToast() const {
    if (m_toastCount == 0) return;
    // Entra deslizandose desde arriba.
    const float in = fminf(1.0f, m_toastTime / 0.2f);
    const float y = kToastY - (1.0f - in) * 40.0f;
    gfx::fillRect(kToastX, y, kToastW, kToastH, rgba(8, 18, 48, 235));
    gfx::fillRect(kToastX, y, 6, kToastH, kYellow);
    ui::frame(kToastX, y, kToastW, kToastH, 1, kYellow);
}

void App::overlayAchievementToast() const {
    if (m_toastCount == 0) return;
    const float in = fminf(1.0f, m_toastTime / 0.2f);
    const int y = static_cast<int>(kToastY - (1.0f - in) * 40.0f);
    printCenteredAt(243, y + 5, kYellow, "LOGRO DESBLOQUEADO");
    printCenteredAt(243, y + 17, kWhite, achievementDef(m_toasts[0]).name);
}

// Pantalla de inicio: la imagen de Recursos/loginscreen.png, "Pulsa (X) para iniciar" bajo
// el logotipo y, al pulsar, una barra de carga (la conexion es simulada).
constexpr int kLoginCenterX = 158;  // centro del logotipo en la imagen
constexpr int kLoginTextY = 236;
constexpr const char* kLoginPrompt = "Pulsa ";
const ui::Hint kLoginHint[] = {{ui::Button::Cross, "para iniciar"}};

int loginPromptX() {
    return kLoginCenterX - (gfx::text::width(kLoginPrompt) + ui::hintsWidth(kLoginHint, 1)) / 2;
}

void App::renderLogin() const {
    if (m_login.pixels) gfx::drawTexture(m_login, 0.0f, 0.0f);
    gfx::fillRect(0, 224, gfx::kScreenWidth, 36, rgba(8, 18, 48, 170));
    if (m_loginStage == 0) {
        // Parpadea suavemente, como el "pulsa para empezar" de las consolas.
        if (fmodf(m_blinkTime, 1.2f) < 0.85f) {
            ui::drawHintIcons(m_atlas, loginPromptX() + gfx::text::width(kLoginPrompt), kLoginTextY, kLoginHint, 1);
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
            gfx::text::printShadow(x, kLoginTextY, kWhite, "%s", kLoginPrompt);
            ui::printHints(x + gfx::text::width(kLoginPrompt), kLoginTextY, kWhite, kLoginHint, 1);
        }
    } else {
        int step = static_cast<int>(m_loginTimer / (kConnectSeconds / 3.0f));
        if (step > 2) step = 2;
        printCenteredAt(kLoginCenterX, 234, kWhite, kLoginSteps[step]);
    }
}

// --- Lobby ------------------------------------------------------------------------------

void App::updateLobby(const input::Input& in, float dt) {
    m_lobbyTime += dt;
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

    // L / R: seccion del lobby (jugar, logros, taquilla). En la partida L y R son apuntar
    // y disparar: no hay conflicto, el lobby solo existe fuera de ella.
    const int tabs = static_cast<int>(LobbyTab::Count);
    const int tabStep = (in.pressed(PSP_CTRL_RTRIGGER) ? 1 : 0) - (in.pressed(PSP_CTRL_LTRIGGER) ? 1 : 0);
    if (tabStep != 0 && !in.held(PSP_CTRL_START)) {
        m_lobbyTab = static_cast<LobbyTab>((static_cast<int>(m_lobbyTab) + tabs + tabStep) % tabs);
        m_lockerChoosing = false;
        playSfx(audio::Sfx::MenuMove);
        return;
    }
    switch (m_lobbyTab) {
        case LobbyTab::Play: updateLobbyPlay(in); break;
        case LobbyTab::Achievements: updateAchievementsTab(in); break;
        case LobbyTab::Locker: updateLockerTab(in); break;
        case LobbyTab::Count: break;
    }
}

void App::updateLobbyPlay(const input::Input& in) {
    const int count = static_cast<int>(LobbyItem::Count);
    int item = static_cast<int>(m_lobbyItem);
    if (in.pressed(PSP_CTRL_UP)) item = (item + count - 1) % count;
    if (in.pressed(PSP_CTRL_DOWN)) item = (item + 1) % count;
    if (item != static_cast<int>(m_lobbyItem)) {
        m_lobbyItem = static_cast<LobbyItem>(item);
        playSfx(audio::Sfx::MenuMove);
    }
    if (m_lobbyItem == LobbyItem::Play) {
        const int before = m_botCount;
        if (in.pressed(PSP_CTRL_LEFT) && m_botCount > kMinBots) m_botCount -= 5;
        if (in.pressed(PSP_CTRL_RIGHT) && m_botCount < game::kMaxBots) m_botCount += 5;
        if (before != m_botCount) playSfx(audio::Sfx::MenuMove);
    }
    if (!in.pressed(PSP_CTRL_CROSS)) return;
    switch (m_lobbyItem) {
        case LobbyItem::Play:
            if (!m_worldReady) break;
            playSfx(audio::Sfx::MenuAccept);
            startLoading();
            break;
        case LobbyItem::Controls:
            playSfx(audio::Sfx::MenuAccept);
            m_screen = Screen::Controls;
            break;
        case LobbyItem::Exit:
            playSfx(audio::Sfx::MenuAccept);
            m_confirmExit = true;
            break;
        case LobbyItem::Count:
            break;
    }
}

// Logros: lista con su progreso (arriba / abajo la recorren).
constexpr int kAchievementRows = 6;
constexpr float kListX = 14.0f, kListY = 52.0f, kListW = 452.0f, kRowH = 31.0f;

void App::updateAchievementsTab(const input::Input& in) {
    const int step = (in.pressed(PSP_CTRL_DOWN) ? 1 : 0) - (in.pressed(PSP_CTRL_UP) ? 1 : 0);
    if (step == 0) return;
    m_achievementCursor = (m_achievementCursor + kAchievementCount + step) % kAchievementCount;
    playSfx(audio::Sfx::MenuMove, 0.6f);
}

// Taquilla: rejilla de 3 x 2 categorias (como Referencias/locker.jpg); X entra en una y
// elige objeto; X equipa; CIRCULO vuelve.
constexpr int kLockerCols = 3;
constexpr float kLockerX = 262.0f, kLockerY = 64.0f, kLockerSlot = 60.0f, kLockerGap = 8.0f;

void App::updateLockerTab(const input::Input& in) {
    const CosmeticSlot slot = static_cast<CosmeticSlot>(m_lockerSlot);
    int items[16];
    const int n = cosmeticsIn(slot, items, 16);
    if (m_lockerChoosing) {
        const int step = (in.pressed(PSP_CTRL_DOWN) ? 1 : 0) - (in.pressed(PSP_CTRL_UP) ? 1 : 0);
        if (step != 0 && n > 0) {
            m_lockerItem = (m_lockerItem + n + step) % n;
            playSfx(audio::Sfx::MenuMove, 0.6f);
        }
        if (in.pressed(PSP_CTRL_CROSS) && n > 0) {
            const Cosmetic& item = cosmetic(items[m_lockerItem]);
            const Cosmetic* before = m_profile.equippedIn(slot);
            m_profile.equip(item);
            saveProfile();
            // Otro traje: el modelo del jugador se rehace con su paleta.
            if (slot == CosmeticSlot::Outfit && item.palette && before != &item) {
                m_models[0].destroy();
                m_modelsReady &= m_models[0].build(*item.palette);
            }
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
constexpr float kTabsX = 150.0f, kTabGap = 18.0f, kTabY = 8.0f;
const ui::Hint kTabLeft[] = {{ui::Button::L, ""}};
const ui::Hint kTabRight[] = {{ui::Button::R, ""}};
const ui::Hint kAchievementHints[] = {{ui::Button::Up, ""}, {ui::Button::Down, "recorrer"}, {ui::Button::L, ""},
                                      {ui::Button::R, "secciones"}};
const ui::Hint kLockerHints[] = {{ui::Button::Dpad, "elegir"}, {ui::Button::Cross, "abrir"}, {ui::Button::L, ""},
                                 {ui::Button::R, "secciones"}};
const ui::Hint kLockerChooseHints[] = {{ui::Button::Up, ""}, {ui::Button::Down, "elegir"}, {ui::Button::Cross, "equipar"},
                                       {ui::Button::Circle, "volver"}};

const assets::ui::Rect& tabLabel(int tab) {
    switch (tab) {
        case 1: return atlas::kTabLogros;
        case 2: return atlas::kTabTaquilla;
        default: return atlas::kTabJugar;
    }
}

// Recorre las pestanas: fn(indice, x, ancho).
template <typename Fn>
void forEachTab(Fn&& fn) {
    float x = kTabsX + ui::hintsWidth(kTabLeft, 1) + 8.0f;
    for (int i = 0; i < 3; ++i) {
        const assets::ui::Rect& r = tabLabel(i);
        fn(i, x, static_cast<float>(r.w));
        x += r.w + kTabGap;
    }
}

void App::renderLobbyTabs() const {
    gfx::fillRect(0, 0, gfx::kScreenWidth, 30, kNavy);
    gfx::fillRect(0, 30, gfx::kScreenWidth, 1, rgba(60, 90, 160));
    if (!m_atlas.ready()) return;
    m_atlas.draw(atlas::kLogoSmall, 8, 3, 0.7f);
    float end = 0.0f;
    forEachTab([&](int i, float x, float w) {
        const bool on = static_cast<int>(m_lobbyTab) == i;
        if (on) {
            gfx::fillRect(x - 5, 4, w + 10, 22, rgba(255, 255, 255, 235));
            m_atlas.draw(tabLabel(i), x, kTabY, 1.0f, rgba(16, 26, 58));
        } else {
            m_atlas.draw(tabLabel(i), x, kTabY, 1.0f, rgba(170, 190, 230));
        }
        end = x + w;
    });
    ui::drawHintIcons(m_atlas, static_cast<int>(kTabsX), 11, kTabLeft, 1);
    ui::drawHintIcons(m_atlas, static_cast<int>(end + 10.0f), 11, kTabRight, 1);
}

void App::overlayLobbyTabs() const {}

void App::renderLobby() {
    // Composicion de Referencias/Lobby.png: barra superior azul marino con el logotipo y las
    // secciones, fondo azul con reticula, suelo de arena y el personaje en su plataforma.
    if (m_lobbyReady) m_lobbyScene.drawBackground(m_lobbyTime);
    if (m_lobbyReady && m_modelsReady) {
        m_lobbyScene.drawScene(m_models[0], m_lobbyAnim, m_world.textures(), m_lobbyYaw, m_lobbyTime,
                               &m_lobbyViewProjection);
        gfx::begin2D();
    }
    renderLobbyTabs();
    gfx::fillRect(0, 252, gfx::kScreenWidth, 20, rgba(10, 18, 44, 220));
    if (m_lobbyTab == LobbyTab::Achievements) {
        renderAchievementsTab();
    } else if (m_lobbyTab == LobbyTab::Locker) {
        renderLockerTab();
    } else {
        if (m_atlas.ready()) m_atlas.draw(atlas::kMapa, kLobbyColumnX + (kLobbyColumnW - atlas::kMapa.w * 0.82f) * 0.5f, 44, 0.82f);
        // Columna de botones: JUGAR (amarillo), selector de bots, CONTROLES y SALIR.
        for (int i = 0; i < static_cast<int>(LobbyItem::Count); ++i) {
            const LobbyButton& b = kLobbyButtons[i];
            const bool selected = static_cast<int>(m_lobbyItem) == i && !m_confirmExit;
            const u32 fill = i == 0 ? (selected ? kYellow : kYellowDark)
                                    : (selected ? kFortniteBlue : rgba(20, 44, 100, 220));
            drawButton(kLobbyColumnX, b.y, kLobbyColumnW, b.h, fill, selected, m_lobbyTime);
        }
        gfx::fillRect(kLobbyColumnX, kBotsSelectorY, kLobbyColumnW, 22, rgba(0, 0, 0, 120));
        if (m_atlas.ready()) {
            const float cx = kLobbyColumnX + kLobbyColumnW * 0.5f;
            m_atlas.drawCentered(atlas::kJugar, cx, kLobbyButtons[0].y + 6);
            m_atlas.drawCentered(atlas::kControles, cx, kLobbyButtons[1].y + 3);
            m_atlas.drawCentered(atlas::kSalir, cx, kLobbyButtons[2].y + 3);
        }
        ui::drawHintIcons(m_atlas, 8, kHintsY, kLobbyHints, kLobbyHintCount);
        // Panel de estadisticas del perfil (el texto, en overlayLobby).
        gfx::fillRect(kStatsX, kStatsY, kStatsW, kStatsH, rgba(8, 18, 48, 200));
        ui::frame(kStatsX, kStatsY, kStatsW, kStatsH, 1, rgba(90, 160, 255));
    }

    if (m_confirmExit) {
        gfx::fillRect(0, 0, gfx::kScreenWidth, gfx::kScreenHeight, rgba(0, 0, 0, 140));
        gfx::fillRect(120, 100, 240, 72, kNavy);
        ui::frame(120, 100, 240, 72, 2, rgba(90, 160, 255));
        ui::drawHintIcons(m_atlas, centeredHintsX(kExitHints, kExitHintCount), 140, kExitHints, kExitHintCount);
    }
}

void App::overlayLobby() const {
    overlayLobbyTabs();
    if (m_lobbyTab == LobbyTab::Achievements) {
        overlayAchievementsTab();
    } else if (m_lobbyTab == LobbyTab::Locker) {
        overlayLockerTab();
    } else {
        // Nombre sobre la cabeza del personaje, como en la referencia.
        float x = 0.0f, y = 0.0f;
        if (m_modelsReady && projectToScreen(m_lobbyViewProjection, LobbyScene::namePoint(), &x, &y)) {
            printCenteredAt(static_cast<int>(x), static_cast<int>(y), kWhite, m_profile.name);
        }
        // Estadisticas del perfil (perfil.txt), en el panel de la izquierda.
        gfx::text::printShadow(kStatsX + 4, kStatsY + 4, config::kColorGold, "%s", m_profile.name);
        const struct {
            const char* label;
            int value;
        } stats[] = {{"Victorias", m_profile.wins},   {"Partidas", m_profile.matches},   {"Bajas", m_profile.kills},
                     {"A la cabeza", m_profile.headshots}, {"Max. bajas", m_profile.mostKills}};
        for (int k = 0; k < 5; ++k) {
            gfx::text::print(kStatsX + 4, kStatsY + 16 + 10 * k, kLightGrey, "%s", stats[k].label);
            gfx::text::print(kStatsX + 86, kStatsY + 16 + 10 * k, kWhite, "%4d", stats[k].value);
        }
        gfx::text::print(kStatsX + 4, kStatsY + 66, kLightGrey, "Mejor puesto");
        if (m_profile.bestPlace > 0) gfx::text::print(kStatsX + 94, kStatsY + 66, kWhite, "#%d", m_profile.bestPlace);
        else gfx::text::print(kStatsX + 110, kStatsY + 66, kWhite, "-");
        const int cx = static_cast<int>(kLobbyColumnX + kLobbyColumnW * 0.5f);
        char bots[24];
        snprintf(bots, sizeof(bots), "<  BOTS: %d  >", m_botCount);
        printCenteredAt(cx, static_cast<int>(kBotsSelectorY) + 7,
                        m_botCount > game::kDefaultBots ? kErrorRed : kWhite, bots);
        if (m_botCount > game::kDefaultBots) printCenteredAt(cx, 216, kLightGrey, "(mas de 10: mide FPS)");
        if (!m_worldReady) printCenteredAt(cx, 232, kErrorRed, "ERROR: sin escenario");
        ui::printHints(8, kHintsY, kLightGrey, kLobbyHints, kLobbyHintCount);
    }
    gfx::text::print(400, 258, kDimGrey, "v%s", config::kVersion);

    if (m_confirmExit) {
        printCentered(114, kWhite, "Salir de FortnitePSP?");
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
        // Barra de progreso.
        const int value = m_profile.stat(def.stat);
        const float progress = st.unlocked ? 1.0f : math3d::clampf(static_cast<float>(value) / def.goal, 0.0f, 1.0f);
        gfx::fillRect(kListX + 300, y + 17, 140, 6, rgba(0, 0, 0, 160));
        gfx::fillRect(kListX + 300, y + 17, 140 * progress, 6, st.unlocked ? kYellow : kFortniteBlue);
    }
    // Barra de desplazamiento.
    const float h = 186.0f * kAchievementRows / kAchievementCount;
    gfx::fillRect(kListX + kListW + 1, kListY + 186.0f * top / kAchievementCount, 2, h, rgba(255, 255, 255, 160));
    ui::drawHintIcons(m_atlas, 8, kHintsY, kAchievementHints, 4);
}

void App::overlayAchievementsTab() const {
    char line[64];
    snprintf(line, sizeof(line), "LOGROS  %d / %d", m_profile.unlockedCount(), kAchievementCount);
    gfx::text::printShadow(static_cast<int>(kListX) + 2, 40, config::kColorGold, "%s", line);
    const int top = achievementScroll(m_achievementCursor);
    for (int r = 0; r < kAchievementRows; ++r) {
        const int i = top + r;
        const AchievementState& st = m_profile.achievements[i];
        const AchievementDef& def = achievementDef(static_cast<AchievementId>(i));
        const int y = static_cast<int>(kListY + r * kRowH);
        gfx::text::printShadow(static_cast<int>(kListX) + 30, y + 5, st.unlocked ? kWhite : kLightGrey, "%s", def.name);
        gfx::text::print(static_cast<int>(kListX) + 30, y + 16, st.unlocked ? kLightGrey : kDimGrey, "%s", def.description);
        if (st.unlocked) {
            if (st.date > 0) {
                snprintf(line, sizeof(line), "%02d/%02d/%04d", st.date % 100, (st.date / 100) % 100, st.date / 10000);
            } else {
                snprintf(line, sizeof(line), "CONSEGUIDO");
            }
        } else {
            const int value = m_profile.stat(def.stat);
            snprintf(line, sizeof(line), "%d / %d", value < def.goal ? value : def.goal, def.goal);
        }
        gfx::text::printShadow(static_cast<int>(kListX) + 440 - gfx::text::width(line), y + 5,
                               st.unlocked ? kYellow : kWhite, "%s", line);
    }
    ui::printHints(8, kHintsY, kLightGrey, kAchievementHints, 4);
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
            m_atlas.draw(atlas::kIconVivos, x + 16, y + 16, 2.0f);
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
        const int n = cosmeticsIn(static_cast<CosmeticSlot>(m_lockerSlot), items, 16);
        gfx::fillRect(kLockerX - 4, kLockerY - 4, 206, 144, rgba(10, 20, 50, 245));
        ui::frame(kLockerX - 4, kLockerY - 4, 206, 144, 1, kYellow);
        if (n > 0) gfx::fillRect(kLockerX, kLockerY + 14 + m_lockerItem * 14.0f, 198, 12, rgba(36, 92, 196));
        ui::drawHintIcons(m_atlas, 8, kHintsY, kLockerChooseHints, 4);
    } else {
        ui::drawHintIcons(m_atlas, 8, kHintsY, kLockerHints, 4);
    }
}

void App::overlayLockerTab() const {
    gfx::text::printShadow(static_cast<int>(kLockerX) - 4, 43, kWhite, "EQUIPO");
    for (int i = 0; i < kCosmeticSlotCount && !m_lockerChoosing; ++i) {
        const int x = static_cast<int>(kLockerX + (i % kLockerCols) * (kLockerSlot + kLockerGap));
        const int y = static_cast<int>(kLockerY + (i / kLockerCols) * (kLockerSlot + kLockerGap + 10.0f));
        printCenteredAt(x + static_cast<int>(kLockerSlot) / 2, y + static_cast<int>(kLockerSlot) + 2, kLightGrey,
                        cosmeticSlotName(static_cast<CosmeticSlot>(i)));
    }
    const CosmeticSlot slot = static_cast<CosmeticSlot>(m_lockerSlot);
    const Cosmetic* on = m_profile.equippedIn(slot);
    char line[64];
    if (on) {
        snprintf(line, sizeof(line), "%s | %s", game::rarityName(on->rarity), cosmeticSlotName(slot));
        gfx::text::print(14, 202, rgba(40, 40, 50), "%s", line);
        gfx::text::print(14, 216, rgba(16, 26, 58), "%s", on->name);
        gfx::text::print(14, 230, rgba(60, 60, 70), "Por defecto");
    } else {
        gfx::text::print(14, 202, rgba(40, 40, 50), "%s", cosmeticSlotName(slot));
        // Categoria sin objetos todavia: mensaje en vez de una lista vacia.
        gfx::text::print(14, 216, rgba(16, 26, 58), "Aun no hay objetos aqui.");
        gfx::text::print(14, 230, rgba(60, 60, 70), "Llegaran en una actualizacion.");
    }
    if (m_lockerChoosing) {
        int items[16];
        const int n = cosmeticsIn(slot, items, 16);
        const int listY = static_cast<int>(kLockerY);
        gfx::text::printShadow(static_cast<int>(kLockerX), listY, kYellow, "%s", cosmeticSlotName(slot));
        if (n == 0) {
            gfx::text::print(static_cast<int>(kLockerX), listY + 16, kLightGrey, "Vacio: aun no hay objetos.");
            gfx::text::print(static_cast<int>(kLockerX), listY + 28, kLightGrey, "Llegaran en una");
            gfx::text::print(static_cast<int>(kLockerX), listY + 40, kLightGrey, "actualizacion.");
        }
        for (int i = 0; i < n; ++i) {
            // Lo equipado, en amarillo y con una marca.
            const Cosmetic& c = cosmetic(items[i]);
            gfx::text::print(static_cast<int>(kLockerX) + 4, listY + 16 + i * 14, &c == on ? kYellow : kWhite, "%s%s",
                             &c == on ? "* " : "  ", c.name);
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
    if (m_atlas.ready()) m_atlas.draw(atlas::kCargando, 12, 244, 0.85f);
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
    else gfx::text::printShadow(kTipX, kTipY, kLightGrey, "%s", tip.hint.text);
    gfx::text::printShadow(100, 262, kDimGrey, "Polvorin Polvoriento - %d bots", m_botCount);
}

// --- Pausa ------------------------------------------------------------------------------

void App::updatePause(const input::Input& in) {
    if (in.pressed(PSP_CTRL_UP) || in.pressed(PSP_CTRL_DOWN)) {
        m_pauseItem = 1 - m_pauseItem;
        playSfx(audio::Sfx::MenuMove);
    }
    const bool resume = in.pressed(PSP_CTRL_START) || in.pressed(PSP_CTRL_CIRCLE) ||
                        (in.pressed(PSP_CTRL_CROSS) && m_pauseItem == 0);
    if (resume) {
        m_paused = false;
        playSfx(audio::Sfx::MenuMove);
    } else if (in.pressed(PSP_CTRL_CROSS)) {
        playSfx(audio::Sfx::MenuAccept);
        recordMatch();  // abandonar tambien cuenta
        goToLobby();
    }
}

void App::renderPause() const {
    gfx::fillRect(0, 0, gfx::kScreenWidth, gfx::kScreenHeight, rgba(0, 0, 0, 150));
    gfx::fillRect(140, 74, 200, 118, kNavy);
    gfx::fillRect(140, 74, 200, 22, kFortniteBlue);
    for (int i = 0; i < 2; ++i) {
        const bool selected = m_pauseItem == i;
        drawButton(156, 110 + i * 34, 168, 24, selected ? kYellow : rgba(20, 44, 100), selected, m_blinkTime);
    }
    ui::drawHintIcons(m_atlas, centeredHintsX(kPauseHints, kPauseHintCount), 176, kPauseHints, kPauseHintCount);
}

void App::overlayPause() const {
    printCentered(81, kWhite, "PAUSA");
    for (int i = 0; i < 2; ++i) {
        printCentered(118 + i * 34, m_pauseItem == i ? kNavy : kWhite, kPauseItems[i]);
    }
    ui::printHints(centeredHintsX(kPauseHints, kPauseHintCount), 176, kLightGrey, kPauseHints, kPauseHintCount);
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
    const bool victory = m_results == Results::Victory;
    gfx::fillRect(0, 0, gfx::kScreenWidth, gfx::kScreenHeight, rgba(0, 0, 0, 90));
    gfx::fillRect(0, 34, gfx::kScreenWidth, 82, victory ? rgba(20, 44, 120, 200) : rgba(70, 8, 8, 200));
    // El rotulo entra grande y se asienta (escala 2.2 -> 1.5 en 0.25 s).
    if (m_atlas.ready()) {
        const float t = m_resultsTime / 0.25f;
        const float scale = t >= 1.0f ? 1.5f : 2.2f - 0.7f * t;
        const atlas::Rect& banner = victory ? atlas::kVictory : atlas::kDefeat;
        m_atlas.drawCentered(banner, 240.0f, 75.0f - banner.h * scale * 0.5f, scale);
    }

    // Tres columnas: puesto, bajas y tiempo.
    gfx::fillRect(84, 124, 312, 64, kPanel);
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
    const bool victory = m_results == Results::Victory;
    const game::MatchPhase phase = m_match.phase();
    gfx::text::printShadow(98, 130, kLightGrey, "PUESTO");
    gfx::text::printShadow(118, 152, victory ? config::kColorGold : kWhite, "#");
    gfx::text::printShadow(218, 130, kLightGrey, "BAJAS");
    gfx::text::printShadow(318, 130, kLightGrey, "TIEMPO");
    char time[16];
    formatTime(m_match.elapsed(), time, sizeof(time));
    printCenteredAt(344, 154, kWhite, time);
    char line[64];
    snprintf(line, sizeof(line), "de %d jugadores", m_match.entityCount());
    printCenteredAt(136, 176, kDimGrey, line);

    if (victory) {
        printCentered(196, config::kColorGold, "Has sobrevivido a todos!");
    } else {
        if (m_playerKiller > 0) {
            snprintf(line, sizeof(line), "Te ha eliminado %s", m_match.name(m_playerKiller));
        } else if (m_playerKiller == game::kStormKiller) {
            snprintf(line, sizeof(line), "Te ha eliminado la tormenta");
        } else {
            snprintf(line, sizeof(line), "Has caido");
        }
        printCentered(194, kErrorRed, line);
        if (phase == game::MatchPhase::Finished) {
            snprintf(line, sizeof(line), "Ganador: %s",
                     m_match.winner() >= 0 ? m_match.name(m_match.winner()) : "nadie");
        } else {
            snprintf(line, sizeof(line), "Quedan %d con vida", m_match.aliveCount());
        }
        printCentered(206, kWhite, line);
    }

    if (m_resultsTime < kResultsInputDelay) return;
    const int n = resultsHintCount();
    ui::printHints(centeredHintsX(kResultsHints, n), 233, kWhite, kResultsHints, n);
}

}  // namespace app
