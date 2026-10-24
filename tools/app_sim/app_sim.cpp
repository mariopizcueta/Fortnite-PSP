// Simulacion de host (Mac) de la aplicacion COMPLETA: el mismo app::App que el EBOOT, con
// el mando, el reloj y el audio simulados y el dibujo por el rasterizador por software
// (tools/host/soft_gfx.cpp). Recorre el flujo de la fase 6 con un guion de botones:
//
//   inicio de sesion -> lobby (Jugar / Controles / Salir) -> carga -> partida -> derrota
//   -> seguir mirando -> fin -> otra partida -> victoria -> lobby -> varias partidas mas
//   (reinicios, vuelta al lobby desde la pausa) -> salir
//
// y comprueba cada transicion, que suenan efectos y musica, que la memoria no crece
// entre partidas y que no se pierden sonidos. Guarda capturas (con el color reducido a
// 16 bits como el framebuffer) y el audio de los primeros segundos en build/app_sim/.
//
// Es una simulacion: no sustituye a probar en PPSSPP o en una PSP (no hay GE, ni VSync,
// ni hilo de audio reales).
//
// Uso: sh tools/app_sim/build_and_run.sh   (o: make app-sim). Sale con 1 si algo falla.
#include <malloc.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <vector>

#include <pspaudiolib.h>
#include <pspctrl.h>
#include <psppower.h>
#include <pspthreadman.h>

#include "app/app.h"
#include "assets/embedded_assets.h"
#include "audio/audio_system.h"
#include "audio/sound_data.h"
#include "core/config.h"
#include "gfx/debug_text.h"
#include "gfx/graphics.h"
#include "platform/log.h"
#include "game/fixed_timestep.h"
#include "soft_gfx.h"
#include "world/storm.h"

// --- Plataforma simulada --------------------------------------------------------------------

namespace {

u64 g_clock = 1000000;  // us
SceCtrlData g_pad{0, 0, 128, 128, {0, 0, 0, 0, 0, 0}};
pspAudioCallback_t g_audioCallback = nullptr;
FILE* g_logFile = nullptr;

}  // namespace

long long sceKernelGetSystemTimeWide(void) { return static_cast<long long>(g_clock); }
unsigned int sceKernelGetSystemTimeLow(void) { return static_cast<unsigned int>(g_clock); }
int sceCtrlSetSamplingCycle(int) { return 0; }
int sceCtrlSetSamplingMode(int) { return 0; }
int sceCtrlPeekBufferPositive(SceCtrlData* pad, int) {
    *pad = g_pad;
    return 1;
}
int scePowerGetCpuClockFrequencyInt(void) { return config::kCpuClockMhz; }
int scePowerGetBusClockFrequencyInt(void) { return config::kBusClockMhz; }
int pspAudioInit(void) { return 0; }
void pspAudioEndPre(void) {}
void pspAudioEnd(void) {}
void pspAudioSetChannelCallback(int, pspAudioCallback_t callback, void*) { g_audioCallback = callback; }

namespace platform {
namespace log {
void open(const char*) {}
void write(const char* fmt, ...) {
    if (!g_logFile) return;
    va_list args;
    va_start(args, fmt);
    fprintf(g_logFile, "[%9.3f] ", static_cast<double>(g_clock) / 1e6);
    vfprintf(g_logFile, fmt, args);
    fputc('\n', g_logFile);
    va_end(args);
}
void close() {}
}  // namespace log
}  // namespace platform

// --- Arnes ----------------------------------------------------------------------------------

namespace {

using math3d::Vec3;

// Mismos valores que App::Screen / App::Results.
enum Screen { kLogin, kNickname, kLobby, kControls, kLoading, kGame };
enum Results { kNone, kVictory, kDefeat };

int g_failures = 0;

void check(bool ok, const char* what) {
    printf("  [%s] %s\n", ok ? " OK " : "FALLO", what);
    if (!ok) ++g_failures;
}

unsigned heapBytes() { return static_cast<unsigned>(mallinfo().uordblks); }

class Sim {
public:
    explicit Sim(const char* outDir) : m_outDir(outDir) {}

    void init() {
        gfx::init();
        gfx::text::init();
        m_input.init();
        m_input.setTrackedCombo(config::kExitCombo);
        softgfx::g.enabled = true;
        m_app.setDataDirectory(m_outDir);  // perfil.txt en build/app_sim/
        m_app.init();
        m_timer.reset();
    }

    // Un frame de 1/60 s con los botones dados mantenidos.
    void frame(u32 buttons, float stickX = 0.0f, float stickY = 0.0f) {
        g_pad.Buttons = buttons;
        g_pad.Lx = static_cast<unsigned char>(127.5f + stickX * 127.0f);
        g_pad.Ly = static_cast<unsigned char>(127.5f + stickY * 127.0f);
        g_clock += 16667;
        m_timer.tick();
        m_input.update(m_timer.deltaSeconds());
        m_app.update(m_input, m_timer.deltaSeconds());

        softgfx::g.enabled = m_capture;
        const app::FrameInfo info{m_timer, m_input, 8.0f};
        gfx::beginFrame(m_app.clearColor());
        m_app.render(info);
        gfx::endFrame();
        gfx::text::setTarget(gfx::cpuDrawBuffer());
        m_app.renderOverlay(info);
        pullAudio();
        ++m_frames;
    }

    void press(u32 button) {
        frame(button);
        frame(0);
    }
    // Pulsar un boton con otro mantenido (p. ej. L + SELECT).
    void pressWith(u32 button, u32 held) {
        frame(held);
        frame(held | button);
        frame(held);
        frame(0);
    }
    void hold(u32 buttons, float seconds, float stickX = 0.0f, float stickY = 0.0f) {
        for (int i = 0; i < static_cast<int>(seconds * 60.0f + 0.5f); ++i) frame(buttons, stickX, stickY);
    }
    void wait(float seconds) { hold(0, seconds); }
    // Espera hasta que se cumpla la condicion (como mucho `maxSeconds`).
    template <typename F>
    bool waitUntil(F condition, float maxSeconds) {
        for (int i = 0; i < static_cast<int>(maxSeconds * 60.0f); ++i) {
            if (condition()) return true;
            frame(0);
        }
        return condition();
    }

    void shot(const char* name, u32 buttons = 0) {
        m_capture = true;
        frame(buttons);
        m_capture = false;
        char path[512];
        snprintf(path, sizeof(path), "%s/%s.png", m_outDir, name);
        softgfx::savePng(path, gfx::kRetro16BitFramebuffer);
        printf("  captura %s.png\n", name);
    }

    // Pico de audio desde la ultima llamada (0..32767).
    int takeAudioPeak() {
        const int p = m_peak;
        m_peak = 0;
        return p;
    }

