// Logica de la aplicacion: portada + partida battle royale con HUD.
#pragma once

#include <psptypes.h>

#include "app/build_visuals.h"
#include "camera/orbit_camera.h"
#include "camera/third_person_camera.h"
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
    // Carga recursos y construye el escenario. Si algo falla se muestra en pantalla
    // (y en el log) en lugar de cerrar la aplicacion.
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

private:
    enum class Screen { Title, Game };
    // Paginas del HUD de la partida (SELECT las recorre).
    enum class Hud { Game, Debug, Controls, Hidden, Count };
    static constexpr int kModelCount = 4;  // jugador + 3 aspectos de bot

    void startMatch();
    void updateGame(const input::Input& in, float dt);
    void updatePerformance(float dt);

    void renderTitle() const;
    void overlayTitle(const FrameInfo& frame) const;
    void renderGame(const FrameInfo& frame);
    void renderCharacters(const math3d::Frustum& frustum, const math3d::Vec3& eye) const;
    void renderTracers() const;
    void renderGameHud() const;
    void renderDamageNumbers() const;
    void renderAmmo() const;
    void overlayGame(const FrameInfo& frame) const;
    void overlayGameHud(const FrameInfo& frame) const;
    void overlayMatchEnd() const;
    void overlayDebug(const FrameInfo& frame) const;
    void renderControlsPage(const FrameInfo& frame) const;
    void overlayControlsPage(const FrameInfo& frame) const;
    void renderExitBar(float x, float y, float w, float h) const;

    math3d::Vec3 cameraEye() const;
    game::CharacterState renderState(int id) const;

    gfx::Texture m_loadscreen;
    bool m_hasLoadscreen = false;
    gfx::Texture m_logo;
    bool m_hasLogo = false;

    world::World m_world;
    bool m_worldReady = false;
    world::RenderStats m_stats;
    camera::Projection m_projection;
    game::NavGrid m_nav;

    // Partida: la simulacion avanza en pasos fijos; lo que se dibuja se interpola.
    game::Match m_match;
    game::MatchConfig m_matchConfig;
    int m_botCount = game::kDefaultBots;
    game::FixedTimestep m_timestep;
    game::CharacterModel m_models[kModelCount];
    bool m_modelsReady = false;
    game::CharacterAnim m_playerAnim;
    game::CharacterAnim m_botAnims[game::kMaxBots];
    int m_lockTarget = -1;
    int m_matchesPlayed = 0;

    // Construccion: modo activo (CIRCULO), pared que se colocaria y pared apuntada.
    bool m_buildMode = false;
    game::WallPlacement m_wallPlan;
    int m_targetWall = -1;
    BuildVisuals m_buildVisuals;
    bool m_buildVisualsReady = false;
    math3d::Mat4 m_viewProjection = math3d::Mat4::identity();  // para proyectar numeros de dano

    camera::ThirdPersonCamera m_followCamera;
    camera::OrbitCamera m_freeCamera;
    bool m_freeCameraActive = false;

    // Medidas de rendimiento (ventana de 1 s).
    float m_perfWindow = 0.0f;
    u64 m_perfAiMicros = 0, m_perfPhysicsMicros = 0;
    int m_perfDecisions = 0, m_perfSteps = 0, m_perfFrames = 0;
    float m_aiMsPerFrame = 0.0f, m_physicsMsPerFrame = 0.0f;
    int m_decisionsPerSecond = 0;
    float m_matchFrameMsSum = 0.0f, m_matchFrameMsMax = 0.0f;
    int m_matchFrames = 0;
    bool m_matchLogged = false;

    Screen m_screen = Screen::Title;
    Hud m_hud = Hud::Game;
    bool m_fog = true;
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
