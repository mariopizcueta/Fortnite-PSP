// Pantallas fuera de la partida y superpuestas a ella: inicio de sesion, lobby, controles,
// carga, pausa y resultados (victoria / derrota).
#include "app/app.h"

#include <math.h>

#include "app/app_common.h"
#include "audio/audio_system.h"

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

const char* const kLoginSteps[] = {"Conectando...", "Comprobando la version...", "Cargando el perfil..."};

const char* const kTips[] = {
    "Pulsa O para construir: una pared te cubre al instante.",
    "Manten L para fijar al enemigo mas cercano.",
    "La zona se cierra hacia el deposito: no te quedes fuera.",
    "[] recarga la SCAR: 30 balas por cargador.",
    "Las paredes paran las balas, pero se pueden romper.",
};
constexpr int kTipCount = sizeof(kTips) / sizeof(kTips[0]);

// Botones del lobby (columna derecha, como el boton JUGAR de Fortnite).
constexpr float kLobbyColumnX = 312.0f;
constexpr float kLobbyColumnW = 152.0f;
struct LobbyButton {
    float y, h;
};
constexpr LobbyButton kLobbyButtons[] = {{70.0f, 42.0f}, {146.0f, 28.0f}, {180.0f, 28.0f}};
constexpr float kBotsSelectorY = 116.0f;

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
        goToLobby();
    }
}

void App::renderLogin() const {
    if (m_login.pixels) gfx::drawTexture(m_login, 0.0f, 0.0f);
    // Panel de acceso bajo el logotipo (Referencias: pantalla de inicio de Fortnite).
    gfx::fillRect(24, 150, 268, 118, rgba(8, 18, 48, 215));
    gfx::fillRect(24, 150, 268, 2, rgba(90, 160, 255));
    gfx::fillRect(36, 170, 244, 18, rgba(236, 240, 248));
    gfx::fillRect(36, 194, 244, 18, rgba(236, 240, 248));
    if (m_loginStage == 0) {
        drawButton(36, 220, 244, 32, kYellow, true, m_blinkTime);
        if (m_atlas.ready()) m_atlas.drawCentered(atlas::kIniciarSesion, 158, 223);
    } else {
        drawButton(36, 220, 244, 32, kYellowDark, false, m_blinkTime);
        // Barra de progreso que se llena durante la "conexion".
        const float t = m_loginTimer / kConnectSeconds;
        gfx::fillRect(44, 244, 228, 4, rgba(0, 0, 0, 120));
        gfx::fillRect(44, 244, 228 * (t < 1.0f ? t : 1.0f), 4, kWhite);
    }
}

void App::overlayLogin() const {
    gfx::text::printShadow(6, 4, kLightGrey, "v%s", config::kVersion);
    gfx::text::printShadow(36, 157, kWhite, "Inicia sesion con tu cuenta");
    gfx::text::print(42, 175, kDimGrey, "Usuario:");
    gfx::text::print(42 + 8 * 12, 175, kNavy, "%s", kPlayerName);
    gfx::text::print(42, 199, kDimGrey, "Contrasena:");
    gfx::text::print(42 + 8 * 12, 199, kNavy, "********");
    if (m_loginStage == 0) {
        gfx::text::printShadow(36, 257, kLightGrey, "X: iniciar sesion");
    } else {
        int step = static_cast<int>(m_loginTimer / (kConnectSeconds / 3.0f));
        if (step > 2) step = 2;
        gfx::text::print(48, 228, kWhite, "%s", kLoginSteps[step]);
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

void App::renderLobby() {
    // Composicion de Referencias/Lobby.png: barra superior azul marino con el logotipo y el
    // mapa, fondo azul con reticula, suelo de arena y el personaje en su plataforma.
    if (m_lobbyReady) m_lobbyScene.drawBackground(m_lobbyTime);
    if (m_lobbyReady && m_modelsReady) {
        m_lobbyScene.drawScene(m_models[0], m_lobbyAnim, m_world.textures(), m_lobbyYaw, m_lobbyTime,
                               &m_lobbyViewProjection);
        gfx::begin2D();
    }
    gfx::fillRect(0, 0, gfx::kScreenWidth, 30, kNavy);
    gfx::fillRect(0, 30, gfx::kScreenWidth, 1, rgba(60, 90, 160));
    if (m_atlas.ready()) {
        m_atlas.draw(atlas::kLogoSmall, 8, 3, 0.7f);
        m_atlas.draw(atlas::kMapa, gfx::kScreenWidth - 8 - atlas::kMapa.w * 0.9f, 5, 0.9f);
    }

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
    gfx::fillRect(0, 252, gfx::kScreenWidth, 20, rgba(10, 18, 44, 220));

    if (m_confirmExit) {
        gfx::fillRect(0, 0, gfx::kScreenWidth, gfx::kScreenHeight, rgba(0, 0, 0, 140));
        gfx::fillRect(120, 100, 240, 72, kNavy);
        ui::frame(120, 100, 240, 72, 2, rgba(90, 160, 255));
    }
}

void App::overlayLobby() const {
    // Nombre sobre la cabeza del personaje, como en la referencia.
    float x = 0.0f, y = 0.0f;
    if (m_modelsReady && projectToScreen(m_lobbyViewProjection, LobbyScene::namePoint(), &x, &y)) {
        printCenteredAt(static_cast<int>(x), static_cast<int>(y), kWhite, kPlayerName);
    }
    const int cx = static_cast<int>(kLobbyColumnX + kLobbyColumnW * 0.5f);
    char bots[24];
    snprintf(bots, sizeof(bots), "<  BOTS: %d  >", m_botCount);
    printCenteredAt(cx, static_cast<int>(kBotsSelectorY) + 7,
                    m_botCount > game::kDefaultBots ? kErrorRed : kWhite, bots);
    if (m_botCount > game::kDefaultBots) printCenteredAt(cx, 216, kLightGrey, "(mas de 10: mide FPS)");
    if (!m_worldReady) printCenteredAt(cx, 232, kErrorRed, "ERROR: sin escenario");

    gfx::text::print(8, 258, kLightGrey, "Cruceta: elegir  X: aceptar  Stick: girar");
    gfx::text::print(400, 258, kDimGrey, "v%s", config::kVersion);

    if (m_confirmExit) {
        printCentered(114, kWhite, "Salir de FortnitePSP?");
        printCentered(136, config::kColorGold, "X: salir      O: cancelar");
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
    float t = m_loadingTimer / kLoadingSeconds;
    if (t > 1.0f) t = 1.0f;
    if (!m_loadingStarted && t > 0.4f) t = 0.4f;
    gfx::fillRect(100, 252, 368, 6, rgba(255, 255, 255, 50));
    gfx::fillRect(100, 252, 368 * t, 6, kYellow);
}

void App::overlayLoading() const {
    gfx::text::printShadow(100, 238, kLightGrey, "%s", kTips[m_tip]);
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
}

void App::overlayPause() const {
    printCentered(81, kWhite, "PAUSA");
    for (int i = 0; i < 2; ++i) {
        printCentered(118 + i * 34, m_pauseItem == i ? kNavy : kWhite, kPauseItems[i]);
    }
    printCentered(178, kLightGrey, "START / O: continuar");
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
        playSfx(won ? audio::Sfx::Victory : audio::Sfx::Defeat);
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
    if (!victory && phase != game::MatchPhase::Finished) {
        printCentered(227, kWhite, "X: lobby   []: otra partida   O: seguir mirando");
    } else {
        printCentered(227, kWhite, "X: volver al lobby     []: otra partida");
    }
}

}  // namespace app
