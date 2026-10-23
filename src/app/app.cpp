#include "app/app.h"

#include <malloc.h>

#include <psppower.h>

#include "app/app_common.h"
#include "assets/embedded_assets.h"
#include "audio/audio_system.h"
#include "platform/log.h"
#include "world/world_units.h"

namespace app {
namespace {

using namespace common;
using math3d::Vec3;

constexpr float kLockAngleDegrees = 35.0f;  // L: objetivo dentro de este cono
constexpr float kLockDistance = 45.0f;
constexpr float kSoundRange = 60.0f;        // m: mas lejos no se oyen disparos de bots

// Aparicion del jugador: al sur del muro, mirando al deposito.
constexpr Vec3 kPlayerSpawn{0.0f, 0.0f, 24.0f};
constexpr float kPlayerSpawnYaw = 0.0f;

constexpr int kMinimapSize = 128;  // texeles por lado: 192 m / 128 = 1.5 m por texel

unsigned heapInUse() {
    const struct mallinfo mem = mallinfo();
    return static_cast<unsigned>(mem.uordblks);
}

}  // namespace

// --- Ciclo de vida ----------------------------------------------------------------------

void App::presentBootFrame(const char* status) {
    // Un frame completo fuera del bucle principal: la carga inicial no deja la pantalla
    // en negro (lo pesado es construir el escenario, ~1 s).
    gfx::beginFrame(rgba(0, 0, 0));
    if (m_login.pixels) gfx::drawTexture(m_login, 0.0f, 0.0f);
    gfx::fillRect(0, 244, gfx::kScreenWidth, 28, kBar);
    gfx::endFrame();
    gfx::text::setTarget(gfx::cpuDrawBuffer());
    printCenteredShadow(254, kWhite, status);
    gfx::present(true);
}

void App::logStage(const char* stage, u64 startMicros) const {
    platform::log::write("carga: %-22s %4u ms, heap en uso %u KB", stage,
                         static_cast<unsigned>((core::nowMicros() - startMicros) / 1000),
                         heapInUse() / 1024);
}

void App::init() {
    const u64 bootStart = core::nowMicros();
    u64 t0 = bootStart;
    const assets::Blob login = assets::loginPng();
    // Las imagenes a pantalla completa son opacas: a 16 bits ocupan la mitad (255 KB).
    if (!gfx::loadPngFromMemory(m_login, login.data, login.size, GU_PSM_5551)) {
        platform::log::write("ERROR: login.png");
    }
    presentBootFrame("Cargando...");

    const assets::Blob loading = assets::loadscreenPng();
    if (!gfx::loadPngFromMemory(m_loadscreen, loading.data, loading.size, GU_PSM_5551)) {
        platform::log::write("ERROR: loadscreen.png");
    }
    if (!m_atlas.load()) platform::log::write("ERROR: ui_atlas.png");
    platform::log::write("imagenes: inicio %u KB, carga %u KB, atlas de interfaz %u KB",
                         static_cast<unsigned>(gfx::textureBytes(m_login) / 1024),
                         static_cast<unsigned>(gfx::textureBytes(m_loadscreen) / 1024),
                         static_cast<unsigned>(m_atlas.bytes() / 1024));
    logStage("imagenes de interfaz", t0);

    t0 = core::nowMicros();
    m_audioReady = audio::init();
    platform::log::write("audio: %s, recursos %u KB", m_audioReady ? "ok" : "ERROR (sin sonido)",
                         audio::resourceBytes() / 1024);
    if (m_audioReady) audio::mixer().playMusic(true);
    logStage("audio", t0);

    presentBootFrame("Construyendo el escenario...");
    t0 = core::nowMicros();
    m_worldReady = m_world.build();
    platform::log::write("escenario: %s, %d lotes, %d triangulos, geometria %u KB, texturas %u KB "
                         "(%s), %d cajas de colision",
                         m_worldReady ? "ok" : "ERROR (memoria)", m_world.batchCount(),
                         m_world.totalTriangles(), static_cast<unsigned>(m_world.geometryBytes() / 1024),
                         static_cast<unsigned>(m_world.textureBytes() / 1024),
                         m_world.textures().fromResources() ? "Recursos/Texturas" : "procedurales",
                         m_world.collision().count());
    logStage("escenario", t0);

    t0 = core::nowMicros();
    m_nav.build(m_world.collision(), world::kMapHalfSize);
    platform::log::write("navegacion: %d de %d nodos transitables", m_nav.walkableCount(),
                         m_nav.size() * m_nav.size());
    logStage("navegacion", t0);

    t0 = core::nowMicros();
    const game::CharacterPalette* palettes[kModelCount] = {
        &game::palettes::kPlayer, &game::palettes::kCommando, &game::palettes::kOrange,
        &game::palettes::kRanger};
    m_modelsReady = true;
    for (int i = 0; i < kModelCount; ++i) m_modelsReady &= m_models[i].build(*palettes[i]);
    platform::log::write("modelos de personaje: %s, %d triangulos cada uno",
                         m_modelsReady ? "ok" : "ERROR", m_models[0].triangles());
    m_buildVisualsReady = m_buildVisuals.build(m_matchConfig.build);
    m_lobbyReady = m_lobbyScene.build();
    buildMinimap();
    logStage("modelos, lobby, minimapa", t0);

    m_projection.fovYDegrees = config::kFovYDegrees;
    m_projection.aspect = static_cast<float>(gfx::kScreenWidth) / gfx::kScreenHeight;
    m_projection.zNear = config::kNearPlane;
    m_projection.zFar = config::kFarPlane;

    m_cpuMhz = scePowerGetCpuClockFrequencyInt();
    m_busMhz = scePowerGetBusClockFrequencyInt();
    m_lobbyYaw = 3.45f;  // mirando a la camara del lobby
    m_startHeapBytes = heapInUse();
    logStage("TOTAL arranque", bootStart);
}

void App::buildMinimap() {
    if (!gfx::createTexture(m_minimap, kMinimapSize, kMinimapSize, GU_PSM_5551)) return;
    u16* pixels = static_cast<u16*>(m_minimap.pixels);
    const float texel = 2.0f * world::kMapHalfSize / kMinimapSize;
    for (int v = 0; v < kMinimapSize; ++v) {
        for (int u = 0; u < kMinimapSize; ++u) {
            // Norte (-Z) arriba y este (+X) a la derecha.
            const u32 c = world::mapColorAt(-world::kMapHalfSize + (u + 0.5f) * texel,
                                            -world::kMapHalfSize + (v + 0.5f) * texel);
            pixels[v * m_minimap.stride + u] = gfx::pack5551(c & 0xFF, (c >> 8) & 0xFF, (c >> 16) & 0xFF);
        }
    }
    gfx::uploadTexture(m_minimap);
}

void App::startMatch() {
    const u64 t0 = core::nowMicros();
    m_matchConfig = game::MatchConfig{};
    m_matchConfig.botCount = m_botCount;
    // Semilla distinta en cada partida (el reloj del sistema).
    m_matchConfig.seed = static_cast<u32>(core::nowMicros()) ^ (0x9E3779B9u * ++m_matchesPlayed);
    m_match.start(m_matchConfig, m_world.mutableCollision(), m_nav, m_world.pointsOfInterest(),
                  kPlayerSpawn, kPlayerSpawnYaw);
    m_eventCursor = m_match.eventCount();
    m_playerKiller = -1;
    m_buildMode = false;
    m_targetWall = -1;
    m_timestep.reset();
    m_playerAnim = game::CharacterAnim{};
    for (game::CharacterAnim& a : m_botAnims) a = game::CharacterAnim{};
    m_followCamera.reset(m_match.body(0).state().position, kPlayerSpawnYaw);
    m_freeCameraActive = false;
    m_lockTarget = -1;
    m_paused = false;
    m_results = Results::None;
    m_resultsTime = m_resultsDelay = 0.0f;
    m_spectating = false;
    m_resultsDismissed = false;
    m_matchFrameMsSum = m_matchFrameMsMax = 0.0f;
    m_matchFrames = 0;
    m_matchLogged = false;
    platform::log::write("partida %d: %d bots, semilla %u, preparada en %u ms, heap %u KB "
                         "(%+d KB desde el lobby inicial), %d cajas de colision",
                         m_matchesPlayed, m_botCount, static_cast<unsigned>(m_matchConfig.seed),
                         static_cast<unsigned>((core::nowMicros() - t0) / 1000), heapInUse() / 1024,
                         static_cast<int>(heapInUse() / 1024) - static_cast<int>(m_startHeapBytes / 1024),
                         m_world.collision().count());
}

void App::goToLobby() {
    // La imagen de inicio de sesion ya no se vuelve a usar.
    if (m_login.pixels) gfx::destroyTexture(m_login);
    m_screen = Screen::Lobby;
    m_paused = false;
    m_results = Results::None;
    m_lobbyItem = LobbyItem::Play;
    m_confirmExit = false;
    if (m_audioReady) audio::mixer().playMusic(true);
}

void App::shutdown() {
    if (m_audioReady) audio::shutdown();
    m_audioReady = false;
    m_lobbyScene.destroy();
    m_lobbyReady = false;
    m_buildVisuals.destroy();
    m_buildVisualsReady = false;
    for (game::CharacterModel& m : m_models) m.destroy();
    m_modelsReady = false;
    m_world.destroy();
    m_worldReady = false;
    gfx::destroyTexture(m_minimap);
    m_atlas.destroy();
    gfx::destroyTexture(m_loadscreen);
    gfx::destroyTexture(m_login);
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
        case Screen::Login: updateLogin(in, dt); break;
        case Screen::Lobby: updateLobby(in, dt); break;
        case Screen::Controls: updateControlsScreen(in); break;
        case Screen::Loading: updateLoading(dt); break;
        case Screen::Game: updateGame(in, dt); break;
    }
}

