// Logica de la aplicacion: inicio de sesion -> lobby -> pantalla de carga -> partida
// battle royale con HUD -> victoria / derrota -> lobby.
//
// La implementacion esta repartida en tres ficheros:
//   app.cpp        ciclo de vida, partida (logica, sucesos y sonido) y dibujo 3D
//   app_menus.cpp  inicio de sesion, lobby, controles, carga, pausa y resultados
//   app_hud.cpp    HUD de la partida, minimapa y paginas de depuracion/controles
#pragma once

#include <psptypes.h>

#include "app/achievements.h"
#include "app/build_visuals.h"
#include "app/lobby_scene.h"
#include "app/profile.h"
#include "app/prop_visuals.h"
#include "app/settings.h"
#include "app/ui.h"
#include "audio/mixer.h"
#include "camera/orbit_camera.h"
#include "camera/third_person_camera.h"
#include "core/config.h"
#include "core/timer.h"
#include "game/character_model.h"
#include "game/fixed_timestep.h"
#include "game/match.h"
#include "game/nav_grid.h"
#include "gfx/texture.h"
#include "input/input.h"
#include "world/world.h"

namespace app {

struct FrameInfo {
    const core::FrameTimer& timer;
    const input::Input& input;
    float workMs;  // tiempo de CPU+GPU del frame anterior, sin contar la espera de VSync
};

class App {
public:
    // Carga recursos y construye el escenario (mostrando la pantalla de inicio mientras).
    // Si algo falla se muestra en pantalla (y en el log) en lugar de cerrar la aplicacion.
    // Carpeta donde se guarda el perfil (la del EBOOT). Antes de init().
    void setDataDirectory(const char* dir);
    void init();
    void shutdown();

    // Logica: entrada, pasos fijos de la partida, camaras. No dibuja nada.
    void update(const input::Input& in, float dt);
    // Comandos GU del frame (entre gfx::beginFrame y gfx::endFrame). No toca la partida.
    void render(const FrameInfo& frame);
    // Texto por CPU sobre el frame ya dibujado (entre gfx::endFrame y gfx::present).
    void renderOverlay(const FrameInfo& frame) const;

    u32 clearColor() const;
    bool vsync() const { return m_vsync; }
    bool exitRequested() const { return m_exitRequested; }

#ifdef FORTNITEPSP_HOST_TEST
    // Solo para tools/app_sim (simulacion de host de la aplicacion completa).
    int testScreen() const { return static_cast<int>(m_screen); }
    int testResults() const { return static_cast<int>(m_results); }
    bool testPaused() const { return m_paused; }
    int testBotSetting() const { return m_botCount; }
    game::Match& testMatch() { return m_match; }
    const world::World& testWorld() const { return m_world; }
    int testNavWalkable() const { return m_nav.walkableCount(); }
    float testCameraYaw() const { return m_followCamera.yaw(); }
    bool testScoped() const { return sniperScoped(); }
    math3d::Vec3 testCameraEye() const { return cameraEye(); }
    int testLockTarget() const { return m_lockTarget; }
    bool testFreeCamera() const { return m_freeCameraActive; }
    int testBuildPiece() const { return m_buildPiece; }
    const Profile& testProfile() const { return m_profile; }
    int testLobbyTab() const { return static_cast<int>(m_lobbyTab); }
    bool testSettingsOpen() const { return m_settingsOpen; }
    const Settings& testSettings() const { return m_settings; }
    int testSettingsSection() const { return m_settingsSection; }
    int testToastCount() const { return m_toastCount; }
    int testLockerSlot() const { return m_lockerSlot; }
    bool testLockerChoosing() const { return m_lockerChoosing; }
    bool testMapOpen() const { return m_mapOpen; }
    bool testDancing() const { return m_dancing; }
    bool testSwapOpen() const { return m_swapOpen; }
    int testSwapCursor() const { return m_swapCursor; }
    const camera::ThirdPersonCamera& testFollowCamera() const { return m_followCamera; }
    void testSetCameraAim(float yaw, float look) { m_followCamera.setAim(yaw, look); }
    // La siguiente partida tiene llama seguro (para las capturas).
    void testForceLlama(bool on) { m_testForceLlama = on; }
    const game::TrapPlacement& testTrapPlan() const { return m_trapPlan; }
    math3d::Vec3 testAimPoint() const { return aimPointFrom(cameraEye(), m_followCamera.lookDirection()); }
    u32 testFootsteps() const { return m_playerSteps; }
    float testFogEnd() const { return m_fogEnd; }
#endif

private:
    enum class Screen { Login, Nickname, Lobby, Controls, Loading, Game, Language };
    // Paginas del HUD de la partida (SELECT las recorre).
    enum class Hud { Game, Debug, Controls, Hidden, Count };
    // Pantalla de resultados sobre la partida.
    enum class Results { None, Victory, Defeat };
    enum class LobbyItem { Play, Controls, Exit, Count };
    // Secciones del lobby (L / R): jugar, logros y taquilla.
    enum class LobbyTab { Play, Achievements, Locker, Count };
    static constexpr int kModelCount = 4;  // jugador + 3 aspectos de bot