    void writeAudio() const {
        char path[512];
        snprintf(path, sizeof(path), "%s/audio.wav", m_outDir);
        FILE* f = fopen(path, "wb");
        if (!f) return;
        const unsigned dataBytes = static_cast<unsigned>(m_recording.size() * 2);
        auto u32le = [&](unsigned v) { fwrite(&v, 4, 1, f); };
        auto u16le = [&](unsigned short v) { fwrite(&v, 2, 1, f); };
        fwrite("RIFF", 1, 4, f);
        u32le(36 + dataBytes);
        fwrite("WAVEfmt ", 1, 8, f);
        u32le(16);
        u16le(1);
        u16le(2);
        u32le(audio::Mixer::kOutputRate);
        u32le(audio::Mixer::kOutputRate * 4);
        u16le(4);
        u16le(16);
        fwrite("data", 1, 4, f);
        u32le(dataBytes);
        fwrite(m_recording.data(), 2, m_recording.size(), f);
        fclose(f);
        printf("  audio de la simulacion: %s (%.0f s)\n", path,
               static_cast<double>(m_recording.size()) / 2.0 / audio::Mixer::kOutputRate);
    }

    app::App& app() { return m_app; }
    int screen() const { return m_app.testScreen(); }
    int results() const { return m_app.testResults(); }
    float seconds() const { return m_frames / 60.0f; }

private:
    void pullAudio() {
        if (!g_audioCallback) return;
        // 44100 / 60 = 735 muestras estereo por frame.
        s16 buffer[735 * 2];
        g_audioCallback(buffer, 735, nullptr);
        for (int i = 0; i < 735 * 2; ++i) {
            const int v = buffer[i] < 0 ? -buffer[i] : buffer[i];
            if (v > m_peak) m_peak = v;
        }
        if (m_recording.size() < static_cast<size_t>(audio::Mixer::kOutputRate) * 2 * 60) {
            m_recording.insert(m_recording.end(), buffer, buffer + 735 * 2);
        }
    }