game::CharacterState App::renderState(int id) const {
    return m_match.body(id).interpolated(m_timestep.alpha());
}

int App::spectateTarget() const {
    if (m_match.alive(0)) return 0;
    if (m_playerKiller > 0 && m_match.alive(m_playerKiller)) return m_playerKiller;
    for (int i = 1; i < m_match.entityCount(); ++i) {
        if (m_match.alive(i)) return i;
    }
    return 0;
}

void App::updateGame(const input::Input& in, float dt) {
    if (m_paused) {
        updatePause(in);
        return;
    }
    updateResults(in, dt);
    if (m_screen != Screen::Game) return;  // los resultados han vuelto al lobby o a cargar
    const bool overlay = m_results != Results::None;

    if (!overlay && in.pressed(PSP_CTRL_SELECT)) {
        m_hud = static_cast<Hud>((static_cast<int>(m_hud) + 1) % static_cast<int>(Hud::Count));
    }
    // START: pausa (si no se esta haciendo la combinacion de salida L+R+START).
    if (!overlay && !m_freeCameraActive && in.pressed(PSP_CTRL_START) &&
        !in.held(PSP_CTRL_LTRIGGER) && !in.held(PSP_CTRL_RTRIGGER)) {
        m_paused = true;
        m_pauseItem = 0;
        playSfx(audio::Sfx::MenuAccept, 0.7f);
        return;
    }
    // TRIANGULO: camara libre de depuracion. Dentro de ella, CUADRADO = niebla y
    // START = VSync (el jugador no actua, asi que esos botones estan libres).
    if (!overlay && in.pressed(PSP_CTRL_TRIANGLE)) {
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

    const bool control = !overlay && !m_freeCameraActive && m_match.phase() == game::MatchPhase::Playing;
    // CIRCULO: modo construccion. Se desactiva solo si el jugador cae.
    if (control && in.pressed(PSP_CTRL_CIRCLE)) {
        m_buildMode = !m_buildMode;
        playSfx(audio::Sfx::MenuMove, 0.6f);
    }
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

    // 3) Sucesos de la partida -> sonido y reacciones de las animaciones.
    processEvents();

    // 4) Animaciones (visuales, con el tiempo de frame). Los eliminados tambien: caen.
    m_playerAnim.update(renderState(0), m_match.playerAiming(), m_match.alive(0), dt);
    for (int i = 0; i < m_match.botCount(); ++i) {
        const game::Bot& b = m_match.bot(i);
        m_botAnims[i].update(renderState(i + 1), b.aiming, b.state != game::BotState::Eliminated, dt);
    }

    // 5) Camaras y construccion.
    updateGameCameras(in, dt, control, lockHeld);
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

void App::updateGameCameras(const input::Input& in, float dt, bool control, bool lockHeld) {
    // L: fijar el bot visible mas centrado (o recolocar si no hay ninguno).
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
        return;
    }
    // Tras caer, la camara sigue a quien elimino al jugador (o a otro superviviente).
    const game::CharacterState followed = renderState(spectateTarget());
    m_followCamera.update(in, followed.position, followed.yaw, dt, recenter, lock, &m_world.collision());
}

void App::playSfx(audio::Sfx sfx, float volume, float pan) const {
    if (m_audioReady) audio::mixer().play(sfx, volume, pan);
}

void App::playSpatial(audio::Sfx sfx, const Vec3& position, float maxVolume) const {
    const Vec3 eye = cameraEye();
    const Vec3 delta = position - eye;
    const float distance = math3d::length(delta);
    const float volume = maxVolume * (1.0f - distance / kSoundRange);
    if (volume < 0.04f) return;
    float pan = 0.0f;
    if (distance > 0.5f) {
        // Derecha de la camara = adelante x arriba = (-z, 0, x).
        const Vec3 f = m_followCamera.lookDirection();
        const Vec3 right = math3d::normalize(Vec3{-f.z, 0.0f, f.x});
        pan = 0.8f * math3d::dot(delta, right) / distance;
    }
    playSfx(sfx, volume, pan);
}

void App::processEvents() {
    const u32 count = m_match.eventCount();
    // Si se acumularon mas de los que caben (frame muy largo), se pierden los mas viejos.
    if (count - m_eventCursor > static_cast<u32>(game::Match::kMaxEvents)) {
        m_eventCursor = count - game::Match::kMaxEvents;
    }
    for (; m_eventCursor < count; ++m_eventCursor) {
        const game::GameEvent& e = m_match.event(m_eventCursor);
        switch (e.type) {
            case game::GameEventType::Shot:
                if (e.entity == 0) {
                    playSfx(audio::Sfx::Shot, 0.85f);
                    m_playerAnim.onShot();
                } else if (e.entity > 0) {
                    playSpatial(audio::Sfx::Shot, e.position, 0.7f);
                    m_botAnims[e.entity - 1].onShot();
                }
                break;
            case game::GameEventType::HitEntity:
                if (e.entity > 0) m_botAnims[e.entity - 1].onHit();
                if (e.other == 0) playSfx(audio::Sfx::HitMarker, 0.9f);
                break;
            case game::GameEventType::HitStructure:
                if (e.other == 0) playSfx(audio::Sfx::HitMarker, 0.4f);
                break;
            case game::GameEventType::PlayerHurt:
                playSfx(audio::Sfx::Hurt, 0.9f);
                m_playerAnim.onHit();
                break;
            case game::GameEventType::Eliminated:
                if (e.entity == 0) m_playerKiller = e.other;
                if (e.other == 0) playSfx(audio::Sfx::Elimination, 1.0f);
                break;
            case game::GameEventType::WallBuilt:
                playSfx(audio::Sfx::WallBuild, 0.8f);
                break;
            case game::GameEventType::WallRemoved:
                playSfx(audio::Sfx::WallBuild, 0.5f);
                break;
            case game::GameEventType::WallBroken:
                playSpatial(audio::Sfx::WallBreak, e.position, 0.9f);
                break;
            case game::GameEventType::Reload:
                playSfx(audio::Sfx::Reload, 0.8f);
                break;
        }
    }
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
            "paredes construidas) | paredes: %d construidas, %d quitadas, %d rotas | heap %u KB, "
            "voces de audio descartadas %u",
            m_match.name(m_match.winner()), m_match.playerPlace(), m_match.playerKills(),
            static_cast<double>(m_match.elapsed()), m_botCount,
            static_cast<double>(m_matchFrames ? m_matchFrameMsSum / m_matchFrames : 0.0f),
            static_cast<double>(m_matchFrameMsMax),
            static_cast<double>(s.aiMicros / 1000.0 / (m_match.elapsed() > 0.0f ? m_match.elapsed() : 1.0f)),
            s.decisions, s.pathRequests, s.maxPathExpansions, s.stuckEvents, s.shotsFired,
            s.shotsHitEntity, s.shotsBlockedByWorld, s.shotsHitStructure, s.wallsBuilt, s.wallsRemoved,
            s.wallsDestroyed, heapInUse() / 1024,
            static_cast<unsigned>(m_audioReady ? audio::mixer().droppedCommands() : 0));
    }
}