    // --- app.cpp ----------------------------------------------------------------------
    void presentBootFrame(const char* status);
    void logStage(const char* stage, u64 startMicros) const;
    void buildMinimap();
    void startMatch();
    void goToLobby();
    void startLoading();
    void updateGame(const input::Input& in, float dt);
    void updateGameCameras(const input::Input& in, float dt, bool control, bool aimHeld);
    // Al poner o quitar la mirilla, la camara se orienta para que siga apuntando al mismo
    // punto que la mira (sin recolocarse detras del jugador).
    void updateScopeTransition();
    // Punto al que apunta la mira desde `eye` en `dir` (mundo, paredes y personajes).
    math3d::Vec3 aimPointFrom(const math3d::Vec3& eye, const math3d::Vec3& dir) const;
    void processEvents();
    void playFootsteps();
    void updateDanceSound(float dt);
    void updateNickname(const input::Input& in);
    void renderNickname() const;
    void overlayNickname() const;
    void recordMatch();             // suma la partida al perfil y lo guarda (una vez)
    // Logros: suma a un contador del perfil y desbloquea (con aviso y guardado) lo alcanzado.
    void addStat(Stat stat, int amount);
    void checkAchievements();
    void saveProfile();
    void updateAchievementToasts(float dt);
    // --- app_settings.cpp: menu de ajustes (START en el lobby) --------------------------
    void applySettings();
    void openSettings();
    void closeSettings();
    void updateSettings(const input::Input& in);
    void renderSettings() const;
    void overlaySettings() const;
    void deletePlayerData();
    int settingsItemCount() const;
    void renderAchievementToast() const;
    void overlayAchievementToast() const;
    void renderFullMap() const;
    void updateChestHum(float dt);
    void playSfx(audio::Sfx sfx, float volume = 1.0f, float pan = 0.0f) const;
    // Efecto con volumen por distancia (hasta 60 m) y panoramica respecto a la camara.
    void playSpatial(audio::Sfx sfx, const math3d::Vec3& position, float maxVolume) const;
    void updatePerformance(float dt);
    void updateStormFog();
    void renderGame(const FrameInfo& frame);
    // Devuelve los triangulos de personajes dibujados (para la pagina de depuracion).
    int renderCharacters(const math3d::Frustum& frustum, const math3d::Vec3& eye) const;
    void renderTracers() const;
    void renderExitBar(float x, float y, float w, float h) const;
    math3d::Vec3 cameraEye() const;
    // Mirilla del francotirador (L con el francotirador): vista desde la cabeza con zoom.
    bool sniperScoped() const;
    math3d::Vec3 scopeEye() const;
    float scopeFov() const;  // campo de vision con la mirilla del arma en la mano
    static audio::Sfx shotSound(int weaponType);
    void renderScope() const;
    game::CharacterState renderState(int id) const;
    int spectateTarget() const;
    bool projectToScreen(const math3d::Mat4& viewProjection, const math3d::Vec3& p, float* x,
                         float* y) const;

    // --- app_menus.cpp ----------------------------------------------------------------
    void updateLogin(const input::Input& in, float dt);
    void updateLanguage(const input::Input& in);
    void renderLanguage() const;
    void overlayLanguage() const;
    void updateLobby(const input::Input& in, float dt);
    void updateLobbyPlay(const input::Input& in);
    void updateAchievementsTab(const input::Input& in);
    void updateLockerTab(const input::Input& in);
    void renderLobbyTabs() const;
    void overlayLobbyTabs() const;
    void renderAchievementsTab() const;
    void overlayAchievementsTab() const;
    void renderLockerTab() const;
    void overlayLockerTab() const;
    void updateControlsScreen(const input::Input& in);
    void updateLoading(float dt);
    void updatePause(const input::Input& in);
    void updateResults(const input::Input& in, float dt);
    void renderLogin() const;
    void overlayLogin() const;
    void renderLobby();
    void overlayLobby() const;
    void renderControlsScreen(const FrameInfo& frame) const;
    void overlayControlsScreen(const FrameInfo& frame) const;
    void renderLoading() const;
    void overlayLoading() const;
    void renderPause() const;
    void overlayPause() const;
    void renderResults() const;
    int resultsHintCount() const;
    void overlayResults() const;

