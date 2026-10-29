#include "app/app.h"

#include <malloc.h>
#include <math.h>
#include <string.h>

#include <psppower.h>

#include "app/app_common.h"
#include "assets/embedded_assets.h"
#include "audio/audio_system.h"
#include "platform/log.h"
#include "world/storm.h"
#include "world/water.h"
#include "world/world_units.h"
#include "world/zombie_sites.h"

namespace app {
namespace {

using namespace common;
using math3d::Vec3;

// L: objetivo dentro de este cono (fase 7: mas estrecho y la camara gira con velocidad
// limitada; antes, 35 grados y giro inmediato: acertar era casi automatico).
constexpr float kLockAngleDegrees = 22.0f;
constexpr float kLockDistance = 40.0f;
constexpr float kChestHumRange = 9.0f;  // m: se oye el zumbido de un cofre sin abrir
constexpr float kSoundRange = 60.0f;
constexpr float kFootstepRange = 12.0f;
// Tormenta del limite del mapa (world/storm.h): niebla y color.
constexpr float kStormFogRange = 30.0f;  // m antes del borde en que la niebla empieza a espesarse
constexpr u32 kStormFogColor = gfx::rgba(150, 92, 200);
constexpr u32 kStormInsideColor = gfx::rgba(98, 54, 142);        // m: pasos de los bots que se oyen


// Aparicion del jugador: al sur del muro, mirando al deposito.
constexpr Vec3 kPlayerSpawn{0.0f, 0.0f, 24.0f};
constexpr float kPlayerSpawnYaw = 0.0f;

constexpr int kMinimapSize = 128;  // texeles por lado: 192 m / 128 = 1.5 m por texel
// L + R "en seco": las dos pulsaciones a la vez (como mucho 4 frames entre una y otra). Si
// una ya estaba mantenida de antes, no cuenta.
constexpr float kChordWindow = 0.07f;

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

void App::setDataDirectory(const char* dir) {
    const char* sep = dir && dir[0] && dir[strlen(dir) - 1] != '/' ? "/" : "";
    snprintf(m_profilePath, sizeof(m_profilePath), "%s%sperfil.txt", dir ? dir : "", sep);
    snprintf(m_settingsPath, sizeof(m_settingsPath), "%s%sajustes.txt", dir ? dir : "", sep);
}

void App::init() {
    const u64 bootStart = core::nowMicros();
    u64 t0 = bootStart;
    // Ajustes primero: el idioma ya sale bien en la pantalla de carga.
    if (m_settings.load(m_settingsPath)) {
        platform::log::write("ajustes cargados de %s", m_settingsPath);
    } else {
        platform::log::write("sin ajustes en %s: valores por defecto", m_settingsPath);
        // Primer arranque: se elige el idioma antes del inicio de sesion.
        m_screen = Screen::Language;
        m_langChoice = m_settings.language == i18n::Lang::En ? 1 : 0;
    }
    applySettings();
    // Perfil guardado (nombre y estadisticas); si no existe, se pide el nombre tras el inicio.
    if (m_profile.load(m_profilePath)) {
        platform::log::write("perfil de %s: %d partidas, %d victorias, %d bajas", m_profile.name, m_profile.matches,
                             m_profile.wins, m_profile.kills);
    } else {
        platform::log::write("sin perfil en %s: se pedira el nombre", m_profilePath);
    }
    const assets::Blob login = assets::loginPng();
    // Las imagenes a pantalla completa son opacas: a 16 bits ocupan la mitad (255 KB).
    if (!gfx::loadPngFromMemory(m_login, login.data, login.size, GU_PSM_5551)) {
        platform::log::write("ERROR: login.png");
    }
    presentBootFrame(i18n::tr(i18n::T::BootLoading));

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

    presentBootFrame(i18n::tr(i18n::T::BootBuilding));
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
    m_match.setHarvestables(&m_world.harvestables());
    platform::log::write("objetos recolectables con el pico: %d",
                         static_cast<int>(m_world.harvestables().size()));
    platform::log::write("navegacion: %d de %d nodos transitables", m_nav.walkableCount(),
                         m_nav.size() * m_nav.size());
    logStage("navegacion", t0);

    t0 = core::nowMicros();
    // El jugador, con el traje equipado en la taquilla (de momento, el de por defecto).
    const Cosmetic* outfit = m_profile.equippedIn(CosmeticSlot::Outfit);
    const game::CharacterPalette* palettes[kModelCount] = {
        outfit && outfit->palette ? outfit->palette : &game::palettes::kPlayer, &game::palettes::kCommando,
        &game::palettes::kOrange, &game::palettes::kRanger, &game::palettes::kZombie, &game::palettes::kZombieWorker};
    m_modelsReady = true;
    for (int i = 0; i < kModelCount; ++i) m_modelsReady &= m_models[i].build(*palettes[i]);
    platform::log::write("modelos de personaje: %s, %d triangulos (%d el de lejos)",
                         m_modelsReady ? "ok" : "ERROR", m_models[0].triangles(), m_models[0].lowDetailTriangles());
    if (!m_items.build()) platform::log::write("ERROR: modelos de armas y objetos");
    platform::log::write("modelos de objetos: SCAR %d triangulos (%d el de lejos), lanzacohetes %d",
                         m_items.triangles(game::ItemModel::Scar), m_items.triangles(game::ItemModel::Scar, true),
                         m_items.triangles(game::ItemModel::RocketLauncher));
    m_buildVisualsReady = m_buildVisuals.build(m_matchConfig.build);
    m_propsReady = m_props.build();
    platform::log::write("botin, autobus y ala delta: %s, %d triangulos en total", m_propsReady ? "ok" : "ERROR",
                         m_props.triangles());
    m_lobbyReady = m_lobbyScene.build();
    buildMinimap();
    buildZombieMinimap();
    buildSky();
    {
        const assets::Blob scope = assets::scopePng();
        if (!gfx::loadPngFromMemory(m_scope, scope.data, scope.size, GU_PSM_5551)) {
            platform::log::write("aviso: no se pudo cargar la mirilla");
        }
    }
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
    m_matchConfig.forceLlama = m_testForceLlama;
    m_matchConfig.mode = m_gameMode;
    // Trucos del menu de administrador (solo si esta desbloqueado).
    m_matchConfig.cheats = m_settings.admin ? m_settings.cheats : game::Cheats{};
    m_matchConfig.forceLlama |= m_matchConfig.cheats.alwaysLlama;
    if (m_matchConfig.cheats.legendaryKit) {
        // Kit legendario: lo mejor de cada familia (en Monodisparo, de las suyas), curas,
        // municion y materiales.
        using game::ItemType;
        using game::Rarity;
        const bool oneShot = m_gameMode == game::GameMode::OneShot;
        m_matchConfig.startItems[0] = {oneShot ? ItemType::Sniper : ItemType::AssaultRifle, 1, Rarity::Legendary};
        m_matchConfig.startItems[1] = {oneShot ? ItemType::ScopedRifle : ItemType::Shotgun, 1,
                                       oneShot ? Rarity::Epic : Rarity::Legendary};
        m_matchConfig.startItems[2] = {oneShot ? ItemType::Grenade : ItemType::RocketLauncher, oneShot ? 6 : 1,
                                       oneShot ? Rarity::Common : Rarity::Legendary};
        m_matchConfig.startItems[3] = {ItemType::ShieldPotion, 3, Rarity::Rare};
        m_matchConfig.startItems[4] = {ItemType::Medkit, 3, Rarity::Uncommon};
        for (int a = 0; a < static_cast<int>(game::AmmoType::Count); ++a) m_matchConfig.startAmmo[a] = 120;
        for (int r = 0; r < static_cast<int>(world::Resource::Count); ++r) m_matchConfig.startMaterials[r] = 500;
    }
    // Supervivencia zombi (v0.14): sus reglas, el mapa en ruinas y se despierta en la base.
    const bool zombies = m_gameMode == game::GameMode::Zombies;
    if (zombies) configureZombieMatch();
    m_world.setTheme(zombies ? world::TextureTheme::Ruined : world::TextureTheme::Normal);
    const Vec3 spawn = zombies ? world::zsites::kWakeSpot : kPlayerSpawn;
    const float spawnYaw = zombies ? world::zsites::kWakeYaw : kPlayerSpawnYaw;
    m_match.start(m_matchConfig, m_world.mutableCollision(), m_nav, m_world.pointsOfInterest(), spawn, spawnYaw);
    m_match.setPlayerName(m_profile.name);
    // Con menos de 30 bots la partida es de practica: ni pavos ni logros (el modo zombi, si).
    m_rewardsActive = zombies || m_botCount >= game::kRewardBots;
    m_wakeTime = zombies ? 0.0f : 99.0f;
    m_craftOpen = false;
    m_craftMessageTime = 0.0f;
    m_zMessageTime = 0.0f;
    m_zMessage = i18n::T::None;
    m_matchBestNights = m_profile.zombieBestNights;
    m_eventCursor = m_match.eventCount();
    m_playerKiller = -1;
    m_buildMode = false;
    m_buildPiece = 0;
    m_harvestAge = 99.0f;
    m_targetWall = -1;
    m_timestep.reset();
    m_playerAnim = game::CharacterAnim{};
    for (game::CharacterAnim& a : m_botAnims) a = game::CharacterAnim{};
    m_playerSteps = 0;
    for (u32& s : m_botSteps) s = 0;
    // En el autobus, vista de tres cuartos (el autobus va a lo largo de Z).
    m_followCamera.reset(m_match.body(0).state().position + Vec3{0.0f, 4.0f, 0.0f},
                         m_match.airState(0) == game::AirState::Bus ? 0.9f : spawnYaw);
    m_freeCameraActive = false;
    m_lockTarget = -1;
    m_matchRecorded = false;
    m_matchVbucks = 0;
    m_inventoryOpen = false;
    m_invHeld = -1;
    m_lPressAge = m_rPressAge = 99.0f;
    m_useSfx = m_reloadSfx = audio::Sfx::Count;
    m_mapOpen = false;
    m_swapOpen = false;
    m_wasScoped = false;
    m_dancing = false;
    m_chestHumTimer = 0.0f;
    m_airdropMessage = 0.0f;
    m_stormMessage = 0.0f;
    m_paused = false;
    m_results = Results::None;
    m_resultsTime = m_resultsDelay = 0.0f;
    m_spectating = false;
    m_resultsDismissed = false;
    m_resultSoundPlayed = false;
    m_matchFrameMsSum = m_matchFrameMsMax = 0.0f;
    m_matchFrames = 0;
    m_matchLogged = false;
    platform::log::write("partida %d (%s): %d bots, semilla %u, preparada en %u ms, heap %u KB "
                         "(%+d KB desde el lobby inicial), %d cajas de colision",
                         m_matchesPlayed, zombies ? "zombis" : (m_rewardsActive ? "con recompensas" : "practica"),
                         zombies ? m_matchConfig.botCount : m_botCount, static_cast<unsigned>(m_matchConfig.seed),
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
    m_confirmExit = false;
    m_modeOpen = false;
    m_shopConfirm = false;
    m_inventoryOpen = false;
    m_modMenuOpen = false;
    m_passwordOpen = false;
    m_craftOpen = false;
    if (m_audioReady && m_useSfx != audio::Sfx::Count) audio::mixer().stop(m_useSfx);
    if (m_audioReady && m_reloadSfx != audio::Sfx::Count) audio::mixer().stop(m_reloadSfx);
    m_useSfx = m_reloadSfx = audio::Sfx::Count;
    if (m_audioReady) audio::mixer().playMusic(true);
}

void App::shutdown() {
    // Contadores de los logros que aun no se habian guardado (p. ej. al salir con HOME).
    if (m_profileDirty && m_profile.hasName()) saveProfile();
    if (m_audioReady) audio::shutdown();
    m_audioReady = false;
    m_lobbyScene.destroy();
    m_lobbyReady = false;
    m_buildVisuals.destroy();
    m_buildVisualsReady = false;
    m_props.destroy();
    m_propsReady = false;
    for (game::CharacterModel& m : m_models) m.destroy();
    m_modelsReady = false;
    m_items.destroy();
    m_world.destroy();
    m_worldReady = false;
    gfx::destroyTexture(m_minimap);
    gfx::destroyTexture(m_minimapZombie);
    gfx::destroyTexture(m_vignette);
    gfx::destroyMesh(m_sky);
    gfx::destroyMesh(m_skyZombie);
    m_skyReady = false;
    gfx::destroyTexture(m_scope);
    m_atlas.destroy();
    gfx::destroyTexture(m_loadscreen);
    gfx::destroyTexture(m_login);
}

// --- Logica -----------------------------------------------------------------------------

void App::update(const input::Input& in, float dt) {
    m_blinkTime += dt;
    updateAchievementToasts(dt);
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
        case Screen::Language: updateLanguage(in); break;
        case Screen::Login: updateLogin(in, dt); break;
        case Screen::Nickname: updateNickname(in); break;
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
    // Botones de las acciones (ajustes > controles). Los fijos (cruceta izquierda/derecha,
    // SELECT, START, L+SELECT y la salida L+R+START) siguen con los botones fisicos.
    const u32 kJump = m_settings.button(Action::Jump), kBuild = m_settings.button(Action::Build);
    const u32 kInteract = m_settings.button(Action::Interact), kPickaxe = m_settings.button(Action::Pickaxe);
    const u32 kFire = m_settings.button(Action::Fire), kAim = m_settings.button(Action::Aim);
    const u32 kCrouch = m_settings.button(Action::Crouch), kEmote = m_settings.button(Action::Emote);
    updateResults(in, dt);
    if (m_screen != Screen::Game) return;  // los resultados han vuelto al lobby o a cargar
    const bool overlay = m_results != Results::None;

    // SELECT: mapa a pantalla completa. L + SELECT: paginas del HUD (depuracion, controles...).
    if (!overlay && in.pressed(PSP_CTRL_SELECT)) {
        if (in.held(PSP_CTRL_LTRIGGER)) {
            m_hud = static_cast<Hud>((static_cast<int>(m_hud) + 1) % static_cast<int>(Hud::Count));
        } else {
            m_mapOpen = !m_mapOpen;
            playSfx(audio::Sfx::MenuMove, 0.6f);
        }
    }
    if (overlay) m_mapOpen = false;
    // START: pausa (si no se esta haciendo la combinacion de salida L+R+START).
    if (!overlay && !m_freeCameraActive && in.pressed(PSP_CTRL_START) &&
        !in.held(PSP_CTRL_LTRIGGER) && !in.held(PSP_CTRL_RTRIGGER)) {
        m_paused = true;
        m_pauseItem = 0;
        playSfx(audio::Sfx::MenuAccept, 0.7f);
        return;
    }
    // Camara libre de depuracion: TRIANGULO en la pagina de depuracion del HUD (fuera de
    // ella TRIANGULO es el pico). Dentro, CUADRADO = niebla y START = VSync.
    const bool freeCameraToggle = !overlay && in.pressed(PSP_CTRL_TRIANGLE) &&
                                  (m_freeCameraActive || m_hud == Hud::Debug);
    if (freeCameraToggle) {
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
    // En el autobus o cayendo solo cuentan el stick y X (saltar / abrir el ala delta).
    const bool onGround = m_match.airState(0) == game::AirState::Ground;
    // L + R a la vez: inventario (ordenar y soltar). Mientras esta abierto, el jugador no se
    // mueve ni actua (la partida sigue). Mantener L y pulsar R despues (o al reves) no cuenta.
    if (in.pressed(PSP_CTRL_LTRIGGER)) m_lPressAge = 0.0f;
    if (in.pressed(PSP_CTRL_RTRIGGER)) m_rPressAge = 0.0f;
    const bool chord = (in.pressed(PSP_CTRL_LTRIGGER) || in.pressed(PSP_CTRL_RTRIGGER)) && in.held(PSP_CTRL_LTRIGGER) &&
                       in.held(PSP_CTRL_RTRIGGER) && !in.held(PSP_CTRL_START) &&
                       fabsf(m_lPressAge - m_rPressAge) <= kChordWindow;
    m_lPressAge += dt;
    m_rPressAge += dt;
    if (m_inventoryOpen && (!control || !onGround || !m_match.alive(0))) m_inventoryOpen = false;
    // Modo zombi: el despertar (los primeros segundos no se mueve), los avisos y el menu de
    // crafteo (como el inventario: la partida sigue y el jugador no actua).
    m_wakeTime += dt;
    if (m_zMessageTime > 0.0f) m_zMessageTime -= dt;
    if (m_craftMessageTime > 0.0f) m_craftMessageTime -= dt;
    if (m_craftOpen && (!control || !onGround || m_match.nearbyWorkbench() < 0)) m_craftOpen = false;
    const bool craftWasOpen = m_craftOpen;
    if (m_craftOpen && !chord) updateCraftMenu(in);
    const bool waking = m_wakeTime < 2.2f;
    // Lo que cierra el inventario (CIRCULO) no cuenta despues como accion de la partida.
    const bool inventoryWasOpen = m_inventoryOpen || craftWasOpen || waking;
    if (chord && control && onGround && !craftWasOpen) {
        m_inventoryOpen = !m_inventoryOpen;
        m_invCursor = m_match.inventory().selected();
        m_invHeld = -1;
        m_swapOpen = false;
        m_buildMode = false;
        m_dancing = false;
        playSfx(audio::Sfx::MenuAccept, 0.6f);
    } else if (m_inventoryOpen) {
        updateInventoryScreen(in);
    }
    const bool inventory = inventoryWasOpen || m_inventoryOpen || chord;
    const bool act = control && onGround && !inventory;
    if (!onGround) m_buildMode = false;
    game::PlayerCommands commands;
    const bool usingItem = m_match.useProgress() >= 0.0f;
    // Selector de que soltar (inventario lleno): mientras esta abierto, la cruceta elige la
    // ranura, CUADRADO cambia y CIRCULO cancela. Se cierra si el objeto deja de estar al alcance.
    if (m_swapOpen && (!act || m_buildMode || !m_match.pickupInReach(m_swapPickup))) m_swapOpen = false;
    const bool swapInput = m_swapOpen;
    if (m_swapOpen) {
        const int step = (in.pressed(PSP_CTRL_RIGHT) ? 1 : 0) - (in.pressed(PSP_CTRL_LEFT) ? 1 : 0);
        if (step != 0) {
            m_swapCursor = (m_swapCursor + game::Inventory::kSlots + step) % game::Inventory::kSlots;
            playSfx(audio::Sfx::MenuMove, 0.5f);
        }
        if (in.pressed(kInteract)) {
            commands.swapPickup = m_swapPickup;
            commands.swapSlot = m_swapCursor;
            m_swapOpen = false;
        } else if (in.pressed(kBuild)) {
            m_swapOpen = false;
            playSfx(audio::Sfx::MenuMove, 0.5f);
        }
    }
    // CIRCULO: modo construccion. Se desactiva solo si el jugador cae.
    if (act && !swapInput && in.pressed(kBuild)) {
        m_buildMode = !m_buildMode;
        playSfx(audio::Sfx::MenuMove, 0.6f);
    }
    // TRIANGULO: sacar / guardar el pico (desde construccion, vuelve al combate con el pico).
    if (act && !freeCameraToggle && !swapInput && in.pressed(kPickaxe)) {
        if (m_buildMode) {
            m_buildMode = false;
            if (!m_match.inventory().pickaxe()) commands.togglePickaxe = true;
        } else {
            commands.togglePickaxe = true;
        }
        playSfx(audio::Sfx::MenuMove, 0.6f);
    }
    // Cruceta izquierda / derecha: ranura del inventario o pieza de construccion.
    const int dpad = swapInput ? 0 : (in.pressed(PSP_CTRL_RIGHT) ? 1 : 0) - (in.pressed(PSP_CTRL_LEFT) ? 1 : 0);
    if (act && dpad != 0) {
        if (m_buildMode) m_buildPiece = (m_buildPiece + kBuildPieces + dpad) % kBuildPieces;
        else commands.cycleSlot = dpad;
        playSfx(audio::Sfx::MenuMove, 0.5f);
    }
    if (!m_match.alive(0)) m_buildMode = false;
    // Cruceta abajo: agacharse / levantarse. Cruceta arriba: bailar (hasta moverse o actuar).
    if (act && in.pressed(kCrouch)) {
        commands.toggleCrouch = true;
        m_dancing = false;
    }
    if (act && !m_buildMode && !usingItem && in.pressed(kEmote)) {
        m_dancing = !m_dancing;
        m_danceSoundTimer = 0.0f;
        if (m_dancing && m_match.playerCrouched()) commands.toggleCrouch = true;  // se levanta para bailar
    }
    const float stick = sqrtf(in.analogX() * in.analogX() + in.analogY() * in.analogY());
    if (!act || m_buildMode || stick > 0.35f || in.held(kFire) || in.held(kAim) || in.pressed(kJump) ||
        in.pressed(kInteract) || in.pressed(kPickaxe) || dpad != 0) {
        m_dancing = false;
    }
    updateDanceSound(dt);
    // L: modo apuntado. Con un enemigo a la vista lo fija (y el stick mueve); sin el, el
    // stick gira la camara y sube o baja la mira en lugar de andar.
    const bool aimHeld = act && in.held(kAim);
    m_aimHeld = aimHeld;
    updateScopeTransition();
    const bool stickAims = aimHeld && !(m_lockTarget > 0);

    // 1) Entrada del jugador.
    game::PlayerInput playerInput;
    game::PlayerAim aim;
    aim.eye = cameraEye();  // con la mirilla, desde la cabeza
    aim.direction = m_followCamera.lookDirection();
    commands.buildMode = m_buildMode;
    if (control && !inventory) {
        if (!stickAims) {
            playerInput.moveX = in.analogX();
            playerInput.moveY = in.analogY();
        }
        playerInput.jumpPressed = in.pressed(kJump);
        playerInput.jumpHeld = in.held(kJump);
        playerInput.crouchHeld = in.held(kCrouch);  // truco de volar: bajar
        playerInput.aiming = aimHeld;
        if (m_buildMode) {
            // Una pieza por pulsacion; con la cuarta, una trampa donde apunta la mira.
            if (m_buildPiece == kTrapPiece) {
                commands.placeTrap = in.pressed(kFire);
            } else {
                commands.placeWall = in.pressed(kFire);
                commands.buildPiece = static_cast<game::PieceType>(m_buildPiece);
            }
            commands.cycleMaterial = in.pressed(kInteract);  // madera -> piedra -> metal
        } else {
            if (m_match.inventory().pickaxe() || m_match.inventory().activeWeapon()) {
                // Disparar, o golpes seguidos con el pico (no mientras se elige que soltar).
                aim.fire = !swapInput && in.held(kFire);
            } else {
                commands.useItem = !swapInput && in.pressed(kFire);  // consumible: R lo usa
            }
            // CUADRADO: abrir o recoger (lleve lo que lleve en la mano, tambien un curativo) y,
            // si no hay nada al alcance, recargar. Si el objeto no cabe, se elige que soltar.
            if (!swapInput && in.pressed(kInteract)) {
                const game::Interactable it = m_match.playerInteractable();
                if (it.kind == game::Interactable::Kind::Pickup && m_match.pickupRoom(it.index) <= 0 && !usingItem) {
                    m_swapOpen = true;
                    m_swapPickup = it.index;
                    m_swapCursor = m_match.inventory().selected();
                    playSfx(audio::Sfx::MenuMove, 0.6f);
                } else {
                    commands.reload = true;
                }
            }
        }
    }
    playerInput.cameraYaw = m_followCamera.yaw();
    m_match.setPlayerInput(playerInput, aim, commands);

    // 2) Simulacion en pasos fijos de 1/60 s, independiente de los FPS. Truco "tiempo bala":
    // todo avanza a la mitad.
    const int steps = m_timestep.advance(m_match.cheats().slowMotion ? dt * 0.5f : dt);
    for (int i = 0; i < steps; ++i) m_match.step(game::FixedTimestep::kStep);

    // 3) Sucesos de la partida -> sonido y reacciones de las animaciones.
    m_harvestAge += dt;
    processEvents();
    // La recarga se ha interrumpido (otra ranura, el pico, construir...): su sonido se corta.
    if (m_reloadSfx != audio::Sfx::Count) {
        const game::Weapon* w = m_match.inventory().activeWeapon();
        if (!w || !w->reloading()) {
            if (m_audioReady) audio::mixer().stop(m_reloadSfx);
            m_reloadSfx = audio::Sfx::Count;
        }
    }

    // 4) Animaciones (visuales, con el tiempo de frame). Los eliminados tambien: caen.
    {
        game::AnimInput a;
        a.alive = m_match.alive(0);
        a.aiming = m_match.playerAiming();
        a.aimPitch = a.aiming ? m_followCamera.pitch() + 0.065f : 0.0f;  // la mira en reposo baja 4 grados
        a.crouched = m_match.playerCrouched();
        if (const game::Weapon* w = m_match.inventory().activeWeapon()) {
            a.reloading = w->reloading();
            a.reloadProgress = w->reloadProgress();
        }
        a.usingItem = m_match.useProgress() >= 0.0f;
        a.useProgress = a.usingItem ? m_match.useProgress() : 0.0f;
        m_playerAnim.update(renderState(0), a, dt);
    }
    for (int i = 0; i < m_match.botCount(); ++i) {
        const game::Bot& b = m_match.bot(i);
        game::AnimInput a;
        a.alive = b.state != game::BotState::Eliminated;
        a.aiming = b.aiming;
        // Apuntan a la altura del pecho de su objetivo.
        if (b.aiming && b.target >= 0 && m_match.alive(b.target)) {
            const Vec3 d = m_match.chestOf(b.target) - m_match.eyeOf(i + 1);
            a.aimPitch = atan2f(d.y, sqrtf(d.x * d.x + d.z * d.z));
        }
        a.reloading = b.armed && b.weapon.reloading();
        a.reloadProgress = b.weapon.reloadProgress();
        a.usingItem = b.healTimer > 0.0f;
        // Los supervivientes que esperan en su campamento estan agachados junto a la hoguera.
        a.crouched = b.kind == game::BotKind::Survivor && !b.rescued;
        if (b.kind == game::BotKind::Zombie) a.aiming = false;
        m_botAnims[i].update(renderState(i + 1), a, dt);
    }
    playFootsteps();
    updateChestHum(dt);

    // 5) Camaras y construccion.
    updateGameCameras(in, dt, control, aimHeld);
    if (m_buildMode && m_buildPiece == kTrapPiece) {
        game::PlayerAim now;
        now.eye = cameraEye();
        now.direction = m_followCamera.lookDirection();
        m_trapPlan = m_match.planTrap(now);
    } else if (m_buildMode) {
        m_wallPlan = m_match.planWall(m_followCamera.yaw(), static_cast<game::PieceType>(m_buildPiece));
    }
    // Las piezas ya no se quitan con CUADRADO (cambia de material): se rompen con el pico.
    m_targetWall = -1;

    updateStormFog();
    updatePerformance(dt);
}

void App::updateStormFog() {
    // Lejos de la tormenta, la niebla normal. A menos de kStormFogRange metros se espesa y
    // tira a morado; dentro, en cuanto se entran unos metros, solo se ven ~13 m de
    // tormenta morada: nunca se llega a ver donde acaba el mundo.
    // Cuenta lo mas metido en la tormenta entre la camara y el personaje seguido: la
    // camara va 5 m por detras y, al entrar, la pantalla debe cerrarse ya.
    // La tormenta que se cierra tine de morado y acorta la vista, pero menos que la del
    // borde (alli no hay nada que ver mas alla).
    const Vec3 eye = cameraEye();
    float depth = world::stormDepth(eye.x, eye.z);
    float closing = m_match.storm().depth(eye.x, eye.z);
    if (!m_freeCameraActive) {
        const Vec3 p = renderState(spectateTarget()).position;
        const float followed = world::stormDepth(p.x, p.z);
        if (followed > depth) depth = followed;
        closing = fmaxf(closing, m_match.storm().depth(p.x, p.z));
    }
    const float closingT = math3d::clampf((closing + 2.0f) / 4.0f, 0.0f, 1.0f);
    const float nearT = math3d::clampf((depth + kStormFogRange) / kStormFogRange, 0.0f, 1.0f);
    const float inT = math3d::clampf(depth / 4.0f, 0.0f, 1.0f);
    // Desde el autobus y cayendo, la niebla se aleja con la altura (si no, a 90 m solo se
    // veria niebla); se dibuja mas lejos solo esos segundos.
    const float altitude = eye.y > 8.0f ? eye.y - 8.0f : 0.0f;
    auto lerp = [](float a, float b, float t) { return a + (b - a) * t; };
    auto lerpColor = [&](u32 a, u32 b, float t) {
        auto ch = [&](int shift) {
            return static_cast<u32>(lerp(static_cast<float>((a >> shift) & 0xFF), static_cast<float>((b >> shift) & 0xFF), t));
        };
        return rgba(ch(0), ch(8), ch(16));
    };
    // Modo zombi (v0.14): niebla gris verdosa de dia y casi negra de noche, mas cerrada.
    u32 baseColor = config::kColorSky;
    float baseStart = m_settings.fogStart(), baseEnd = m_settings.fogEnd();
    if (m_match.zombieMode()) updateZombieFog(&baseStart, &baseEnd, &baseColor);
    m_fogColor = lerpColor(lerpColor(lerpColor(baseColor, kStormFogColor, 0.45f * nearT), kStormFogColor,
                                     0.7f * closingT),
                           kStormInsideColor, inT);
    // La distancia de dibujado de los ajustes fija la niebla normal.
    m_fogStart = lerp(lerp(lerp(baseStart, 6.0f, nearT), 4.0f, closingT), 0.0f, inT) + altitude * 0.6f;
    const float edgeEnd = lerp(baseEnd, 42.0f, nearT), closingEnd = lerp(baseEnd, 38.0f, closingT);
    m_fogEnd = lerp(fminf(edgeEnd, closingEnd), 13.0f, inT) + altitude * 1.4f;
}

void App::updateGameCameras(const input::Input& in, float dt, bool control, bool aimHeld) {
    // L en combate: fijar el bot visible mas centrado (o recolocar si no hay ninguno).
    Vec3 lockPoint;
    const Vec3* lock = nullptr;
    // Mirilla: el zoom entra en ~0.15 s; sin fijar objetivo (con el francotirador se apunta
    // a mano) y con el stick mas lento, en proporcion al zoom.
    const bool scoped = sniperScoped();
    m_scopeZoom = math3d::clampf(m_scopeZoom + (scoped ? dt : -dt) * 7.0f, 0.0f, 1.0f);
    if (!scoped) m_scopeZoom = 0.0f;
    if (aimHeld && !m_buildMode && !scoped) {
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
    // L sin objetivo recoloca la camara detras del jugador, salvo con la mirilla: entonces
    // se sigue apuntando a donde estaba la mira (updateScopeTransition).
    const bool recenter = control && in.pressed(m_settings.button(Action::Aim)) && !lock && !scoped;
    if (m_freeCameraActive) {
        m_freeCamera.update(in, dt);
        return;
    }
    // Tras caer, la camara sigue a quien elimino al jugador (o a otro superviviente).
    const int target = spectateTarget();
    game::CharacterState followed = renderState(target);
    // En el autobus, la camara mira al centro entre el autobus y su globo.
    if (m_match.airState(target) == game::AirState::Bus) followed.position.y += 4.0f;
    camera::CameraInput cam;
    if (control && aimHeld && !lock) {
        // Modo apuntado sin objetivo: el stick mueve la mira.
        // Sensibilidad de los ajustes (camara y mirilla) e inversion del eje vertical.
        const float slow = scoped ? 0.3f * m_settings.scopeScale() : m_settings.cameraScale();
        cam.turn = in.analogX() * slow;
        cam.look = -in.analogY() * slow * (m_settings.invertY ? -1.0f : 1.0f);
    } else if (!control || !aimHeld) {
        // Al andar, la camara se coloca detras poco a poco (no al ir hacia atras, para
        // poder retroceder sin girar). Mirando a otro tras caer, tambien le sigue.
        const float ax = control ? in.analogX() : 0.0f, ay = control ? in.analogY() : 0.0f;
        const float magnitude = sqrtf(ax * ax + ay * ay);
        const bool backwards = ay > 0.6f * magnitude;
        cam.followYaw = followed.yaw;
        cam.follow = control ? (backwards ? 0.0f : magnitude) : (m_match.alive(0) ? 0.0f : 0.6f);
    }
    // Autobus y caida: camara mas lejos y mirando hacia abajo; en el autobus el stick la
    // hace girar alrededor de el.
    switch (m_match.airState(target)) {
        case game::AirState::Bus:
            cam.distance = 22.0f;
            cam.restLook = -0.12f;
            cam.follow = 0.4f;
            cam.followYaw = m_followCamera.yaw();
            cam.turn = control ? in.analogX() : 0.0f;
            cam.look = 0.0f;
            break;
        case game::AirState::Freefall:
            cam.distance = 8.0f;
            cam.restLook = -0.60f;
            cam.follow = fmaxf(cam.follow, 0.4f);
            break;
        case game::AirState::Glide:
            cam.distance = 6.5f;
            cam.restLook = -0.35f;
            cam.follow = fmaxf(cam.follow, 0.4f);
            break;
        case game::AirState::Ground:
            break;
    }
    m_followCamera.update(cam, followed.position, followed.yaw, dt, recenter, lock, &m_world.collision());
}

audio::Sfx App::shotSound(int weaponType) {
    const game::ItemType type = static_cast<game::ItemType>(weaponType);
    switch (type) {
        case game::ItemType::Sniper: return audio::Sfx::SniperShot;
        case game::ItemType::RocketLauncher: return audio::Sfx::RocketLaunch;
        case game::ItemType::GrenadeLauncher: return audio::Sfx::GrenadeLaunch;
        case game::ItemType::Revolver: return audio::Sfx::RevolverShot;     // grabaciones (v0.13)
        case game::ItemType::Shotgun: return audio::Sfx::PumpShotgunShot;  // disparo y corredera
        default: break;
    }
    switch (game::weaponClass(type)) {
        case game::WeaponClass::Shotgun: return audio::Sfx::ShotgunShot;
        case game::WeaponClass::Smg: return audio::Sfx::SmgShot;
        case game::WeaponClass::Sniper: return audio::Sfx::SniperShot;  // semiautomatico
        case game::WeaponClass::Pistol: return audio::Sfx::PistolShot;
        default: return audio::Sfx::Shot;  // fusiles (y bots con las reglas clasicas)
    }
}

audio::Sfx App::reloadSound(int weaponType) {
    // Cada familia con su recarga (v0.13); el lanzacohetes, la de su grabacion.
    const game::ItemType type = static_cast<game::ItemType>(weaponType);
    switch (type) {
        case game::ItemType::Shotgun: return audio::Sfx::ReloadShotgun;
        case game::ItemType::TacticalShotgun: return audio::Sfx::ReloadTactical;
        case game::ItemType::Revolver: return audio::Sfx::ReloadRevolver;
        case game::ItemType::RocketLauncher: return audio::Sfx::ReloadRocket;
        case game::ItemType::GrenadeLauncher: return audio::Sfx::ReloadGrenadeLauncher;
        default: break;
    }
    switch (game::weaponClass(type)) {
        case game::WeaponClass::Rifle: return audio::Sfx::ReloadRifle;
        case game::WeaponClass::Smg: return audio::Sfx::ReloadSmg;
        case game::WeaponClass::Pistol: return audio::Sfx::ReloadPistol;
        case game::WeaponClass::Sniper: return audio::Sfx::ReloadSniper;
        default: return audio::Sfx::Reload;
    }
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

void App::updateChestHum(float dt) {
    // Zumbido de un cofre sin abrir cercano (Sonidos Limpios/Fortnite Chest(sin abrir), 5 s
    // que se repiten mientras se esta cerca), como el aviso de Fortnite.
    m_airdropMessage -= dt;
    m_stormMessage -= dt;
    m_chestHumTimer -= dt;
    const Vec3 pos = renderState(0).position;
    const game::Container* nearest = nullptr;
    float best = kChestHumRange;
    if (m_results == Results::None && m_match.alive(0) && m_match.phase() != game::MatchPhase::Finished &&
        m_match.airState(0) == game::AirState::Ground) {
        for (const game::Container& c : m_match.containers()) {
            if (c.kind != game::ContainerKind::Chest || c.opened) continue;
            const float d = math3d::length(c.position - pos);
            if (d < best) {
                best = d;
                nearest = &c;
            }
        }
    }
    if (nearest) {
        if (m_chestHumTimer <= 0.0f) {
            playSpatial(audio::Sfx::ChestHum, nearest->position, 0.6f);
            m_chestHumTimer = 5.0f;
            m_chestHumOn = true;
        }
    } else if (m_chestHumOn) {
        if (m_audioReady) audio::mixer().stop(audio::Sfx::ChestHum);
        m_chestHumOn = false;
        m_chestHumTimer = 0.0f;
    }
}

void App::updateDanceSound(float dt) {
    // La musica del baile (7.6 s) se repite mientras se baila y se corta al parar.
    if (m_dancing) {
        m_danceSoundTimer -= dt;
        if (m_danceSoundTimer <= 0.0f) {
            playSfx(audio::Sfx::Dance, 0.8f);
            m_danceSoundTimer = 7.55f;
            m_danceSoundOn = true;
        }
    } else if (m_danceSoundOn) {
        if (m_audioReady) audio::mixer().stop(audio::Sfx::Dance);
        m_danceSoundOn = false;
        m_danceSoundTimer = 0.0f;
    }
}

void App::playFootsteps() {
    // Una pisada = un paso de Sonidos Limpios/Fortnite Running Sound Effect. Los del
    // jugador, siempre; los de los bots, solo cerca (avisan de que alguien se acerca).
    if (m_playerAnim.footsteps != m_playerSteps) {
        m_playerSteps = m_playerAnim.footsteps;
        // En el agua del lago, chapoteo (v0.14).
        if (m_match.body(0).state().wading) playSfx(audio::Sfx::Splash, 0.5f);
        else playSfx(audio::Sfx::Footstep, 0.45f);
    }
    for (int i = 0; i < m_match.botCount(); ++i) {
        if (m_botAnims[i].footsteps == m_botSteps[i]) continue;
        m_botSteps[i] = m_botAnims[i].footsteps;
        const Vec3 p = renderState(i + 1).position;
        if (math3d::length(p - renderState(0).position) < kFootstepRange) {
            const bool wading = m_match.body(i + 1).state().wading;
            playSpatial(wading ? audio::Sfx::Splash : audio::Sfx::Footstep, p, 0.22f);  // mas bajos que los propios (0,45)
        }
    }
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
            case game::GameEventType::Shot: {
                // Cada familia de armas con su sonido (other = tipo de arma).
                const audio::Sfx shot = shotSound(e.other);
                if (e.entity == 0) {
                    playSfx(shot, 0.85f);
                    m_playerAnim.onShot();
                    // Retroceso: la mira sube un poco y vuelve (la dispersion la lleva la partida).
                    m_followCamera.kick(math3d::toRadians(0.35f * m_match.playerWeapon().config().recoil));
                } else if (e.entity > 0) {
                    playSpatial(shot, e.position, 0.7f);
                    m_botAnims[e.entity - 1].onShot();
                }
                break;
            }
            case game::GameEventType::HitEntity:
                if (e.entity > 0) m_botAnims[e.entity - 1].onHit();
                if (e.other == 0 && e.entity > 0 && e.amount) addStat(Stat::Headshots, 1);
                if (e.other == 0) playSfx(audio::Sfx::HitMarker, e.amount ? 1.0f : 0.9f);
                if (e.other == 0 && e.amount) playSfx(audio::Sfx::HitMarker, 0.7f, 0.0f);  // a la cabeza: doble
                break;
            case game::GameEventType::HitStructure:
                if (e.other == 0) playSfx(audio::Sfx::HitMarker, 0.4f);
                break;
            case game::GameEventType::PlayerHurt:
                playSfx(audio::Sfx::Hurt, 0.9f);
                m_playerAnim.onHit();
                break;
            case game::GameEventType::Eliminated:
                if (e.entity == 0) {
                    m_playerKiller = e.other;
                    playSfx(audio::Sfx::PlayerDeath, 1.0f);
                }
                if (e.other == 0) playSfx(audio::Sfx::Elimination, 1.0f);
                if (e.entity > 0 && m_match.isSurvivor(e.entity) && m_match.bot(e.entity - 1).rescued) {
                    showZombieMessage(i18n::T::ZSurvivorDown, m_match.name(e.entity), 0, kErrorRed);
                }
                if (e.other == 0 && e.entity > 0 && m_match.isZombie(e.entity)) {
                    addStat(Stat::ZombieKills, 1);  // modo zombi: sus bajas no cuentan como las del battle royale
                } else if (e.other == 0 && e.entity > 0) {
                    // Logros: bajas, y con que (amount = tipo de objeto + 1).
                    const game::ItemType source = static_cast<game::ItemType>(e.amount - 1);
                    addStat(Stat::Kills, 1);
                    if (source == game::ItemType::SpikeTrap) addStat(Stat::TrapKills, 1);
                    if (source == game::ItemType::RocketLauncher || source == game::ItemType::GrenadeLauncher ||
                        source == game::ItemType::Grenade) {
                        addStat(Stat::ExplosiveKills, 1);
                    }
                    if (m_match.playerKills() > m_profile.mostKills) {
                        addStat(Stat::MostKills, m_match.playerKills() - m_profile.mostKills);
                    }
                }
                break;
            case game::GameEventType::WallBuilt:
                // De los demas solo se oyen disparos y pasos.
                if (e.entity == 0) playSfx(audio::Sfx::WallBuild, 0.8f);
                if (e.entity == 0) addStat(Stat::PiecesBuilt, 1);
                break;
            case game::GameEventType::BusJump:
                if (e.entity == 0) playSfx(audio::Sfx::BusJump, 0.9f);
                break;
            case game::GameEventType::GliderOpen:
                if (e.entity == 0) playSfx(audio::Sfx::GliderOpen, 0.9f);
                break;
            case game::GameEventType::Landed:
                if (e.entity == 0) playSfx(audio::Sfx::Footstep, 0.8f);
                break;
            case game::GameEventType::ContainerOpened:
                if (e.entity == 0 && m_audioReady) audio::mixer().stop(audio::Sfx::ChestHum);
                if (e.entity == 0) {
                    switch (static_cast<game::ContainerKind>(e.other)) {
                        case game::ContainerKind::Chest: addStat(Stat::ChestsOpened, 1); break;
                        case game::ContainerKind::Airdrop: addStat(Stat::AirdropsOpened, 1); break;
                        case game::ContainerKind::Llama: addStat(Stat::LlamasOpened, 1); break;
                        case game::ContainerKind::AmmoBox:
                        case game::ContainerKind::Workbench: break;
                    }
                }
                // Cada contenedor con su sonido: la caja de municion ya no usa el del cofre.
                if (e.entity == 0 || e.other == static_cast<int>(game::ContainerKind::Llama)) {
                    switch (static_cast<game::ContainerKind>(e.other)) {
                        case game::ContainerKind::Chest:
                        case game::ContainerKind::Airdrop: playSfx(audio::Sfx::ChestOpen, 0.9f); break;
                        case game::ContainerKind::AmmoBox: playSfx(audio::Sfx::AmmoBoxOpen, 0.9f); break;
                        case game::ContainerKind::Llama: playSpatial(audio::Sfx::LlamaOpen, e.position, 1.0f); break;
                        case game::ContainerKind::Workbench: break;
                    }
                }
                break;
            case game::GameEventType::PickedUp:
                if (e.entity == 0) playSfx(audio::Sfx::Pickup, 0.8f);
                break;
            case game::GameEventType::AirdropIncoming:
                m_airdropMessage = 5.0f;
                break;
            case game::GameEventType::AirdropLanded:
            case game::GameEventType::LlamaSpawned:
                break;
            case game::GameEventType::Explosion:
                playSpatial(audio::Sfx::Explosion, e.position, 1.0f);
                break;
            case game::GameEventType::TrapPlaced:
                if (e.entity == 0) playSfx(audio::Sfx::WallBuild, 0.8f);
                break;
            case game::GameEventType::TrapTriggered:
                playSpatial(audio::Sfx::TrapTrigger, e.position, 1.0f);
                break;
            case game::GameEventType::TrapRemoved:
                break;
            case game::GameEventType::Thrown:
                if (e.entity == 0) {
                    playSfx(audio::Sfx::GrenadeThrow, 0.8f);
                    m_playerAnim.onSwing();
                }
                break;
            case game::GameEventType::WallRemoved:
                playSfx(audio::Sfx::WallBuild, 0.5f);
                break;
            case game::GameEventType::ObjectDestroyed:
                if (e.entity == 0) playSfx(audio::Sfx::WallBreak, 0.9f);  // lo ha destruido el jugador
                break;
            case game::GameEventType::WallBroken:
                if (e.other == 0) playSpatial(audio::Sfx::WallBreak, e.position, 0.9f);  // la rompe el jugador
                break;
            case game::GameEventType::Reload:
                if (m_audioReady && m_reloadSfx != audio::Sfx::Count) audio::mixer().stop(m_reloadSfx);
                m_reloadSfx = reloadSound(e.other);
                playSfx(m_reloadSfx, 0.85f);
                break;
            case game::GameEventType::StormEnter:
                if (e.entity == 0) playSfx(audio::Sfx::StormWarning, 1.0f);
                break;
            case game::GameEventType::StormClosing:
                playSfx(audio::Sfx::StormWarning, 1.0f);
                m_stormMessage = 4.0f;
                break;
            case game::GameEventType::PickaxeSwing:
                if (e.entity > 0) {
                    // Un bot picando (v0.13): se oye cerca.
                    m_botAnims[e.entity - 1].onSwing();
                    playSpatial(audio::Sfx::Pickaxe, e.position, 0.55f);
                    break;
                }
                m_playerAnim.onSwing();
                if (e.other == 1) playSfx(audio::Sfx::Pickaxe, 0.9f);
                break;
            case game::GameEventType::Harvest:
                m_harvestResource = e.other;
                m_harvestAmount = e.amount;
                m_harvestAge = 0.0f;
                if (e.amount > 0) addStat(Stat::MaterialsHarvested, e.amount);
                break;
            case game::GameEventType::ItemUsed:
                if (e.entity == 0) addStat(Stat::ItemsUsed, 1);
                // Las pociones, las vendas y el botiquin ya suenan mientras se usan (Sonidos
                // Limpios): al terminar se corta lo que quede y suena la cura.
                if (m_audioReady && m_useSfx != audio::Sfx::Count && m_useSfx != audio::Sfx::DrinkSmallShield &&
                    m_useSfx != audio::Sfx::DrinkShieldPotion) {
                    audio::mixer().stop(m_useSfx);
                }
                m_useSfx = audio::Sfx::Count;
                if (e.other == static_cast<int>(game::ItemType::Bandage) ||
                    e.other == static_cast<int>(game::ItemType::Medkit)) {
                    playSfx(audio::Sfx::Heal, 0.6f);
                }
                break;
            default:
                processZombieEvent(e);
                break;
            case game::GameEventType::UseStarted:
            case game::GameEventType::UseCanceled: {
                const game::ItemType item = static_cast<game::ItemType>(e.other);
                const audio::Sfx use = item == game::ItemType::SmallShield    ? audio::Sfx::DrinkSmallShield
                                       : item == game::ItemType::ShieldPotion ? audio::Sfx::DrinkShieldPotion
                                       : item == game::ItemType::Medkit       ? audio::Sfx::MedkitUse
                                       : item == game::ItemType::Bandage      ? audio::Sfx::BandageUse
                                                                              : audio::Sfx::Count;
                if (use == audio::Sfx::Count || !m_audioReady) break;
                if (e.type == game::GameEventType::UseStarted) {
                    playSfx(use, 0.9f);
                    m_useSfx = use;
                } else {
                    audio::mixer().stop(use);
                    m_useSfx = audio::Sfx::Count;
                }
                break;
            }
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
    return m_screen == Screen::Game ? (m_fog ? m_fogColor : config::kColorSky) : kNavy;
}

math3d::Vec3 App::cameraEye() const {
    if (m_freeCameraActive) return m_freeCamera.eye();
    if (sniperScoped()) return scopeEye();
    return m_followCamera.eye();
}

Vec3 App::aimPointFrom(const Vec3& eye, const Vec3& dir) const {
    const game::Weapon* w = m_match.inventory().activeWeapon();
    return m_match.aimPoint(eye, dir, w ? w->config().range : 200.0f);
}

void App::updateScopeTransition() {
    const bool scoped = sniperScoped();
    if (scoped == m_wasScoped) return;
    m_wasScoped = scoped;
    if (scoped) {
        // Lo que estaba bajo la mira en tercera persona queda en el centro de la mirilla:
        // desde la cabeza se apunta a ese mismo punto (el ojo de la mirilla esta 0.35 m
        // delante de la cara, segun la direccion: dos iteraciones).
        const Vec3 target = aimPointFrom(m_followCamera.eye(), m_followCamera.lookDirection());
        for (int k = 0; k < 2; ++k) {
            const Vec3 d = target - scopeEye();
            const float flat = sqrtf(d.x * d.x + d.z * d.z);
            if (flat < 1.0f) break;
            m_followCamera.setAim(atan2f(-d.x, -d.z), atan2f(d.y, flat));
        }
    } else {
        // Al quitarla, la mira de tercera persona vuelve a senalar lo que se veia.
        m_followCamera.aimAt(aimPointFrom(scopeEye(), m_followCamera.lookDirection()), &m_world.collision());
    }
}

bool App::sniperScoped() const {
    // L con un arma con mirilla en la mano (francotiradores, fusil con mira), a pie.
    if (!m_aimHeld || m_buildMode || m_freeCameraActive || !m_match.alive(0) ||
        m_match.airState(0) != game::AirState::Ground || m_results != Results::None) {
        return false;
    }
    const game::Inventory& inv = m_match.inventory();
    return !inv.pickaxe() && game::hasScope(inv.slot(inv.selected()).type);
}

float App::scopeFov() const {
    const game::Weapon* w = m_match.inventory().activeWeapon();
    return w && w->config().scopeFov > 0.0f ? w->config().scopeFov : kScopeFovDegrees;
}

Vec3 App::scopeEye() const {
    // Primera persona: a la altura de los ojos, un poco por delante de la cara.
    return renderState(0).position + Vec3{0.0f, 1.62f, 0.0f} + m_followCamera.lookDirection() * 0.35f;
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
        case Screen::Language: renderLanguage(); break;
        case Screen::Login: renderLogin(); break;
        case Screen::Nickname: renderNickname(); break;
        case Screen::Lobby: renderLobby(); break;
        case Screen::Controls: renderControlsScreen(frame); break;
        case Screen::Loading: renderLoading(); break;
        case Screen::Game:
            renderGame(frame);
            if (m_results != Results::None) renderResults();
            if (m_paused) renderPause();
            break;
    }
    if ((m_screen == Screen::Game || m_screen == Screen::Lobby) && !m_settingsOpen) renderAchievementToast();
    if (m_exitProgress > 0.0f) renderExitBar(140, 4, 200, 4);
}

void App::renderOverlay(const FrameInfo& frame) const {
    switch (m_screen) {
        case Screen::Language: overlayLanguage(); break;
        case Screen::Login: overlayLogin(); break;
        case Screen::Nickname: overlayNickname(); break;
        case Screen::Lobby: overlayLobby(); break;
        case Screen::Controls: overlayControlsScreen(frame); break;
        case Screen::Loading: overlayLoading(); break;
        case Screen::Game:
            // En pausa solo su texto (el del HUD atravesaria el panel: va por CPU encima).
            if (m_results != Results::None) {
                overlayResults();
            } else if (!m_paused) {
                overlayGame(frame);
            }
            if (m_paused) overlayPause();
            break;
    }
    if ((m_screen == Screen::Game || m_screen == Screen::Lobby) && !m_settingsOpen) overlayAchievementToast();
}

void App::renderExitBar(float x, float y, float w, float h) const {
    gfx::fillRect(x, y, w, h, kButtonOff);
    gfx::fillRect(x, y, w * m_exitProgress, h, rgba(220, 60, 50));
}

// --- Partida: dibujo 3D ---------------------------------------------------------------------

void App::renderGame(const FrameInfo& frame) {
    if (m_worldReady) {
        // Con la mirilla: primera persona y el campo de vision cerrado (zoom x4 aprox.).
        camera::Projection lens = m_projection;
        const bool scoped = sniperScoped();
        if (scoped) lens.fovYDegrees = m_projection.fovYDegrees + (scopeFov() - m_projection.fovYDegrees) * m_scopeZoom;
        const math3d::Mat4 projection = lens.matrix();
        const Vec3 scopeFrom = scopeEye();
        const math3d::Mat4 view =
            m_freeCameraActive ? m_freeCamera.viewMatrix()
            : scoped ? math3d::Mat4::lookAt(scopeFrom, scopeFrom + m_followCamera.lookDirection(), {0.0f, 1.0f, 0.0f})
                     : m_followCamera.viewMatrix();
        // Niebla: la normal o, cerca de la tormenta del borde, mas densa y morada.
        gfx::FogSettings fog;
        fog.enabled = m_fog;
        fog.start = m_fogStart;
        fog.end = m_fogEnd;
        fog.color = m_fogColor;  // == clearColor()

        gfx::begin3D(projection, view, fog);
        renderSky(view);  // v0.14: cielo con degradado, nubes y sol (lo primero, sin niebla)
        const math3d::Frustum frustum = math3d::Frustum::fromMatrix(projection * view);
        const float drawDistance = m_fog ? m_fogEnd : config::kFarPlane;
        m_stats = m_world.draw(frustum, cameraEye(), drawDistance, m_match.destroyedObjects());
        if (m_buildVisualsReady) m_buildVisuals.drawWalls(m_match.structures(), m_world.textures());
        m_characterTriangles = m_modelsReady ? renderCharacters(frustum, cameraEye()) : 0;
        renderTracers();
        // Lo semitransparente al final, para no tapar lo que hay detras.
        if (m_propsReady) {
            // El autobus (si alguien sigue dentro o se ve desde abajo) y el botin cercano.
            m_props.drawBus(m_match.busPosition(), m_world.textures());
            m_props.drawLoot(m_match, frustum, cameraEye(), drawDistance, m_match.elapsed(), m_world.textures(), m_items);
            m_props.drawProjectiles(m_match, frustum, m_items);
            m_props.drawTraps(m_match, frustum, cameraEye(), drawDistance);
            m_props.drawBeams(m_match, frustum, cameraEye(), m_match.elapsed());
            m_props.drawExplosions(m_match, frustum);
        }
        // Modo zombi: piezas fantasma de los planos encontrados (azules; la que se construiria
        // ahora con CUADRADO, parpadeando).
        if (m_buildVisualsReady && m_match.zombieMode()) {
            const game::Interactable it = m_match.playerInteractable();
            const int current = it.kind == game::Interactable::Kind::BlueprintPiece ? it.index : -1;
            for (int bp = 0; bp < game::kBlueprintCount; ++bp) {
                if (!m_match.blueprintFound(bp)) continue;
                for (int k = 0; k < game::blueprint(bp).pieceCount; ++k) {
                    if (m_match.pieceBuilt(bp, k)) continue;
                    if (bp * game::kMaxBlueprintPieces + k == current && fmodf(m_blinkTime, 0.6f) < 0.2f) continue;
                    const game::WallPlacement ghost = m_match.blueprintPiece(bp, k);
                    const Vec3 c = (ghost.box.min + ghost.box.max) * 0.5f;
                    if (math3d::length(c - cameraEye()) > drawDistance || !frustum.sphereVisible(c, 3.0f)) continue;
                    m_buildVisuals.drawGhost(ghost);
                }
            }
        }
        m_world.drawStormWall(frustum, cameraEye(), drawDistance);
        // La que se cierra, mientras no abarque ya todo el mapa.
        if (m_match.zoneRadius() < 124.0f) {
            m_world.drawStormCircle(frustum, cameraEye(), drawDistance, m_match.zoneCenter(), m_match.zoneRadius());
        }
        if (m_buildVisualsReady && m_buildMode && !m_freeCameraActive) {
            if (m_targetWall >= 0) m_buildVisuals.drawHighlight(m_match.structures().wall(m_targetWall));
            if (m_buildPiece == kTrapPiece) {
                if (m_propsReady) m_props.drawTrapGhost(m_trapPlan);
            } else {
                m_buildVisuals.drawGhost(m_wallPlan);
            }
        }
        m_viewProjection = projection * view;
        gfx::begin2D();
        // Modo zombi: penumbra, oscuridad de noche y vineta (antes del HUD).
        if (m_match.zombieMode()) renderNightOverlay();
    }

    switch (m_hud) {
        case Hud::Game:
            if (m_results == Results::None) renderGameHud();
            break;
        case Hud::Debug:
            renderDebugPage();
            break;
        case Hud::Controls:
            renderControlsPage(frame);
            break;
        case Hud::Hidden:
        case Hud::Count:
            break;
    }
}

int App::renderCharacters(const math3d::Frustum& frustum, const Vec3& eye) const {
    int triangles = 0;
    const float maxDistance = m_fog ? m_fogEnd : config::kFarPlane;
    auto visible = [&](const Vec3& p) {
        return math3d::length(p - eye) < maxDistance + 2.0f &&
               frustum.sphereVisible(p + Vec3{0.0f, 0.9f, 0.0f}, 1.3f);
    };
    // Postura segun la caida (en el autobus no se ve a nadie) y el ala delta encima.
    auto pose = [&](int id) {
        switch (m_match.airState(id)) {
            case game::AirState::Freefall: return game::BodyPose::Skydive;
            case game::AirState::Glide: return game::BodyPose::Glide;
            default:
                if (id == 0 && m_dancing) return game::BodyPose::Dance;
                if (id == 0 && m_match.playerCrouched()) return game::BodyPose::Crouch;
                // Modo zombi: los zombis encorvados; los supervivientes que esperan, agachados.
                if (m_match.isZombie(id)) return game::BodyPose::Zombie;
                if (m_match.isSurvivor(id) && !m_match.bot(id - 1).rescued) return game::BodyPose::Crouch;
                // Truco "fiesta": los bots vivos bailan.
                if (id > 0 && m_match.cheats().partyBots && m_match.alive(id)) return game::BodyPose::Dance;
                return game::BodyPose::Normal;
        }
    };
    const game::CharacterState player = renderState(0);
    if (m_match.airState(0) != game::AirState::Bus && !sniperScoped() && visible(player.position)) {
        // En la mano: el pico, el arma o nada (consumibles y ranuras vacias).
        const game::Inventory& inv = m_match.inventory();
        const game::InventorySlot& slot = inv.slot(inv.selected());
        const game::ItemModel held = m_dancing        ? game::ItemModel::None
                                     : inv.pickaxe() ? game::ItemModel::Pickaxe
                                                     : game::itemModelFor(slot.type, slot.rarity);
        m_models[0].draw(player, m_playerAnim, m_world.textures(), !m_match.alive(0), held, false, pose(0), &m_items);
        if (m_propsReady && m_match.airState(0) == game::AirState::Glide) m_props.drawGlider(player.position, player.yaw);
        triangles += m_models[0].triangles() + m_items.triangles(held);
    }
    for (int i = 0; i < m_match.botCount(); ++i) {
        const game::Bot& b = m_match.bot(i);
        const int id = i + 1;
        if (m_match.airState(id) == game::AirState::Bus) continue;
        const bool eliminated = b.state == game::BotState::Eliminated;
        if (eliminated && m_match.elapsed() - b.eliminatedAt > kCorpseSeconds) continue;
        const game::CharacterState s = renderState(id);
        if (!visible(s.position)) continue;
        // Mas alla de la distancia de detalle de los ajustes, la version de pocos poligonos.
        const bool far = math3d::length(s.position - eye) > m_settings.lodDistance();
        const bool zombie = b.kind == game::BotKind::Zombie;
        const game::CharacterModel& model = zombie ? m_models[4 + b.palette % 2] : m_models[1 + b.palette % 3];
        // Los bots armados sin tipo (reglas clasicas) llevan el fusil.
        const bool partying = m_match.cheats().partyBots && !eliminated;
        const game::ItemModel held = partying || zombie || (b.kind == game::BotKind::Survivor && !b.rescued)
                                         ? game::ItemModel::None
                                     : b.healTimer > 0.0f && b.healSlot >= 0 ? game::itemModelFor(b.bag.slot(b.healSlot).type)
                                     : !b.armed || b.state == game::BotState::Harvest ? game::ItemModel::Pickaxe
                                     : b.weaponType == game::ItemType::None ? game::ItemModel::AssaultRifle
                                                                            : game::itemModelFor(b.weaponType, b.weaponRarity);
        model.draw(s, m_botAnims[i], m_world.textures(), eliminated, held, far, pose(id), &m_items, m_match.entityScale(id));
        if (m_propsReady && m_match.airState(id) == game::AirState::Glide) m_props.drawGlider(s.position, s.yaw);
        triangles += (far ? model.lowDetailTriangles() : model.triangles()) + m_items.triangles(held, far);
    }
    return triangles;
}

void App::renderTracers() const {
    if (!m_settings.tracers) return;
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