u32 App::clearColor() const {
    // En la partida, mismo color que la niebla: los objetos lejanos se funden con el fondo.
    return m_screen == Screen::Game ? config::kColorSky : kNavy;
}

math3d::Vec3 App::cameraEye() const {
    return m_freeCameraActive ? m_freeCamera.eye() : m_followCamera.eye();
}

bool App::projectToScreen(const math3d::Mat4& viewProjection, const Vec3& p, float* x, float* y) const {
    const float* m = viewProjection.m;
    const float w = m[3] * p.x + m[7] * p.y + m[11] * p.z + m[15];
    if (w < 0.2f) return false;  // detras de la camara
    const float sx = (m[0] * p.x + m[4] * p.y + m[8] * p.z + m[12]) / w;
    const float sy = (m[1] * p.x + m[5] * p.y + m[9] * p.z + m[13]) / w;
    *x = (sx * 0.5f + 0.5f) * gfx::kScreenWidth;
    *y = (0.5f - sy * 0.5f) * gfx::kScreenHeight;
    return true;
}

void App::render(const FrameInfo& frame) {
    switch (m_screen) {
        case Screen::Login: renderLogin(); break;
        case Screen::Lobby: renderLobby(); break;
        case Screen::Controls: renderControlsScreen(frame); break;
        case Screen::Loading: renderLoading(); break;
        case Screen::Game:
            renderGame(frame);
            if (m_results != Results::None) renderResults();
            if (m_paused) renderPause();
            break;
    }
    if (m_exitProgress > 0.0f) renderExitBar(140, 4, 200, 4);
}

void App::renderOverlay(const FrameInfo& frame) const {
    switch (m_screen) {
        case Screen::Login: overlayLogin(); break;
        case Screen::Lobby: overlayLobby(); break;
        case Screen::Controls: overlayControlsScreen(frame); break;
        case Screen::Loading: overlayLoading(); break;
        case Screen::Game:
            if (m_results != Results::None) {
                overlayResults();
            } else {
                overlayGame(frame);
            }
            if (m_paused) overlayPause();
            break;
    }
}

void App::renderExitBar(float x, float y, float w, float h) const {
    gfx::fillRect(x, y, w, h, kButtonOff);
    gfx::fillRect(x, y, w * m_exitProgress, h, rgba(220, 60, 50));
}

// --- Partida: dibujo 3D ---------------------------------------------------------------------

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
            if (m_results == Results::None) renderGameHud();
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

}  // namespace app