    // --- app_hud.cpp ------------------------------------------------------------------
    void renderGameHud() const;
    const ui::Hint* buildHints(int* count) const;
    const ui::Hint* interactionHint(int* count) const;
    void renderAirHud() const;
    void renderDebugPage() const;
    void renderCrosshair() const;
    void renderMinimap() const;
    void renderVitals() const;
    void renderSlots() const;
    void renderSwapChooser() const;
    void renderDamageNumbers() const;
    void overlayGame(const FrameInfo& frame) const;
    void overlayGameHud(const FrameInfo& frame) const;
    void overlayCompass() const;
    void overlayDebug(const FrameInfo& frame) const;
    void renderControlsPage(const FrameInfo& frame) const;
    void overlayControlsPage(const FrameInfo& frame) const;

    // Recursos de presentacion.
    gfx::Texture m_loadscreen;
    gfx::Texture m_login;
    gfx::Texture m_scope;    // Recursos/mirilla.png a 240x136 (se dibuja a 2x)
    bool m_aimHeld = false;  // L mantenido este frame (modo apuntado)
    float m_scopeZoom = 0.0f;  // 0..1: transicion del zoom de la mirilla
    static constexpr float kScopeFovDegrees = 15.0f;
    gfx::Texture m_minimap;  // 128x128 (1.5 m por texel), generado de world::mapColorAt
    ui::Atlas m_atlas;
    LobbyScene m_lobbyScene;
    bool m_lobbyReady = false;
    bool m_audioReady = false;

    world::World m_world;
    bool m_worldReady = false;
    world::RenderStats m_stats;
    int m_characterTriangles = 0;  // del ultimo frame
    camera::Projection m_projection;
    game::NavGrid m_nav;

    // Partida: la simulacion avanza en pasos fijos; lo que se dibuja se interpola.
    game::Match m_match;
    game::MatchConfig m_matchConfig;
    int m_botCount = game::kDefaultBots;
    game::FixedTimestep m_timestep;
    game::CharacterModel m_models[kModelCount];
    bool m_modelsReady = false;
    game::ItemModels m_items;  // armas, consumibles y proyectiles (en la mano y en el suelo)
    game::CharacterAnim m_playerAnim;
    game::CharacterAnim m_botAnims[game::kMaxBots];
    u32 m_playerSteps = 0;  // pisadas ya sonadas (CharacterAnim::footsteps)
    u32 m_botSteps[game::kMaxBots] = {};
    int m_lockTarget = -1;
    int m_matchesPlayed = 0;
    u32 m_eventCursor = 0;   // sucesos de la partida ya procesados (sonido, animaciones)
    int m_playerKiller = -1;  // quien elimino al jugador (-1: nadie / la zona)

    // Construccion: modo activo (CIRCULO), pared que se colocaria y pared apuntada.
    bool m_buildMode = false;
    game::WallPlacement m_wallPlan;
    int m_targetWall = -1;
    // Pared, suelo, rampa y trampa (cruceta en construccion). La trampa es la cuarta.
    static constexpr int kBuildPieces = 4;
    static constexpr int kTrapPiece = 3;
    int m_buildPiece = 0;
    game::TrapPlacement m_trapPlan;  // donde iria la trampa (con la trampa elegida)
    // Ultimo material conseguido con el pico (aviso "+8" junto a la mira).
    int m_harvestResource = 0, m_harvestAmount = 0;
    float m_harvestAge = 99.0f;
    PropVisuals m_props;  // botin, autobus, ala delta
    bool m_propsReady = false;
    float m_chestHumTimer = 0.0f;
    bool m_chestHumOn = false;
    bool m_mapOpen = false;          // SELECT: mapa a pantalla completa
    // Selector de que soltar (inventario lleno al recoger): la cruceta elige la ranura,
    // CUADRADO cambia y CIRCULO cancela.
    bool m_swapOpen = false;
    int m_swapPickup = -1;
    int m_swapCursor = 0;
    bool m_wasScoped = false;        // la mirilla estaba puesta el frame anterior
    bool m_testForceLlama = false;   // solo pruebas de host (testForceLlama)
    bool m_dancing = false;          // cruceta arriba: baile (hasta moverse o actuar)
    bool m_danceSoundOn = false;
    float m_danceSoundTimer = 0.0f;
    float m_airdropMessage = 0.0f;  // s que queda el aviso "airdrop en camino"
    float m_stormMessage = 0.0f;  // s que queda el aviso "la tormenta se cierra"
    BuildVisuals m_buildVisuals;
    bool m_buildVisualsReady = false;
    math3d::Mat4 m_viewProjection = math3d::Mat4::identity();  // para proyectar numeros de dano