    const char* m_outDir;
    app::App m_app;
    input::Input m_input;
    core::FrameTimer m_timer;
    bool m_capture = false;
    int m_frames = 0;
    int m_peak = 0;
    std::vector<s16> m_recording;
};

// Salta del autobus, cae hasta el suelo y recibe el equipo de las fases anteriores (SCAR,
// vendas, minipociones, municion y madera) en la aparicion de siempre: asi las pruebas
// de armas, construccion y curas no dependen del botin que haya cerca.
bool landAndEquip(Sim& sim) {
    game::Match& match = sim.app().testMatch();
    sim.press(PSP_CTRL_CROSS);
    const bool landed = sim.waitUntil([&] { return match.airState(0) == game::AirState::Ground; }, 25.0f);
    match.giveItem(game::ItemType::AssaultRifle, 1, game::Rarity::Legendary);
    match.giveItem(game::ItemType::Bandage, 5);
    match.giveItem(game::ItemType::SmallShield, 3, game::Rarity::Uncommon);
    match.giveAmmo(game::AmmoType::Medium, 120);
    match.giveMaterials(world::Resource::Wood, 30 - match.materials().get(world::Resource::Wood));
    match.teleport(0, {0.0f, 0.0f, 24.0f}, 0.0f);
    // La SCAR en la mano (se guarda el pico con TRIANGULO y se busca con la cruceta).
    if (match.inventory().pickaxe()) sim.press(PSP_CTRL_TRIANGLE);
    for (int k = 0; k < game::Inventory::kSlots && match.inventory().activeType() != game::ItemType::AssaultRifle; ++k) {
        sim.press(PSP_CTRL_RIGHT);
    }
    sim.wait(0.5f);
    return landed && match.inventory().activeType() == game::ItemType::AssaultRifle;
}

void killAllBots(Sim& sim) {
    game::Match& m = sim.app().testMatch();
    for (int i = 1; i < m.entityCount(); ++i) {
        if (!m.alive(i)) continue;
        m.applyDamage(i, 1000.0f, 0);
        sim.frame(0);
    }
}

// --- 0) Audio: formatos, decodificador IMA y mezclador -------------------------------------

std::vector<unsigned char> readFile(const char* path) {
    std::vector<unsigned char> data;
    FILE* f = fopen(path, "rb");
    if (!f) return data;
    fseek(f, 0, SEEK_END);
    data.resize(static_cast<size_t>(ftell(f)));
    fseek(f, 0, SEEK_SET);
    if (fread(data.data(), 1, data.size(), f) != data.size()) data.clear();
    fclose(f);
    return data;
}

void testAudio(const char* outDir) {
    printf("\n0) Audio\n");
    const assets::Blob clips[] = {assets::sfxDisparo(), assets::sfxImpacto(), assets::sfxHerido(),
                                  assets::sfxEliminacion(), assets::sfxPared(), assets::sfxParedRota(),
                                  assets::sfxRecarga(), assets::sfxMenuMover(), assets::sfxMenuAceptar(),
                                  assets::sfxVictoria(), assets::sfxDerrota(), assets::sfxPico(),
                                  assets::sfxCuracion(), assets::sfxMuerteJugador(), assets::sfxMiniescudo(),
                                  assets::sfxPocionEscudo(), assets::sfxPaso(), assets::sfxAvisoTormenta(),
                                  assets::sfxCofreAbrir(), assets::sfxCofreCerca(), assets::sfxSaltoAutobus(), assets::sfxAlaDelta(), assets::sfxFrancotirador(), assets::sfxEscopeta(), assets::sfxSubfusil(), assets::sfxRecoger()};
    bool clipsOk = true;
    int clipBytes = 0;
    for (const assets::Blob& b : clips) {
        audio::SoundClip clip;
        clipsOk &= audio::parseWav(b.data, b.size, &clip) && clip.rate == 22050 && clip.count > 0;
        clipBytes += static_cast<int>(b.size);
    }
    printf("  26 efectos, %d KB en total\n", clipBytes / 1024);
    check(clipsOk, "los 26 efectos son WAV PCM 16 bits mono a 22050 Hz y se leen sin copiarlos");

    // Musica: el decodificador frente a la conversion de afconvert (la misma de prepare_assets).
    static audio::ImaStream music;
    const assets::Blob blob = assets::musicMenu();
    check(music.open(blob.data, blob.size) && music.rate() == 22050, "la musica IMA ADPCM se abre (22050 Hz)");
    char path[512];
    snprintf(path, sizeof(path), "%s/music_ref.wav", outDir);
    const std::vector<unsigned char> refFile = readFile(path);
    audio::SoundClip ref;
    if (!refFile.empty() && audio::parseWav(refFile.data(), refFile.size(), &ref)) {
        // Chasquido = salto brusco del error entre dos muestras seguidas. En los cambios de
        // bloque el decodificador se reinicia: ahi el salto no debe ser mayor que en el resto.
        double signal = 0.0, noise = 0.0;
        int jumpAtBoundary = 0, jumpElsewhere = 0, boundaryError = 0;
        int previousError = 0;
        for (int i = 0; i < ref.count && i < music.totalSamples(); ++i) {
            const int e = music.next() - ref.samples[i];
            signal += static_cast<double>(ref.samples[i]) * ref.samples[i];
            noise += static_cast<double>(e) * e;
            const int jump = abs(e - previousError);
            const bool boundary = i % 2041 == 0;
            if (boundary && abs(e) > boundaryError) boundaryError = abs(e);
            int& worst = boundary ? jumpAtBoundary : jumpElsewhere;
            if (i > 0 && jump > worst) worst = jump;
            previousError = e;
        }
        const double snr = 10.0 * log10(signal / (noise > 0.0 ? noise : 1.0));
        printf("  musica: %d muestras (%.1f s), referencia %d; SNR %.1f dB; salto maximo del error: %d en "
               "cambios de bloque, %d en el resto\n",
               music.totalSamples(), music.totalSamples() / 22050.0, ref.count, snr, jumpAtBoundary, jumpElsewhere);
        check(music.totalSamples() == ref.count, "la musica decodificada dura lo mismo que la original");
        check(snr > 25.0, "el audio decodificado se parece al original (SNR > 25 dB)");
        check(boundaryError == 0 && jumpAtBoundary <= jumpElsewhere,
              "cada bloque empieza en la muestra exacta y sin chasquidos al cambiar de bloque");
    } else {
        printf("  (sin %s: no se compara con la original)\n", path);
    }
    music.rewind();
    const s16 first = music.next();
    music.rewind();
    for (int i = 0; i < music.totalSamples(); ++i) music.next();
    check(music.next() == first, "al acabar, la musica vuelve a empezar (bucle)");

    // Mezclador: panorama, limite de voces y cola llena.
    static s16 tone[2000];
    for (int i = 0; i < 2000; ++i) tone[i] = static_cast<s16>(i % 40 < 20 ? 8000 : -8000);
    audio::Mixer mixer;
    audio::SoundClip clip;
    clip.samples = tone;
    clip.count = 2000;
    clip.rate = 22050;
    mixer.setClip(audio::Sfx::Shot, clip);
    s16 out[1024 * 2];
    mixer.play(audio::Sfx::Shot, 1.0f, -1.0f);
    mixer.mix(out, 1024);
    int left = 0, right = 0;
    for (int i = 0; i < 1024; ++i) {
        left = abs(out[i * 2]) > left ? abs(out[i * 2]) : left;
        right = abs(out[i * 2 + 1]) > right ? abs(out[i * 2 + 1]) : right;
    }
    check(left > 4000 && right == 0, "panorama a la izquierda: solo suena el canal izquierdo");
    for (int i = 0; i < 12; ++i) mixer.play(audio::Sfx::Shot);
    mixer.mix(out, 64);
    check(mixer.activeVoices() == audio::Mixer::kVoices, "con 12 sonidos a la vez suenan 8 (se reutiliza la voz mas antigua)");
    for (int i = 0; i < 4 * 2000 / 1024 + 2; ++i) mixer.mix(out, 1024);
    check(mixer.activeVoices() == 0, "las voces se liberan al terminar el sonido");
    mixer.play(audio::Sfx::Shot);
    mixer.play(audio::Sfx::Shot);
    mixer.mix(out, 64);
    mixer.stop(audio::Sfx::Shot);
    mixer.mix(out, 64);
    check(mixer.activeVoices() == 0, "stop() corta un sonido que esta sonando (beber al cancelar)");
    for (int i = 0; i < 70; ++i) mixer.play(audio::Sfx::Shot);
    check(mixer.droppedCommands() == 70 - 63, "con la cola llena se descartan ordenes en vez de bloquear");
}

// Controles de la fase 7 con los botones reales: cruceta, R con consumibles, pico, camara.
void testNewControls(Sim& sim) {
    printf("\n5b) Inventario, consumibles, pico y camara con el mando\n");
    game::Match& match = sim.app().testMatch();
    const game::Inventory& inv = match.inventory();
    sim.wait(0.5f);
    sim.press(PSP_CTRL_RIGHT);
    check(inv.selected() == 1 && inv.activeType() == game::ItemType::Bandage, "cruceta derecha: ranura de las vendas");
    sim.press(PSP_CTRL_RTRIGGER);
    sim.wait(0.2f);
    check(match.useProgress() < 0.0f && match.lastUseError() == game::UseError::FullHealth,
          "R con vendas y la vida llena: no se usan (aviso en el HUD)");
    sim.shot("22_vida_al_maximo");
    sim.press(PSP_CTRL_RIGHT);
    sim.press(PSP_CTRL_RTRIGGER);
    sim.wait(1.0f);
    sim.shot("23_usando_minipocion");
    sim.takeAudioPeak();
    sim.wait(1.2f);
    check(match.body(0).state().shield == 25.0f && inv.slot(2).count == 2, "R con minipociones: +25 de escudo en 2 s");
    check(sim.takeAudioPeak() > 3000, "suena el sonido de curacion al terminar");

    // Pico frente a un arbol: se coloca al jugador mirando hacia donde mira la camara.
    sim.press(PSP_CTRL_TRIANGLE);
    check(inv.pickaxe(), "TRIANGULO saca el pico");
    const world::World& world = sim.app().testWorld();
    const float yaw = sim.app().testCameraYaw();
    const Vec3 forward{-sinf(yaw), 0.0f, -cosf(yaw)};
    int tree = -1;
    for (int i = 0; i < static_cast<int>(world.harvestables().size()) && tree < 0; ++i) {
        const world::Harvestable& h = world.harvestables()[i];
        if (h.resource != world::Resource::Wood || h.health != 300.0f) continue;  // un arbol
        const Vec3 c = (h.box.min + h.box.max) * 0.5f;
        if (world::stormDepth(c.x, c.z) > -5.0f) continue;  // fuera de la tormenta del borde
        const Vec3 feet{c.x - forward.x * 1.0f, 0.0f, c.z - forward.z * 1.0f};
        const world::Aabb spot{{feet.x - 0.4f, 0.1f, feet.z - 0.4f}, {feet.x + 0.4f, 1.8f, feet.z + 0.4f}};
        if (!world.collision().overlapsAny(spot, world::kBlocksMovement)) tree = i;
    }
    const world::Harvestable& h = world.harvestables()[tree];
    const Vec3 c = (h.box.min + h.box.max) * 0.5f;
    match.teleport(0, {c.x - forward.x * 1.0f, 0.0f, c.z - forward.z * 1.0f}, yaw);
    sim.wait(1.0f);  // la camara llega a su sitio
    const int wood = match.materials().get(world::Resource::Wood);
    sim.takeAudioPeak();
    sim.hold(PSP_CTRL_RTRIGGER, 0.55f);
    sim.shot("24_pico_arbol", PSP_CTRL_RTRIGGER);
    sim.hold(PSP_CTRL_RTRIGGER, 0.5f);
    printf("  pico frente a un arbol 1 s: madera %d -> %d\n", wood, match.materials().get(world::Resource::Wood));
    check(match.materials().get(world::Resource::Wood) >= wood + 16, "R con el pico frente a un arbol da madera");
    check(sim.takeAudioPeak() > 3000, "los golpes de pico suenan");

    // Camara: al andar de lado se coloca detras; con L sin objetivo el stick la gira.
    float before = sim.app().testCameraYaw();
    const u32 steps = sim.app().testFootsteps();
    sim.hold(0, 1.5f, 1.0f, 0.0f);
    const u32 taken = sim.app().testFootsteps() - steps;
    printf("  1.5 s corriendo: %u pisadas con sonido\n", static_cast<unsigned>(taken));
    check(taken >= 8 && taken <= 14, "cada pisada suena (unas 6 por segundo corriendo)");
    float turned = fabsf(game::wrapAngle(sim.app().testCameraYaw() - before));
    printf("  1.5 s andando a la derecha: la camara gira %.0f grados\n", static_cast<double>(turned * 57.3f));
    check(turned > 0.5f, "al andar, la camara se va colocando detras del jugador");
    // Al pulsar L sin objetivo la camara se recoloca detras y el jugador frena: se mide despues.
    sim.hold(PSP_CTRL_LTRIGGER, 0.5f);
    const Vec3 at = match.body(0).state().position;
    before = sim.app().testCameraYaw();
    sim.hold(PSP_CTRL_LTRIGGER, 0.6f, 1.0f, 0.0f);
    turned = fabsf(game::wrapAngle(sim.app().testCameraYaw() - before));
    const float moved = math3d::length(match.body(0).state().position - at);
    printf("  L + stick 0.6 s (objetivo fijado: %s): la camara gira %.0f grados, el jugador se mueve %.2f m\n",
           sim.app().testLockTarget() > 0 ? "si" : "no", static_cast<double>(turned * 57.3f),
           static_cast<double>(moved));
    if (sim.app().testLockTarget() <= 0) {
        check(turned > 0.5f && moved < 0.2f, "L sin objetivo: el stick gira la camara y el jugador no anda");
    }

    // Construccion: la cruceta elige la pieza; R coloca el suelo y despues la rampa.
    sim.press(PSP_CTRL_CIRCLE);
    sim.press(PSP_CTRL_RIGHT);
    const int pieces = match.structures().count();
    const bool floorValid = match.planWall(sim.app().testCameraYaw(), game::PieceType::Floor).valid;
    sim.press(PSP_CTRL_RTRIGGER);
    sim.wait(0.2f);
    sim.shot("25_construccion_suelo");
    printf("  suelo: %s, piezas %d -> %d\n", floorValid ? "valido" : "no valido", pieces, match.structures().count());
    check(sim.app().testBuildPiece() == 1 && floorValid && match.structures().count() == pieces + 1,
          "en construccion la cruceta elige la pieza y R coloca un suelo");
    sim.press(PSP_CTRL_RIGHT);
    sim.press(PSP_CTRL_RTRIGGER);
    sim.wait(0.2f);
    sim.shot("25b_construccion_rampa");
    check(sim.app().testBuildPiece() == 2 && match.structures().count() == pieces + 2,
          "y con la rampa elegida, R coloca una rampa");
    // CUADRADO cambia de material; sin bastante del elegido se construye con otro.
    sim.press(PSP_CTRL_SQUARE);
    const bool stoneChosen = match.buildMaterial() == world::Resource::Stone;
    const int woodNow = match.materials().get(world::Resource::Wood);
    const game::WallPlacement fallback = match.planWall(sim.app().testCameraYaw() + 1.5708f);
    match.giveMaterials(world::Resource::Stone, 30);
    const game::WallPlacement withStone = match.planWall(sim.app().testCameraYaw() + 1.5708f);
    printf("  CUADRADO: material %s; sin piedra la pieza seria de %s (madera %d), con piedra de %s\n",
           stoneChosen ? "piedra" : "?", fallback.material == world::Resource::Wood ? "madera" : "otro", woodNow,
           withStone.material == world::Resource::Stone ? "piedra" : "otro");
    sim.press(PSP_CTRL_LEFT);
    sim.press(PSP_CTRL_LEFT);  // pared
    sim.shot("25c_construccion_piedra");
    check(stoneChosen && (woodNow < 10 || fallback.material == world::Resource::Wood) &&
              withStone.material == world::Resource::Stone,
          "CUADRADO cambia el material y sin el elegido se construye con el que haya");
    sim.press(PSP_CTRL_SQUARE);
    sim.press(PSP_CTRL_SQUARE);  // de vuelta a la madera
    sim.press(PSP_CTRL_RIGHT);
    sim.press(PSP_CTRL_RIGHT);
    sim.press(PSP_CTRL_LEFT);
    sim.press(PSP_CTRL_LEFT);
    sim.press(PSP_CTRL_CIRCLE);

    // Camara libre: TRIANGULO solo en la pagina de depuracion.
    sim.pressWith(PSP_CTRL_SELECT, PSP_CTRL_LTRIGGER);
    sim.press(PSP_CTRL_TRIANGLE);
    check(sim.app().testFreeCamera() && inv.pickaxe(), "en la pagina de depuracion TRIANGULO abre la camara libre");
    sim.press(PSP_CTRL_TRIANGLE);
    for (int i = 0; i < 3; ++i) sim.pressWith(PSP_CTRL_SELECT, PSP_CTRL_LTRIGGER);
    check(!sim.app().testFreeCamera(), "y la cierra");
    sim.press(PSP_CTRL_LEFT);
    sim.shot("26_hud_inventario");

    // Mira segun lo que se lleva y mirilla del francotirador (L).
    auto select = [&](game::ItemType type) {
        for (int k = 0; k < 7; ++k) {
            if (!inv.pickaxe() && inv.activeType() == type) return true;
            sim.press(PSP_CTRL_RIGHT);
        }
        return false;
    };
    if (select(game::ItemType::Bandage)) sim.shot("26b_mira_curativo");
    const int noRoomShotgun = match.giveItem(game::ItemType::Shotgun, 1, game::Rarity::Rare);
    const int noRoomSniper = match.giveItem(game::ItemType::Sniper, 1, game::Rarity::Legendary);
    printf("  escopeta %s, francotirador %s\n", noRoomShotgun ? "sin sitio" : "dada", noRoomSniper ? "sin sitio" : "dado");
    if (!noRoomShotgun && select(game::ItemType::Shotgun)) sim.shot("26c_mira_escopeta");
    check(!noRoomSniper && select(game::ItemType::Sniper), "se lleva el francotirador en la mano");
    check(!sim.app().testScoped(), "sin L no se mira por la mirilla");
    sim.hold(PSP_CTRL_LTRIGGER, 0.4f);
    const Vec3 head = match.body(0).state().position + Vec3{0.0f, 1.62f, 0.0f};
    const float eyeToHead = math3d::length(sim.app().testCameraEye() - head);
    sim.shot("26d_mirilla", PSP_CTRL_LTRIGGER);
    printf("  con L: mirilla %s, ojo de la camara a %.2f m de la cabeza\n", sim.app().testScoped() ? "si" : "no",
           static_cast<double>(eyeToHead));
    check(sim.app().testScoped() && eyeToHead < 0.5f, "L con el francotirador: vista por la mirilla desde la cabeza");
    sim.wait(0.2f);
    check(!sim.app().testScoped(), "al soltar L se vuelve a la tercera persona");
    select(game::ItemType::AssaultRifle);
}

void loadFont(const char* path) {
    static unsigned char font[2048];
    FILE* f = fopen(path, "rb");
    if (f && fread(font, 1, sizeof(font), f) == sizeof(font)) {
        softgfx::setFont(font);
    } else {
        printf("  (sin %s: el texto se dibuja como bloques)\n", path);
    }
    if (f) fclose(f);
}

}  // namespace

