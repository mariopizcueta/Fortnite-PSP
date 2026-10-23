#include "app/app.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include <initializer_list>

#include <psppower.h>

#include "assets/embedded_assets.h"
#include "core/config.h"
#include "gfx/debug_text.h"
#include "gfx/graphics.h"
#include "platform/log.h"
#include "world/world_units.h"

namespace app {
namespace {

using gfx::rgba;
using math3d::Vec3;

constexpr u32 kWhite = rgba(255, 255, 255);
constexpr u32 kLightGrey = rgba(200, 200, 200);
constexpr u32 kErrorRed = rgba(255, 90, 80);
constexpr u32 kPanel = rgba(0, 0, 0, 140);
constexpr u32 kBar = rgba(0, 0, 0, 170);
constexpr u32 kButtonOff = rgba(255, 255, 255, 60);
constexpr u32 kButtonOn = config::kColorGold;
constexpr u32 kCrosshair = rgba(255, 255, 255, 220);
constexpr u32 kCrosshairLocked = rgba(255, 70, 60, 240);
constexpr u32 kHealthGreen = rgba(84, 200, 72);
constexpr u32 kBuildBlue = rgba(30, 90, 190, 170);
constexpr u32 kBuildReticle = rgba(120, 200, 255);
constexpr float kDamageNumberSeconds = 0.9f;

constexpr int kLineHeight = 11;
constexpr int kMinBots = 5;
constexpr float kCorpseSeconds = 8.0f;     // los eliminados se ven tumbados este tiempo
constexpr float kKillFeedSeconds = 6.0f;
constexpr float kLockAngleDegrees = 35.0f;  // L: objetivo dentro de este cono
constexpr float kLockDistance = 45.0f;

// Aparicion del jugador: al sur del muro, mirando al deposito.
constexpr Vec3 kPlayerSpawn{0.0f, 0.0f, 24.0f};
constexpr float kPlayerSpawnYaw = 0.0f;

// Botones que se muestran en la lista de "Pulsado", en orden de lectura.
constexpr u32 kListedButtons[] = {
    PSP_CTRL_UP,       PSP_CTRL_DOWN,     PSP_CTRL_LEFT,     PSP_CTRL_RIGHT,
    PSP_CTRL_TRIANGLE, PSP_CTRL_CIRCLE,   PSP_CTRL_CROSS,    PSP_CTRL_SQUARE,
    PSP_CTRL_LTRIGGER, PSP_CTRL_RTRIGGER, PSP_CTRL_SELECT,   PSP_CTRL_START,
    PSP_CTRL_NOTE,     PSP_CTRL_SCREEN,   PSP_CTRL_VOLUP,    PSP_CTRL_VOLDOWN,
};

// Disposicion del mando dibujado en la pagina de controles.
struct ButtonBox {
    u32 mask;
    float x, y, w, h;
};
constexpr ButtonBox kButtonBoxes[] = {
    {PSP_CTRL_LTRIGGER, 320, 34, 44, 10}, {PSP_CTRL_RTRIGGER, 422, 34, 44, 10},
    {PSP_CTRL_UP, 336, 54, 12, 12},       {PSP_CTRL_DOWN, 336, 82, 12, 12},
    {PSP_CTRL_LEFT, 322, 68, 12, 12},     {PSP_CTRL_RIGHT, 350, 68, 12, 12},
    {PSP_CTRL_TRIANGLE, 438, 54, 12, 12}, {PSP_CTRL_CROSS, 438, 82, 12, 12},
    {PSP_CTRL_SQUARE, 424, 68, 12, 12},   {PSP_CTRL_CIRCLE, 452, 68, 12, 12},
    {PSP_CTRL_SELECT, 366, 100, 24, 8},   {PSP_CTRL_START, 396, 100, 24, 8},
};

constexpr float kStickBoxX = 361.0f;
constexpr float kStickBoxY = 124.0f;
constexpr float kStickBoxSize = 64.0f;

void printCentered(int y, u32 color, const char* str) {
    gfx::text::print((gfx::kScreenWidth - gfx::text::width(str)) / 2, y, color, "%s", str);
}

void printCenteredShadow(int y, u32 color, const char* str) {
    gfx::text::printShadow((gfx::kScreenWidth - gfx::text::width(str)) / 2, y, color, "%s", str);
}

// Rumbo de 0 a 360 grados (0 = norte, crece hacia el oeste como el yaw).
float headingDegrees(float yaw) {
    float d = yaw * (180.0f / math3d::kPi);
    if (d < 0.0f) d += 360.0f;
    return d;
}

void formatTime(float seconds, char* out, size_t size) {
    const int s = static_cast<int>(seconds);
    snprintf(out, size, "%02d:%02d", s / 60, s % 60);
}

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

}  // namespace

// --- Ciclo de vida ----------------------------------------------------------------------

void App::init() {
    const assets::Blob png = assets::loadscreenPng();
    m_hasLoadscreen = gfx::loadPngFromMemory(m_loadscreen, png.data, png.size);
    const assets::Blob logo = assets::logoPng();
    m_hasLogo = gfx::loadPngFromMemory(m_logo, logo.data, logo.size);
    platform::log::write("portada: %s, logotipo: %s", m_hasLoadscreen ? "ok" : "ERROR",
                         m_hasLogo ? "ok" : "ERROR");

    u64 t0 = core::nowMicros();
    m_worldReady = m_world.build();
    platform::log::write("escenario: %s en %u ms, %d lotes, %d triangulos, geometria %u KB, "
                         "texturas %u KB, %d cajas de colision",
                         m_worldReady ? "ok" : "ERROR (memoria)",
                         static_cast<unsigned>((core::nowMicros() - t0) / 1000), m_world.batchCount(),
                         m_world.totalTriangles(), static_cast<unsigned>(m_world.geometryBytes() / 1024),
                         static_cast<unsigned>(m_world.textureBytes() / 1024), m_world.collision().count());

    t0 = core::nowMicros();
    m_nav.build(m_world.collision(), world::kMapHalfSize);
    platform::log::write("navegacion: %d de %d nodos transitables en %u ms", m_nav.walkableCount(),
                         m_nav.size() * m_nav.size(), static_cast<unsigned>((core::nowMicros() - t0) / 1000));

    const game::CharacterPalette* palettes[kModelCount] = {
        &game::palettes::kPlayer, &game::palettes::kCommando, &game::palettes::kOrange,
        &game::palettes::kRanger};
    m_modelsReady = true;
    for (int i = 0; i < kModelCount; ++i) m_modelsReady &= m_models[i].build(*palettes[i]);
    platform::log::write("modelos de personaje: %s, %d triangulos cada uno",
                         m_modelsReady ? "ok" : "ERROR", m_models[0].triangles());
    m_buildVisualsReady = m_buildVisuals.build(m_matchConfig.build);

    m_projection.fovYDegrees = config::kFovYDegrees;
    m_projection.aspect = static_cast<float>(gfx::kScreenWidth) / gfx::kScreenHeight;
    m_projection.zNear = config::kNearPlane;
    m_projection.zFar = config::kFarPlane;

    m_cpuMhz = scePowerGetCpuClockFrequencyInt();
    m_busMhz = scePowerGetBusClockFrequencyInt();
}

void App::startMatch() {
    m_matchConfig = game::MatchConfig{};
    m_matchConfig.botCount = m_botCount;
    // Semilla distinta en cada partida (el reloj del sistema).
    m_matchConfig.seed = static_cast<u32>(core::nowMicros()) ^ (0x9E3779B9u * ++m_matchesPlayed);
    m_match.start(m_matchConfig, m_world.mutableCollision(), m_nav, m_world.pointsOfInterest(),
                  kPlayerSpawn, kPlayerSpawnYaw);
    m_buildMode = false;
    m_targetWall = -1;
    m_timestep.reset();
    m_playerAnim = game::CharacterAnim{};
    for (game::CharacterAnim& a : m_botAnims) a = game::CharacterAnim{};
    m_followCamera.reset(m_match.body(0).state().position, kPlayerSpawnYaw);
    m_freeCameraActive = false;
    m_lockTarget = -1;
    m_matchFrameMsSum = m_matchFrameMsMax = 0.0f;
    m_matchFrames = 0;
    m_matchLogged = false;
    platform::log::write("partida %d: %d bots, semilla %u", m_matchesPlayed, m_botCount,
                         static_cast<unsigned>(m_matchConfig.seed));
}

void App::shutdown() {
    m_buildVisuals.destroy();
    m_buildVisualsReady = false;
    for (game::CharacterModel& m : m_models) m.destroy();
    m_modelsReady = false;
    m_world.destroy();
    m_worldReady = false;
    gfx::destroyTexture(m_logo);
    m_hasLogo = false;
    gfx::destroyTexture(m_loadscreen);
    m_hasLoadscreen = false;
}

// --- Logica -----------------------------------------------------------------------------

void App::update(const input::Input& in, float dt) {
    m_blinkTime += dt;
    m_lastFrameMs = dt * 1000.0f;

    m_exitProgress = in.comboHeldSeconds() / config::kExitHoldSeconds;
    if (m_exitProgress >= 1.0f) {
        m_exitProgress = 1.0f;
        m_exitRequested = true;
        return;
    }

    const u32 pressed = in.pressedMask();
    for (u32 bit = 1; bit != 0; bit <<= 1) {
        if (!(pressed & bit)) continue;
        if (const char* name = input::buttonName(bit)) m_lastButton = name;
        ++m_pressCount;
    }

    switch (m_screen) {
        case Screen::Title:
            if (in.pressed(PSP_CTRL_LEFT) && m_botCount > kMinBots) m_botCount -= 5;
            if (in.pressed(PSP_CTRL_RIGHT) && m_botCount < game::kMaxBots) m_botCount += 5;
            if (in.pressed(PSP_CTRL_CROSS) && m_worldReady) {
                startMatch();
                m_screen = Screen::Game;
            }
            break;
        case Screen::Game:
            updateGame(in, dt);
            break;
    }
}

game::CharacterState App::renderState(int id) const {
    return m_match.body(id).interpolated(m_timestep.alpha());
}

void App::updateGame(const input::Input& in, float dt) {
    if (in.pressed(PSP_CTRL_SELECT)) {
        m_hud = static_cast<Hud>((static_cast<int>(m_hud) + 1) % static_cast<int>(Hud::Count));
    }
    // TRIANGULO: camara libre de depuracion. Dentro de ella, CUADRADO = niebla y
    // START = VSync (el jugador no actua, asi que esos botones estan libres).
    if (in.pressed(PSP_CTRL_TRIANGLE)) {
        m_freeCameraActive = !m_freeCameraActive;
        if (m_freeCameraActive) {
            m_freeCamera.set(renderState(0).position + Vec3{0.0f, 1.0f, 0.0f}, m_followCamera.yaw(),
                             0.35f, 10.0f);
        }
    }
    if (m_freeCameraActive) {
        if (in.pressed(PSP_CTRL_SQUARE)) m_fog = !m_fog;
        if (in.pressed(PSP_CTRL_START)) m_vsync = !m_vsync;
    }
    if (m_match.phase() == game::MatchPhase::Finished && in.pressed(PSP_CTRL_CROSS)) {
        startMatch();
        return;
    }

    const bool control = !m_freeCameraActive && m_match.phase() != game::MatchPhase::Spectating;
    // CIRCULO: modo construccion. Se desactiva solo si el jugador cae.
    if (control && in.pressed(PSP_CTRL_CIRCLE)) m_buildMode = !m_buildMode;
    if (!m_match.alive(0)) m_buildMode = false;
    const bool lockHeld = control && !m_buildMode && in.held(PSP_CTRL_LTRIGGER);

    // 1) Entrada del jugador.
    game::PlayerInput playerInput;
    game::PlayerAim aim;
    game::PlayerCommands commands;
    aim.eye = m_followCamera.eye();
    aim.direction = m_followCamera.lookDirection();
    commands.buildMode = m_buildMode;
    if (control) {
        playerInput.moveX = in.analogX();
        playerInput.moveY = in.analogY();
        playerInput.jumpPressed = in.pressed(PSP_CTRL_CROSS);
        playerInput.jumpHeld = in.held(PSP_CTRL_CROSS);
        playerInput.aiming = lockHeld;
        if (m_buildMode) {
            commands.placeWall = in.pressed(PSP_CTRL_RTRIGGER);  // una pared por pulsacion
            commands.removeWall = in.pressed(PSP_CTRL_SQUARE);
        } else {
            aim.fire = in.held(PSP_CTRL_RTRIGGER);
            commands.reload = in.pressed(PSP_CTRL_SQUARE);
        }
    }
    playerInput.cameraYaw = m_followCamera.yaw();
    m_match.setPlayerInput(playerInput, aim, commands);

    // 2) Simulacion en pasos fijos de 1/60 s, independiente de los FPS.
    const int steps = m_timestep.advance(dt);
    for (int i = 0; i < steps; ++i) m_match.step(game::FixedTimestep::kStep);

    // 3) Animaciones (visuales, con el tiempo de frame).
    const game::CharacterState player = renderState(0);
    m_playerAnim.update(player, m_match.playerAiming(), dt);
    for (int i = 0; i < m_match.botCount(); ++i) {
        const game::Bot& b = m_match.bot(i);
        if (b.state != game::BotState::Eliminated) m_botAnims[i].update(renderState(i + 1), b.aiming, dt);
    }

    // 4) Camaras. L: fijar el bot visible mas centrado (o recolocar si no hay ninguno).
    Vec3 lockPoint;
    const Vec3* lock = nullptr;
    if (lockHeld) {
        const bool keep = m_lockTarget > 0 && m_match.alive(m_lockTarget) &&
                          m_match.lineOfSight(m_followCamera.eye(), m_match.chestOf(m_lockTarget));
        if (!keep) {
            m_lockTarget = m_match.findLockTarget(m_followCamera.eye(), m_followCamera.lookDirection(),
                                                  kLockAngleDegrees, kLockDistance);
        }
        if (m_lockTarget > 0) {
            lockPoint = renderState(m_lockTarget).position + Vec3{0.0f, game::tuning::kChestHeight, 0.0f};
            lock = &lockPoint;
        }
    } else {
        m_lockTarget = -1;
    }
    const bool recenter = control && in.pressed(PSP_CTRL_LTRIGGER) && !lock;
    if (m_freeCameraActive) {
        m_freeCamera.update(in, dt);
    } else {
        m_followCamera.update(in, player.position, player.yaw, dt, recenter, lock,
                              &m_world.collision());
    }

    // 5) Construccion: donde iria la pared y que pared se quitaria (para dibujarlas).
    if (m_buildMode) {
        game::PlayerAim view;
        view.eye = m_followCamera.eye();
        view.direction = m_followCamera.lookDirection();
        m_wallPlan = m_match.planWall(m_followCamera.yaw());
        m_targetWall = m_match.targetedWall(view);
    } else {
        m_targetWall = -1;
    }

    updatePerformance(dt);
}

void App::updatePerformance(float dt) {
    // Coste medio por frame de IA y fisica, en ventanas de 1 s.
    const game::MatchStats& s = m_match.stats();
    ++m_perfFrames;
    m_perfWindow += dt;
    if (m_perfWindow >= 1.0f) {
        m_aiMsPerFrame = static_cast<float>(s.aiMicros - m_perfAiMicros) / 1000.0f / m_perfFrames;
        m_physicsMsPerFrame = static_cast<float>(s.physicsMicros - m_perfPhysicsMicros) / 1000.0f / m_perfFrames;
        m_decisionsPerSecond = static_cast<int>((s.decisions - m_perfDecisions) / m_perfWindow);
        m_perfAiMicros = s.aiMicros;
        m_perfPhysicsMicros = s.physicsMicros;
        m_perfDecisions = s.decisions;
        m_perfSteps = s.steps;
        m_perfFrames = 0;
        m_perfWindow = 0.0f;
    }

    if (m_match.phase() != game::MatchPhase::Finished) {
        m_matchFrameMsSum += m_lastFrameMs;
        if (m_lastFrameMs > m_matchFrameMsMax) m_matchFrameMsMax = m_lastFrameMs;
        ++m_matchFrames;
    } else if (!m_matchLogged) {
        // Resumen de rendimiento en el log: sirve para decidir si subir a 30 bots.
        m_matchLogged = true;
        platform::log::write(
            "fin de partida: ganador %s, puesto del jugador %d, bajas %d, %.0f s | %d bots: "
            "frame medio %.2f ms (max %.1f), IA %.3f ms/s de juego, %d decisiones, %d caminos "
            "(max %d nodos), %d atascos, %d disparos (%d impactos, %d contra paredes, %d contra "
            "paredes construidas) | paredes: %d construidas, %d quitadas, %d rotas",
            m_match.name(m_match.winner()), m_match.playerPlace(), m_match.playerKills(),
            static_cast<double>(m_match.elapsed()), m_botCount,
            static_cast<double>(m_matchFrames ? m_matchFrameMsSum / m_matchFrames : 0.0f),
            static_cast<double>(m_matchFrameMsMax),
            static_cast<double>(s.aiMicros / 1000.0 / (m_match.elapsed() > 0.0f ? m_match.elapsed() : 1.0f)),
            s.decisions, s.pathRequests, s.maxPathExpansions, s.stuckEvents, s.shotsFired,
            s.shotsHitEntity, s.shotsBlockedByWorld, s.shotsHitStructure, s.wallsBuilt, s.wallsRemoved,
            s.wallsDestroyed);
    }
}

u32 App::clearColor() const {
    // Mismo color que la niebla: los objetos lejanos se funden con el fondo.
    return config::kColorSky;
}

math3d::Vec3 App::cameraEye() const {
    return m_freeCameraActive ? m_freeCamera.eye() : m_followCamera.eye();
}

void App::render(const FrameInfo& frame) {
    if (m_screen == Screen::Title) {
        renderTitle();
    } else {
        renderGame(frame);
    }
}

void App::renderOverlay(const FrameInfo& frame) const {
    if (m_screen == Screen::Title) {
        overlayTitle(frame);
    } else {
        overlayGame(frame);
    }
}

void App::renderExitBar(float x, float y, float w, float h) const {
    gfx::fillRect(x, y, w, h, kButtonOff);
    if (m_exitProgress > 0.0f) gfx::fillRect(x, y, w * m_exitProgress, h, rgba(220, 60, 50));
}

// --- Portada --------------------------------------------------------------------------

void App::renderTitle() const {
    if (m_hasLoadscreen) {
        gfx::drawTexture(m_loadscreen, 0.0f, 0.0f);
    } else {
        gfx::fillRect(0, 180, gfx::kScreenWidth, gfx::kScreenHeight - 180, config::kColorGrass);
    }
    gfx::fillRect(0, 0, gfx::kScreenWidth, 20, kBar);
    if (m_hasLogo) {
        gfx::drawTexture(m_logo, static_cast<float>((gfx::kScreenWidth - m_logo.width) / 2), 26.0f);
    }
    gfx::fillRect(0, 214, gfx::kScreenWidth, 58, kBar);
    if (m_exitProgress > 0.0f) renderExitBar(140, 210, 200, 3);
}

void App::overlayTitle(const FrameInfo& frame) const {
    gfx::text::printShadow(8, 6, config::kColorGold, "%s v%s", config::kGameTitle, config::kVersion);
    gfx::text::printShadow(400, 6, kWhite, "%5.1f FPS", frame.timer.fps());

    char bots[48];
    snprintf(bots, sizeof(bots), "Bots: < %d >   (izq./dcha.)", m_botCount);
    printCentered(220, m_botCount > game::kDefaultBots ? kErrorRed : kWhite, bots);
    if (m_botCount > game::kDefaultBots) printCentered(230, kLightGrey, "(mas de 10: mide el rendimiento)");
    if (!m_worldReady) {
        printCentered(242, kErrorRed, "ERROR: sin memoria para el escenario");
    } else if (fmodf(m_blinkTime, 1.0f) < 0.65f) {
        printCentered(242, config::kColorGold, "Pulsa X para jugar");
    }
    printCentered(256, kLightGrey, "Salir: mantener L + R + START (1 s)");
}

// --- Partida: dibujo ------------------------------------------------------------------------

void App::renderGame(const FrameInfo& frame) {
    if (m_worldReady) {
        const math3d::Mat4 projection = m_projection.matrix();
        const math3d::Mat4 view =
            m_freeCameraActive ? m_freeCamera.viewMatrix() : m_followCamera.viewMatrix();
        gfx::FogSettings fog;
        fog.enabled = m_fog;
        fog.start = config::kFogStart;
        fog.end = config::kFogEnd;
        fog.color = config::kColorSky;  // == clearColor()

        gfx::begin3D(projection, view, fog);
        const math3d::Frustum frustum = math3d::Frustum::fromMatrix(projection * view);
        const float drawDistance = m_fog ? config::kFogEnd : config::kFarPlane;
        m_stats = m_world.draw(frustum, cameraEye(), drawDistance);
        if (m_buildVisualsReady) m_buildVisuals.drawWalls(m_match.structures(), m_world.textures());
        if (m_modelsReady) renderCharacters(frustum, cameraEye());
        renderTracers();
        // Lo semitransparente al final, para no tapar lo que hay detras.
        if (m_buildVisualsReady && m_buildMode && !m_freeCameraActive) {
            if (m_targetWall >= 0) m_buildVisuals.drawHighlight(m_match.structures().wall(m_targetWall));
            m_buildVisuals.drawGhost(m_wallPlan);
        }
        m_viewProjection = projection * view;
        gfx::begin2D();
    }

    switch (m_hud) {
        case Hud::Game:
            renderGameHud();
            break;
        case Hud::Debug:
            gfx::fillRect(0, 0, gfx::kScreenWidth, 16, kBar);
            gfx::fillRect(4, 20, 232, 248, kPanel);
            gfx::fillRect(240, 20, 236, 248, kPanel);
            break;
        case Hud::Controls:
            renderControlsPage(frame);
            break;
        case Hud::Hidden:
        case Hud::Count:
            break;
    }
    if (m_exitProgress > 0.0f) renderExitBar(140, 18, 200, 4);
}

void App::renderCharacters(const math3d::Frustum& frustum, const Vec3& eye) const {
    const float maxDistance = m_fog ? config::kFogEnd : config::kFarPlane;
    auto visible = [&](const Vec3& p) {
        return math3d::length(p - eye) < maxDistance + 2.0f &&
               frustum.sphereVisible(p + Vec3{0.0f, 0.9f, 0.0f}, 1.3f);
    };
    const game::CharacterState player = renderState(0);
    if (visible(player.position)) {
        m_models[0].draw(player, m_playerAnim, m_world.textures(), !m_match.alive(0));
    }
    for (int i = 0; i < m_match.botCount(); ++i) {
        const game::Bot& b = m_match.bot(i);
        const bool eliminated = b.state == game::BotState::Eliminated;
        if (eliminated && m_match.elapsed() - b.eliminatedAt > kCorpseSeconds) continue;
        const game::CharacterState s = renderState(i + 1);
        if (!visible(s.position)) continue;
        m_models[1 + b.palette % 3].draw(s, m_botAnims[i], m_world.textures(), eliminated);
    }
}

void App::renderTracers() const {
    gfx::LineVertex lines[game::Match::kMaxTracers * 2];
    int count = 0;
    const game::Tracer* tracers = m_match.tracers();
    for (int i = 0; i < game::Match::kMaxTracers; ++i) {
        const game::Tracer& t = tracers[i];
        if (t.age >= 0.12f) continue;
        const u32 color = t.byPlayer ? rgba(255, 236, 140) : rgba(255, 150, 90);
        lines[count++] = {color, t.from.x, t.from.y, t.from.z};
        lines[count++] = {color, t.to.x, t.to.y, t.to.z};
    }
    gfx::setModelMatrix(math3d::Mat4::identity());
    gfx::drawLines(lines, count);
}

void App::renderGameHud() const {
    // Destello rojo al recibir un impacto.
    const float hurt = m_match.sincePlayerDamaged();
    if (hurt < 0.35f) {
        gfx::fillRect(0, 0, gfx::kScreenWidth, gfx::kScreenHeight,
                      rgba(200, 20, 10, static_cast<u32>(110.0f * (1.0f - hurt / 0.35f))));
    }
    gfx::fillRect(0, 0, gfx::kScreenWidth, 16, kBar);

    const game::CharacterState& p = m_match.body(0).state();
    if (m_buildMode) {
        // Franja azul: el modo construccion se ve siempre.
        gfx::fillRect(0, 17, gfx::kScreenWidth, 26, kBuildBlue);
    }
    if (m_match.alive(0) && !m_freeCameraActive && m_buildMode) {
        // Reticula de construccion: cuatro esquinas.
        const u32 c = m_wallPlan.valid ? kBuildReticle : kErrorRed;
        for (int sx : {-1, 1}) {
            for (int sy : {-1, 1}) {
                const float cx = 240.0f + sx * 10.0f, cy = 136.0f + sy * 10.0f;
                gfx::fillRect(sx < 0 ? cx : cx - 5.0f, cy, 6, 2, c);
                gfx::fillRect(cx, sy < 0 ? cy : cy - 5.0f, 2, 6, c);
            }
        }
    } else if (m_match.alive(0) && !m_freeCameraActive) {
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

    // Vida (como en Referencias/Referencia gameplay.png).
    const float ratio = p.maxHealth > 0.0f ? p.health / p.maxHealth : 0.0f;
    gfx::fillRect(20, 255, 152, 11, rgba(0, 0, 0, 170));
    gfx::fillRect(21, 256, 150 * ratio, 9, ratio > 0.3f ? kHealthGreen : kErrorRed);
    renderAmmo();
    renderDamageNumbers();

    if (m_match.phase() != game::MatchPhase::Playing) {
        gfx::fillRect(0, 96, gfx::kScreenWidth, 62, rgba(0, 0, 0, 170));
    }
}

void App::renderAmmo() const {
    gfx::fillRect(352, 222, 124, 46, kPanel);
    if (m_buildMode) {
        // Paredes construidas en grande (como la municion en modo combate).
        const game::StructureSystem& s = m_match.structures();
        const bool full = s.count() >= s.config().maxWalls;
        drawBigNumber(358, 238, s.count(), 4.0f, full ? kErrorRed : kBuildReticle);
        return;
    }
    const game::Weapon& w = m_match.playerWeapon();
    const u32 color = w.ammo() == 0 ? kErrorRed : (w.reloading() ? kLightGrey : kWhite);
    drawBigNumber(358, 238, w.ammo(), 4.0f, color);
    if (w.reloading()) {
        gfx::fillRect(356, 263, 116, 3, rgba(0, 0, 0, 200));
        gfx::fillRect(356, 263, 116 * w.reloadProgress(), 3, config::kColorGold);
    }
}

void App::renderDamageNumbers() const {
    // Numeros de dano sobre lo que ha alcanzado el jugador: suben y se desvanecen.
    const game::DamageNumber* numbers = m_match.damageNumbers();
    for (int i = 0; i < game::Match::kMaxDamageNumbers; ++i) {
        const game::DamageNumber& d = numbers[i];
        if (d.age >= kDamageNumberSeconds) continue;
        const Vec3 p = d.position + Vec3{0.0f, d.age * 0.9f, 0.0f};
        const float* m = m_viewProjection.m;
        const float w = m[3] * p.x + m[7] * p.y + m[11] * p.z + m[15];
        if (w < 0.2f) continue;  // detras de la camara
        const float sx = (m[0] * p.x + m[4] * p.y + m[8] * p.z + m[12]) / w;
        const float sy = (m[1] * p.x + m[5] * p.y + m[9] * p.z + m[13]) / w;
        const float x = (sx * 0.5f + 0.5f) * gfx::kScreenWidth;
        const float y = (0.5f - sy * 0.5f) * gfx::kScreenHeight;
        const float fade = d.age < 0.55f ? 1.0f : 1.0f - (d.age - 0.55f) / (kDamageNumberSeconds - 0.55f);
        const u32 alpha = static_cast<u32>(255.0f * fade);
        const float scale = d.structure ? 2.0f : 3.0f;
        const u32 color = d.structure ? rgba(200, 220, 255, alpha) : rgba(255, 255, 255, alpha);
        const int value = static_cast<int>(d.amount + 0.5f);
        drawBigNumber(x - bigNumberWidth(value, scale) * 0.5f, y - 2.5f * scale, value, scale, color);
    }
}

// --- Partida: texto -------------------------------------------------------------------------

void App::overlayGame(const FrameInfo& frame) const {
    if (m_hud == Hud::Hidden) {
        gfx::text::printShadow(400, 4, kWhite, "%5.1f FPS", frame.timer.fps());
        return;
    }
    if (m_hud == Hud::Controls) {
        overlayControlsPage(frame);
        return;
    }
    if (m_hud == Hud::Debug) {
        overlayDebug(frame);
        return;
    }
    overlayGameHud(frame);
}

void App::overlayGameHud(const FrameInfo& frame) const {
    char time[16];
    formatTime(m_match.elapsed(), time, sizeof(time));
    gfx::text::print(6, 4, config::kColorGold, "VIVOS %d", m_match.aliveCount());
    gfx::text::print(96, 4, kWhite, "%s  %s", time, game::matchPhaseName(m_match.phase()));
    gfx::text::print(400, 4, kLightGrey, "%5.1f FPS", frame.timer.fps());

    if (m_buildMode) {
        const game::StructureSystem& s = m_match.structures();
        char banner[48];
        snprintf(banner, sizeof(banner), "MODO CONSTRUCCION   Paredes %d/%d", s.count(), s.config().maxWalls);
        printCenteredShadow(20, kWhite, banner);
        if (!m_wallPlan.valid) {
            printCenteredShadow(31, kErrorRed, game::placementErrorName(m_wallPlan.error));
        } else if (m_targetWall >= 0) {
            printCentered(31, config::kColorGold, "R: colocar  []: quitar esta pared  O: salir");
        } else {
            printCentered(31, kLightGrey, "R: colocar pared   O: volver al combate");
        }
    }

    // Registro de eliminaciones.
    int y = m_buildMode ? 47 : 22;
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

    const game::CharacterState& p = m_match.body(0).state();
    gfx::text::print(8, 257, kWhite, "+");
    gfx::text::print(26, 257, kWhite, "%3.0f|%3.0f", p.health, p.maxHealth);
    gfx::text::printShadow(180, 257, kWhite, "Bajas %d", m_match.playerKills());

    // Panel inferior derecho: arma o construccion.
    if (m_buildMode) {
        const game::StructureSystem& s = m_match.structures();
        gfx::text::print(355, 226, kBuildReticle, "PAREDES MADERA");
        gfx::text::print(400, 244, kWhite, "/ %d", s.config().maxWalls);
        gfx::text::print(400, 255, kLightGrey, "[] quitar");
    } else {
        const game::Weapon& w = m_match.playerWeapon();
        gfx::text::print(355, 226, config::kColorGold, "%s %s", w.config().name, w.config().rarity);
        if (w.reserve() < 0) gfx::text::print(400, 244, kWhite, "/ inf");
        else gfx::text::print(400, 244, kWhite, "/ %d", w.reserve());
        if (w.reloading()) gfx::text::print(400, 254, config::kColorGold, "RECARGA");
        else if (w.empty()) gfx::text::print(400, 254, kErrorRed, "SIN BALAS");
        else if (w.ammo() == 0) gfx::text::print(400, 254, kErrorRed, "[] recarga");
    }

    if (m_match.phase() != game::MatchPhase::Playing) overlayMatchEnd();
}

void App::overlayMatchEnd() const {
    char line[64];
    char time[16];
    formatTime(m_match.elapsed(), time, sizeof(time));
    if (m_match.phase() == game::MatchPhase::Finished && m_match.winner() == 0) {
        printCenteredShadow(102, config::kColorGold, "#1  VICTORIA MAGISTRAL");
        snprintf(line, sizeof(line), "Bajas: %d   Tiempo: %s", m_match.playerKills(), time);
        printCentered(118, kWhite, line);
    } else {
        snprintf(line, sizeof(line), "ELIMINADO - Puesto #%d de %d", m_match.playerPlace(), m_match.entityCount());
        printCenteredShadow(102, kErrorRed, line);
        if (m_match.phase() == game::MatchPhase::Finished) {
            snprintf(line, sizeof(line), "Ganador: %s", m_match.winner() >= 0 ? m_match.name(m_match.winner()) : "nadie");
        } else {
            snprintf(line, sizeof(line), "Quedan %d - la partida sigue", m_match.aliveCount());
        }
        printCentered(118, kWhite, line);
    }
    if (m_match.phase() == game::MatchPhase::Finished) {
        if (fmodf(m_blinkTime, 1.0f) < 0.65f) printCentered(138, config::kColorGold, "Pulsa X para jugar otra vez");
    } else {
        printCentered(138, kLightGrey, "O: camara libre para mirar");
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
    y += kLineHeight;
    gfx::text::print(x, y, kWhite, "Rumbo %3.0f  cam %3.0f", headingDegrees(p.yaw), headingDegrees(m_followCamera.yaw()));
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
    y += kLineHeight + 4;
    gfx::text::print(x, y, config::kColorGold, "PARTIDA");
    y += kLineHeight;
    gfx::text::print(x, y, kWhite, "Vivos %d/%d  zona %2.0f m", m_match.aliveCount(), m_match.entityCount(),
                     m_match.zoneRadius());
    y += kLineHeight;
    gfx::text::print(x, y, kWhite, "Caminos %d (fallos %d)", s.pathRequests, s.pathFailures);
    y += kLineHeight;
    gfx::text::print(x, y, kWhite, "Max nodos A* %d", s.maxPathExpansions);
    y += kLineHeight;
    gfx::text::print(x, y, kWhite, "Atascos %d", s.stuckEvents);
    y += kLineHeight;
    gfx::text::print(x, y, kWhite, "Disparos %d", s.shotsFired);
    y += kLineHeight;
    gfx::text::print(x, y, kWhite, " impactos %d pared %d", s.shotsHitEntity, s.shotsBlockedByWorld);
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

// --- Pagina de controles (herencia de la fase 1) ----------------------------------------

void App::renderControlsPage(const FrameInfo& frame) const {
    const u32 buttons = frame.input.rawButtons();

    gfx::fillRect(0, 0, gfx::kScreenWidth, 20, kBar);
    gfx::fillRect(6, 26, 300, 200, kPanel);
    gfx::fillRect(312, 26, 162, 200, kPanel);
    gfx::fillRect(0, 232, gfx::kScreenWidth, 40, kBar);

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
    const core::FrameTimer& t = frame.timer;
    const input::Input& in = frame.input;

    gfx::text::print(8, 6, config::kColorGold, "%s v%s", config::kGameTitle, config::kVersion);
    gfx::text::print(8 + 8 * 20, 6, kLightGrey, "%s", config::kPhase);
    gfx::text::print(400, 6, kWhite, "%5.1f FPS", t.fps());

    int y = 32;
    const int x = 14;
    gfx::text::print(x, y, config::kColorGold, "COMBATE");
    y += kLineHeight;
    gfx::text::print(x, y, kWhite, "Stick mover  X saltar");
    y += kLineHeight;
    gfx::text::print(x, y, kWhite, "R disparar  [] recargar");
    y += kLineHeight;
    gfx::text::print(x, y, kWhite, "L fijar obj.  Cruceta camara");
    y += kLineHeight;
    gfx::text::print(x, y, config::kColorGold, "CONSTRUCCION (O)");
    y += kLineHeight;
    gfx::text::print(x, y, kWhite, "R colocar  [] quitar");
    y += kLineHeight;
    gfx::text::print(x, y, kWhite, "^ camara libre (depuracion)");
    y += kLineHeight + 4;

    char held[40];
    heldButtonsString(in.rawButtons(), held, 26);
    gfx::text::print(x, y, config::kColorGold, "MANDO");
    y += kLineHeight;
    gfx::text::print(x, y, kWhite, "Botones: 0x%08X", static_cast<unsigned>(in.rawButtons()));
    y += kLineHeight;
    gfx::text::print(x, y, kWhite, "Pulsado: %s", held);
    y += kLineHeight;
    gfx::text::print(x, y, kWhite, "Ultimo : %s  total %u", m_lastButton, static_cast<unsigned>(m_pressCount));
    y += kLineHeight;
    gfx::text::print(x, y, kWhite, "Stick norm: %+.2f, %+.2f", in.analogX(), in.analogY());
    y += kLineHeight;
    gfx::text::print(x, y, kWhite, "CPU %d MHz / Bus %d MHz", m_cpuMhz, m_busMhz);

    gfx::text::print(339, 35, kWhite, "L");
    gfx::text::print(441, 35, kWhite, "R");
    gfx::text::print(366, 110, kLightGrey, "SEL");
    gfx::text::print(396, 110, kLightGrey, "STA");
    gfx::text::print(357, 194, kLightGrey, "ANALOGICO");

    gfx::text::print(8, 238, kWhite, "SELECT: siguiente HUD   ^: camara libre");
    gfx::text::print(8, 253, kLightGrey, "Salir: mantener L+R+START 1 s");
}

}  // namespace app