    camera::ThirdPersonCamera m_followCamera;
    camera::OrbitCamera m_freeCamera;
    bool m_freeCameraActive = false;

    // Pausa (START) y resultados.
    bool m_paused = false;
    int m_pauseItem = 0;
    Results m_results = Results::None;
    float m_resultsTime = 0.0f;     // segundos desde que se muestran los resultados
    float m_resultsDelay = 0.0f;    // espera tras ganar o caer antes de mostrarlos
    bool m_spectating = false;       // el jugador eligio seguir mirando tras caer
    bool m_resultsDismissed = false;
    bool m_resultSoundPlayed = false;  // victoria/derrota ya sono en esta partida

    // Menus.
    int m_langChoice = 0;          // pantalla de idioma (primer arranque): 0 espanol, 1 ingles
    int m_loginStage = 0;          // 0: formulario, 1: conectando
    // Perfil (perfil.txt) y teclado para el nombre la primera vez.
    Profile m_profile;
    char m_profilePath[256] = "perfil.txt";
    char m_nickBuffer[Profile::kMaxName + 1] = "";
    int m_nickRow = 0, m_nickCol = 0;
    bool m_nickLower = false;
    bool m_matchRecorded = false;  // las estadisticas de esta partida ya se han sumado
    float m_loginTimer = 0.0f;
    LobbyItem m_lobbyItem = LobbyItem::Play;
    LobbyTab m_lobbyTab = LobbyTab::Play;
    int m_achievementCursor = 0;   // logro elegido en la lista
    int m_lockerSlot = 0;          // categoria elegida en la taquilla
    bool m_lockerChoosing = false; // dentro de una categoria, eligiendo objeto
    int m_lockerItem = 0;
    // Logros desbloqueados pendientes de anunciar (de uno en uno, 3 s cada uno).
    static constexpr int kMaxToasts = 4;
    AchievementId m_toasts[kMaxToasts] = {};
    int m_toastCount = 0;
    float m_toastTime = 0.0f;
    bool m_profileDirty = false;   // hay contadores sin guardar
    // Ajustes (ajustes.txt) y su menu: seccion, opcion elegida, accion que se esta
    // reasignando (-1: ninguna), confirmacion de borrar datos y un aviso temporal.
    Settings m_settings;
    char m_settingsPath[256] = "ajustes.txt";
    bool m_settingsOpen = false;
    int m_settingsSection = 0;
    int m_settingsItem = 0;
    int m_remapAction = -1;
    bool m_confirmDelete = false;
    i18n::T m_settingsMessage = i18n::T::None;
    float m_settingsMessageTime = 0.0f;
    bool m_settingsMessageError = false;
    bool m_confirmExit = false;
    float m_lobbyYaw = 0.0f;       // giro del personaje del lobby (stick)
    float m_lobbyTime = 0.0f;
    game::CharacterAnim m_lobbyAnim;
    math3d::Mat4 m_lobbyViewProjection = math3d::Mat4::identity();
    float m_loadingTimer = 0.0f;
    int m_loadingFrames = 0;
    bool m_loadingStarted = false;
    int m_tip = 0;

    // Medidas de rendimiento (ventana de 1 s).
    float m_perfWindow = 0.0f;
    u64 m_perfAiMicros = 0, m_perfPhysicsMicros = 0;
    int m_perfDecisions = 0, m_perfSteps = 0, m_perfFrames = 0;
    float m_aiMsPerFrame = 0.0f, m_physicsMsPerFrame = 0.0f;
    int m_decisionsPerSecond = 0;
    float m_matchFrameMsSum = 0.0f, m_matchFrameMsMax = 0.0f;
    int m_matchFrames = 0;
    bool m_matchLogged = false;
    unsigned m_startHeapBytes = 0;  // heap en uso al llegar al lobby por primera vez

    Screen m_screen = Screen::Login;
    Hud m_hud = Hud::Game;
    bool m_fog = true;
    // Niebla del frame (la normal, o densa y morada cerca de la tormenta del borde).
    u32 m_fogColor = config::kColorSky;
    float m_fogStart = config::kFogStart, m_fogEnd = config::kFogEnd;
    bool m_vsync = true;
    bool m_exitRequested = false;
    float m_exitProgress = 0.0f;  // 0..1 mientras se mantiene la combinacion de salida
    float m_blinkTime = 0.0f;
    float m_lastFrameMs = 0.0f;

    const char* m_lastButton = "-";
    u32 m_pressCount = 0;
    int m_cpuMhz = 0;
    int m_busMhz = 0;
};

}  // namespace app