int main(int argc, char** argv) {
    const char* outDir = argc > 1 ? argv[1] : ".";
    char path[512];
    snprintf(path, sizeof(path), "%s/msx_font.bin", outDir);
    loadFont(path);
    snprintf(path, sizeof(path), "%s/fortnitepsp_log.txt", outDir);
    g_logFile = fopen(path, "w");

    testAudio(outDir);

    // Sin perfil: la primera vez se pide el nombre.
    char profilePath[512];
    snprintf(profilePath, sizeof(profilePath), "%s/perfil.txt", outDir);
    remove(profilePath);

    Sim sim(outDir);
    const unsigned heapBefore = heapBytes();
    sim.init();
    printf("Arranque: heap %u KB (+%u KB de recursos y escenario)\n", heapBytes() / 1024,
           (heapBytes() - heapBefore) / 1024);

    printf("\n1) Inicio de sesion\n");
    sim.wait(0.5f);
    check(sim.screen() == kLogin, "la aplicacion arranca en la pantalla de inicio de sesion");
    sim.shot("01_login");
    sim.takeAudioPeak();
    sim.wait(0.5f);
    check(sim.takeAudioPeak() > 1000, "la musica del menu suena desde el inicio");
    sim.press(PSP_CTRL_CROSS);
    sim.wait(0.7f);
    sim.shot("02_login_conectando");
    check(sim.screen() == kLogin, "X inicia la conexion simulada (sigue en la pantalla de inicio)");
    check(sim.waitUntil([&] { return sim.screen() == kNickname; }, 3.0f),
          "la primera vez (sin perfil.txt) se pide el nombre del jugador");
    // Teclado en pantalla: cursor en la A; se escribe MARIO y START acepta.
    sim.wait(0.2f);
    {
        const char* rows[] = {"ABCDEFGHIJ", "KLMNOPQRST", "UVWXYZ-_.!", "0123456789"};
        int row = 0, col = 0;
        for (const char* c = "MARIO"; *c; ++c) {
            for (int r = 0; r < 4; ++r) {
                const char* at = strchr(rows[r], *c);
                if (!at) continue;
                const int targetCol = static_cast<int>(at - rows[r]);
                for (; row < r; ++row) sim.press(PSP_CTRL_DOWN);
                for (; row > r; --row) sim.press(PSP_CTRL_UP);
                for (; col < targetCol; ++col) sim.press(PSP_CTRL_RIGHT);
                for (; col > targetCol; --col) sim.press(PSP_CTRL_LEFT);
                sim.press(PSP_CTRL_CROSS);
                break;
            }
        }
    }
    sim.shot("02b_nombre");
    sim.press(PSP_CTRL_START);
    sim.wait(0.2f);
    check(sim.screen() == kLobby && strcmp(sim.app().testProfile().name, "MARIO") == 0,
          "con START se acepta el nombre y se entra en el lobby");

    printf("\n2) Lobby\n");
    sim.hold(0, 0.6f, 0.0f, 0.0f);
    sim.shot("03_lobby");
    sim.hold(0, 0.5f, 1.0f, 0.0f);  // el stick gira el personaje
    sim.shot("04_lobby_girado");
    sim.press(PSP_CTRL_DOWN);
    sim.wait(0.2f);
    sim.shot("05_lobby_controles");
    sim.press(PSP_CTRL_CROSS);
    sim.wait(0.2f);
    check(sim.screen() == kControls, "CONTROLES abre la pantalla de controles");
    sim.shot("06_controles", PSP_CTRL_LTRIGGER | PSP_CTRL_CROSS);
    sim.press(PSP_CTRL_CIRCLE);
    check(sim.screen() == kLobby, "O vuelve al lobby");
    sim.press(PSP_CTRL_UP);
    sim.press(PSP_CTRL_RIGHT);
    check(sim.app().testBotSetting() == 15, "derecha sobre JUGAR sube los bots (15)");
    sim.press(PSP_CTRL_LEFT);
    check(sim.app().testBotSetting() == 10, "izquierda los baja (10)");

    printf("\n3) Carga y partida\n");
    sim.press(PSP_CTRL_CROSS);
    check(sim.screen() == kLoading, "JUGAR abre la pantalla de carga");
    sim.wait(1.0f);
    sim.shot("07_carga");
    check(sim.waitUntil([&] { return sim.screen() == kGame; }, 3.0f), "tras la carga empieza la partida");
    game::Match& match = sim.app().testMatch();
    check(match.botCount() == 10 && match.aliveCount() == 11, "la partida tiene 10 bots y 11 vivos");

    // Autobus de batalla, caida libre y ala delta, con los botones.
    check(match.airState(0) == game::AirState::Bus, "la partida empieza en el autobus de batalla");
    sim.wait(1.0f);
    sim.shot("08a_autobus");
    sim.takeAudioPeak();
    sim.press(PSP_CTRL_CROSS);
    sim.wait(0.3f);
    const int jumpPeak = sim.takeAudioPeak();
    printf("  salto: estado %s, pico de audio %d\n", game::airStateName(match.airState(0)), jumpPeak);
    check(match.airState(0) == game::AirState::Freefall && jumpPeak > 3000, "X: salto del autobus (con su sonido)");
    sim.hold(0, 1.2f, 0.0f, -1.0f);
    sim.shot("08b_caida_libre", 0);
    sim.press(PSP_CTRL_CROSS);
    check(match.airState(0) == game::AirState::Glide, "X en caida libre abre el ala delta");
    sim.hold(0, 0.8f, 0.0f, -1.0f);
    sim.shot("08c_ala_delta");
    check(sim.waitUntil([&] { return match.airState(0) == game::AirState::Ground; }, 25.0f), "con el ala delta se llega al suelo");

    // Cofre: CUADRADO lo abre y recoge lo que suelta.
    int chest = -1;
    float best = 1e9f;
    for (size_t i = 0; i < match.containers().size(); ++i) {
        const game::Container& c = match.containers()[i];
        const float d = math3d::length(c.position - match.body(0).state().position);
        if (c.kind == game::ContainerKind::Chest && !c.opened && d < best) {
            best = d;
            chest = static_cast<int>(i);
        }
    }
    const game::Container& c = match.containers()[chest];
    const float yawToChest = sim.app().testCameraYaw();
    const Vec3 inFront = c.position - Vec3{-sinf(yawToChest), 0.0f, -cosf(yawToChest)} * 1.3f;
    match.teleport(0, inFront, yawToChest);
    sim.wait(0.5f);
    sim.shot("08d_cofre");
    sim.press(PSP_CTRL_SQUARE);
    check(match.containers()[chest].opened, "CUADRADO abre el cofre");
    sim.wait(0.2f);
    sim.shot("08e_cofre_abierto");
    sim.wait(0.8f);
    sim.shot("08e2_cofre_desaparece");  // abierto, desaparece enseguida (como en Fortnite)
    for (int k = 0; k < 4; ++k) sim.press(PSP_CTRL_SQUARE);
    bool armed = false;
    for (int i = 0; i < game::Inventory::kSlots; ++i) armed |= game::isWeapon(match.inventory().slot(i).type);
    check(armed, "CUADRADO recoge el arma que suelta el cofre");
    sim.shot("08f_botin_recogido");
    check(landAndEquip(sim), "equipo de pruebas listo");
    sim.wait(1.0f);
    sim.shot("08_partida");

    // SELECT: mapa a pantalla completa (y otra vez para cerrarlo).
    sim.press(PSP_CTRL_SELECT);
    sim.wait(0.1f);
    sim.shot("08g_mapa");
    const bool mapOpen = sim.app().testMapOpen();
    sim.press(PSP_CTRL_SELECT);
    check(mapOpen && !sim.app().testMapOpen(), "SELECT abre y cierra el mapa a pantalla completa");
    // Cruceta arriba: baile (con su musica) hasta moverse.
    sim.takeAudioPeak();
    sim.press(PSP_CTRL_UP);
    sim.wait(0.6f);
    sim.shot("08h_baile");
    const bool dancing = sim.app().testDancing();
    const int dancePeak = sim.takeAudioPeak();
    sim.hold(0, 0.2f, 1.0f, 0.0f);
    check(dancing && dancePeak > 3000 && !sim.app().testDancing(), "cruceta arriba baila (con musica) y al moverse para");
    // Cruceta abajo: agacharse (mas lento) y otra vez para levantarse.
    sim.press(PSP_CTRL_DOWN);
    const Vec3 before = match.body(0).state().position;
    sim.hold(0, 1.0f, 0.0f, -1.0f);
    const float crouchedSpeed = math3d::length(match.body(0).state().position - before);
    sim.shot("08i_agachado");
    const bool crouched = match.playerCrouched();
    sim.press(PSP_CTRL_DOWN);
    printf("  agachado: %.1f m en 1 s (de pie ~5.5)\n", static_cast<double>(crouchedSpeed));
    check(crouched && crouchedSpeed < 3.5f && !match.playerCrouched(), "cruceta abajo agacha (mas lento) y levanta");

    const int shotsBefore = match.stats().shotsFired;
    sim.takeAudioPeak();
    sim.hold(PSP_CTRL_RTRIGGER, 0.8f);
    sim.shot("09_disparando", PSP_CTRL_RTRIGGER);
    check(match.stats().shotsFired - shotsBefore >= 4, "R dispara la SCAR");
    check(sim.takeAudioPeak() > 4000, "los disparos suenan");
    check(match.playerWeapon().ammo() < 30, "la municion del HUD baja");

    sim.press(PSP_CTRL_CIRCLE);
    sim.wait(0.3f);
    sim.press(PSP_CTRL_RTRIGGER);
    sim.wait(0.2f);
    check(match.buildMode() && match.structures().count() == 1, "O + R construye una pared");
    sim.shot("10_construccion");
    sim.press(PSP_CTRL_CIRCLE);
    sim.press(PSP_CTRL_SQUARE);  // recarga
    sim.wait(0.3f);
    sim.shot("11_recargando");
    sim.pressWith(PSP_CTRL_SELECT, PSP_CTRL_LTRIGGER);
    sim.shot("12_hud_depuracion");
    sim.pressWith(PSP_CTRL_SELECT, PSP_CTRL_LTRIGGER);
    sim.shot("13_hud_controles");
    sim.pressWith(PSP_CTRL_SELECT, PSP_CTRL_LTRIGGER);
    sim.pressWith(PSP_CTRL_SELECT, PSP_CTRL_LTRIGGER);

    sim.press(PSP_CTRL_START);
    check(sim.app().testPaused(), "START pausa la partida");
    const float pausedAt = match.elapsed();
    sim.wait(1.0f);
    sim.shot("14_pausa");
    check(match.elapsed() == pausedAt, "en pausa la partida no avanza");
    sim.press(PSP_CTRL_START);
    check(!sim.app().testPaused(), "START reanuda");

    printf("\n4) Derrota, seguir mirando y fin\n");
    const bool died = sim.waitUntil([&] { return !match.alive(0); }, 20.0f * 60.0f);
    check(died, "el jugador quieto acaba cayendo (como mucho 20 min de juego)");
    sim.wait(1.0f);
    sim.shot("15_caido");
    check(sim.results() == kNone, "la pantalla de derrota espera a ver la caida");
    check(sim.waitUntil([&] { return sim.results() == kDefeat; }, 3.0f), "aparece la pantalla de derrota");
    sim.press(PSP_CTRL_CROSS);
    check(sim.screen() == kGame, "los resultados ignoran los botones al aparecer (no se saltan disparando)");
    sim.wait(1.0f);
    sim.shot("16_derrota");
    const bool finishedEarly = match.phase() == game::MatchPhase::Finished;
    if (!finishedEarly) {
        sim.press(PSP_CTRL_CIRCLE);
        check(sim.results() == kNone, "O: seguir mirando oculta los resultados");
        sim.wait(1.0f);
        sim.shot("17_espectando");
        check(sim.waitUntil([&] { return match.phase() == game::MatchPhase::Finished; }, 20.0f * 60.0f),
              "la partida termina mientras se mira");
        check(sim.waitUntil([&] { return sim.results() == kDefeat; }, 4.0f),
              "al terminar vuelven los resultados con el ganador");
        sim.wait(1.0f);
        sim.shot("18_derrota_final");
    }
    printf("  puesto %d de %d, ganador %s, %.0f s de partida\n", match.playerPlace(), match.entityCount(),
           match.name(match.winner()), static_cast<double>(match.elapsed()));

    printf("\n5) Otra partida (reinicio) y victoria\n");
    sim.press(PSP_CTRL_SQUARE);
    check(sim.screen() == kLoading, "[] en los resultados abre otra partida");
    check(sim.waitUntil([&] { return sim.screen() == kGame; }, 3.0f), "la nueva partida empieza");
    check(match.aliveCount() == 11 && match.elapsed() < 0.5f && match.structures().count() == 0 &&
              sim.app().testWorld().collision().dynamicCount() == 0,
          "la partida nueva empieza limpia (11 vivos, sin paredes ni cajas dinamicas)");
    check(landAndEquip(sim), "salto, aterrizaje y equipo de pruebas");
    testNewControls(sim);
    sim.takeAudioPeak();
    killAllBots(sim);
    check(sim.takeAudioPeak() > 4000, "las eliminaciones suenan");
    printf("  victoria con %d bajas del jugador (el resto, entre bots)\n", match.playerKills());
    check(match.phase() == game::MatchPhase::Finished && match.winner() == 0 && match.playerKills() >= 1,
          "al eliminar a los bots que quedan gana el jugador");
    check(sim.waitUntil([&] { return sim.results() == kVictory; }, 3.0f), "aparece la pantalla de victoria");
    sim.wait(0.1f);
    sim.shot("19_victoria_entrada");
    sim.wait(0.8f);
    sim.shot("20_victoria");
    sim.press(PSP_CTRL_CROSS);
    check(sim.screen() == kLobby, "X en los resultados vuelve al lobby");

    printf("\n6) Estabilidad: 8 partidas mas alternando victoria, derrota, reinicio y abandono\n");
    unsigned heapFirst = 0, heapLast = 0, heapMax = 0;
    bool flowOk = true, navOk = true;
    // Referencia: la rejilla al empezar la primera de estas partidas (ya sin las piezas
    // de las pruebas anteriores, que la nueva partida retira).
    int walkable = -1;
    for (int round = 0; round < 8; ++round) {
        sim.wait(0.3f);
        sim.press(PSP_CTRL_CROSS);  // JUGAR
        flowOk &= sim.waitUntil([&] { return sim.screen() == kGame; }, 3.0f);
        flowOk &= match.aliveCount() == 11 && sim.app().testWorld().collision().dynamicCount() == 0;
        if (walkable < 0) walkable = sim.app().testNavWalkable();
        navOk &= sim.app().testNavWalkable() == walkable;
        // Algo de construccion y disparos en cada partida.
        sim.press(PSP_CTRL_CIRCLE);
        for (int w = 0; w < 3; ++w) {
            sim.hold(0, 0.2f, w == 1 ? 1.0f : -1.0f, 0.0f);
            sim.press(PSP_CTRL_RTRIGGER);
        }
        sim.press(PSP_CTRL_CIRCLE);
        sim.hold(PSP_CTRL_RTRIGGER, 1.0f);
        switch (round % 4) {
            case 0:  // victoria -> lobby
                killAllBots(sim);
                flowOk &= sim.waitUntil([&] { return sim.results() == kVictory; }, 3.0f);
                sim.wait(1.0f);
                sim.press(PSP_CTRL_CROSS);
                break;
            case 1:  // derrota -> otra partida -> abandono desde la pausa
                flowOk &= sim.waitUntil([&] { return sim.results() == kDefeat; }, 20.0f * 60.0f);
                sim.wait(1.0f);
                sim.press(PSP_CTRL_SQUARE);
                flowOk &= sim.waitUntil([&] { return sim.screen() == kGame; }, 3.0f);
                flowOk &= match.structures().count() == 0;
                sim.wait(2.0f);
                sim.press(PSP_CTRL_START);
                sim.press(PSP_CTRL_DOWN);
                sim.press(PSP_CTRL_CROSS);
                break;
            case 2:  // abandono a mitad de partida con paredes construidas
                sim.wait(20.0f);
                sim.press(PSP_CTRL_START);
                sim.press(PSP_CTRL_DOWN);
                sim.press(PSP_CTRL_CROSS);
                break;
            case 3:  // derrota -> lobby
                flowOk &= sim.waitUntil([&] { return sim.results() == kDefeat; }, 20.0f * 60.0f);
                sim.wait(1.0f);
                sim.press(PSP_CTRL_CROSS);
                break;
        }
        flowOk &= sim.screen() == kLobby;
        sim.wait(0.5f);
        const unsigned heap = heapBytes();
        if (round == 0) heapFirst = heap;
        heapLast = heap;
        if (heap > heapMax) heapMax = heap;
        printf("  ronda %d: vuelta al lobby, heap %u KB, %.0f s simulados\n", round + 1, heap / 1024,
               static_cast<double>(sim.seconds()));
    }
    check(flowOk, "todas las transiciones funcionan en todas las rondas");
    check(navOk, "cada partida empieza con la rejilla de navegacion intacta (las paredes anteriores se retiran)");
    check(heapLast <= heapFirst + 64 * 1024 && heapMax <= heapFirst + 256 * 1024,
          "la memoria no crece entre partidas (tolerancia 64 KB)");
    check(audio::mixer().droppedCommands() == 0, "no se ha perdido ningun sonido por la cola llena");

    printf("\n6b) Tormenta en el limite del mapa\n");
    sim.press(PSP_CTRL_CROSS);  // JUGAR
    check(sim.waitUntil([&] { return sim.screen() == kGame; }, 3.0f), "nueva partida");
    sim.wait(0.5f);
    const float centerFog = sim.app().testFogEnd();
    // El jugador, a 12 m del borde, mirando hacia fuera (hacia donde mira la camara).
    float yaw = sim.app().testCameraYaw();
    const Vec3 out{-sinf(yaw), 0.0f, -cosf(yaw)};
    float r = 0.0f;
    while (world::stormDepth(out.x * r, out.z * r) < -12.0f) r += 0.5f;
    match.teleport(0, out * r, yaw);
    sim.wait(1.0f);
    sim.shot("27_borde_tormenta");
    const float edgeFog = sim.app().testFogEnd();
    check(match.alive(0) && match.body(0).state().health == 100.0f, "a 12 m del borde no hay dano");
    // Dentro: 8 m en la tormenta.
    while (world::stormDepth(out.x * r, out.z * r) < 8.0f) r += 0.5f;
    sim.takeAudioPeak();
    match.teleport(0, out * r, yaw);
    sim.wait(0.6f);
    sim.shot("28_dentro_tormenta");
    const float stormFog = sim.app().testFogEnd();
    printf("  niebla hasta: %.0f m en el centro, %.0f m a 12 m del borde, %.0f m dentro de la tormenta\n",
           static_cast<double>(centerFog), static_cast<double>(edgeFog), static_cast<double>(stormFog));
    check(centerFog > 90.0f && edgeFog < centerFog && stormFog < 20.0f,
          "la niebla se espesa al acercarse y dentro solo se ven unos metros");
    check(match.inStorm(0) && match.body(0).state().health <= 70.0f && sim.takeAudioPeak() > 3000,
          "dentro de la tormenta: dano y aviso sonoro");
    check(sim.waitUntil([&] { return sim.results() == kDefeat; }, 8.0f), "en la tormenta se cae y sale la derrota");
    sim.wait(1.0f);
    sim.shot("29_derrota_tormenta");
    sim.press(PSP_CTRL_CROSS);
    check(sim.screen() == kLobby, "vuelta al lobby");

    printf("\n6c) Tormenta que se cierra\n");
    sim.press(PSP_CTRL_CROSS);  // JUGAR
    sim.waitUntil([&] { return sim.screen() == kGame; }, 3.0f);
    sim.press(PSP_CTRL_CROSS);  // saltar del autobus
    sim.waitUntil([&] { return match.airState(0) == game::AirState::Ground; }, 40.0f);
    // Lejos de los bots (cazan hasta 80 m) hasta que empiece el segundo cierre.
    match.teleport(0, {-68.0f, 0.0f, 68.0f}, 0.0f);
    match.setPlayerInvulnerable(true);  // que ningun bot lo elimine mientras se espera
    const bool closing = sim.waitUntil([&] { return match.storm().phase() >= 2 || !match.alive(0); }, 130.0f);
    const game::StormZone& storm = match.storm();
    printf("  a los %.0f s: cierre %d, radio %.0f m, %s\n", static_cast<double>(match.elapsed()), storm.phase(),
           static_cast<double>(storm.radius()), match.alive(0) ? "jugador vivo" : "jugador eliminado");
    check(closing && storm.phase() >= 2 && storm.radius() < 92.0f, "la tormenta se cierra en partida (HUD con su tiempo)");
    if (match.alive(0)) {
        // A 8 m del borde, mirando hacia la tormenta (del centro de la zona hacia el del mapa).
        Vec3 dir = math3d::length(storm.center()) > 1.0f ? math3d::normalize(Vec3{0, 0, 0} - storm.center()) : Vec3{1, 0, 0};
        dir.y = 0.0f;
        const float faceYaw = atan2f(-dir.x, -dir.z);
        match.teleport(0, storm.center() + dir * (storm.radius() - 8.0f), faceYaw);
        sim.press(PSP_CTRL_LTRIGGER);  // recoloca la camara detras
        sim.wait(0.6f);
        sim.shot("29b_tormenta_cerrandose");
        match.setPlayerInvulnerable(false);
        match.teleport(0, storm.center() + dir * (storm.radius() + 4.0f), faceYaw);
        const float before = match.body(0).state().health;
        sim.wait(1.2f);
        sim.shot("29c_dentro_tormenta_cerrandose");
        printf("  fuera de la zona: vida %.0f -> %.0f, niebla hasta %.0f m\n", static_cast<double>(before),
               static_cast<double>(match.body(0).state().health), static_cast<double>(sim.app().testFogEnd()));
        check(match.inStorm(0) && match.body(0).state().health < before && sim.app().testFogEnd() < 60.0f,
              "fuera de la zona: dano de la fase y niebla morada");
    }
    sim.wait(1.5f);
    if (sim.results() != kNone) {
        sim.press(PSP_CTRL_CROSS);
    } else {
        sim.press(PSP_CTRL_START);
        sim.press(PSP_CTRL_DOWN);
        sim.press(PSP_CTRL_CROSS);
    }
    check(sim.waitUntil([&] { return sim.screen() == kLobby; }, 2.0f), "vuelta al lobby");

    // Sonidos de mas de 3 s (pocion de escudo, 6.6 s): suenan una vez y acaban (con la
    // posicion en 16.16 se desbordaba a las 65536 muestras y se repetian sin fin).
    audio::mixer().stopAll();
    sim.wait(0.1f);
    audio::mixer().play(audio::Sfx::DrinkShieldPotion, 1.0f, 0.0f);
    sim.wait(1.0f);
    const int playing = audio::mixer().activeVoices();
    sim.wait(6.5f);
    printf("  pocion de escudo (6.6 s): voces activas %d al segundo, %d a los 7.5 s\n", playing,
           audio::mixer().activeVoices());
    check(playing == 1 && audio::mixer().activeVoices() == 0, "un sonido largo suena una vez y termina (no se repite)");

    printf("\n7) Salir desde el lobby\n");
    sim.press(PSP_CTRL_DOWN);
    sim.press(PSP_CTRL_DOWN);
    sim.press(PSP_CTRL_CROSS);
    sim.wait(0.2f);
    sim.shot("21_salir");
    check(!sim.app().exitRequested(), "SALIR pide confirmacion");
    sim.press(PSP_CTRL_CROSS);
    check(sim.app().exitRequested(), "X en la confirmacion cierra el juego");

    sim.writeAudio();
    const app::Profile played = sim.app().testProfile();
    sim.app().shutdown();

    printf("\n8) Perfil guardado\n");
    app::Profile loaded;
    const bool read = loaded.load(profilePath);
    printf("  perfil.txt: %s, %d partidas, %d victorias, %d bajas, %d a la cabeza, mejor puesto %d\n", loaded.name,
           loaded.matches, loaded.wins, loaded.kills, loaded.headshots, loaded.bestPlace);
    check(read && strcmp(loaded.name, "MARIO") == 0 && loaded.matches == played.matches && loaded.matches >= 8 &&
              loaded.wins >= 1 && loaded.kills == played.kills && loaded.kills > 0 && loaded.bestPlace == 1,
          "las estadisticas de todas las partidas quedan guardadas en perfil.txt");
    // Al volver a entrar: el juego carga el perfil y del inicio va directo al lobby.
    app::App again;
    again.setDataDirectory(outDir);
    again.init();
    const bool same = strcmp(again.testProfile().name, "MARIO") == 0 && again.testProfile().matches == loaded.matches;
    check(same, "al volver a entrar se cargan el nombre y las estadisticas");
    again.shutdown();
    if (g_logFile) fclose(g_logFile);
    printf("\n%s (%d fallos)\n", g_failures == 0 ? "TODO CORRECTO" : "HAY FALLOS", g_failures);
    return g_failures == 0 ? 0 : 1;
}
