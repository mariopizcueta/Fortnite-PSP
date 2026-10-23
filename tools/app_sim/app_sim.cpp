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
enum Screen { kLogin, kLobby, kControls, kLoading, kGame };
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
                                  assets::sfxCuracion()};
    bool clipsOk = true;
    int clipBytes = 0;
    for (const assets::Blob& b : clips) {
        audio::SoundClip clip;
        clipsOk &= audio::parseWav(b.data, b.size, &clip) && clip.rate == 22050 && clip.count > 0;
        clipBytes += static_cast<int>(b.size);
    }
    printf("  13 efectos, %d KB en total\n", clipBytes / 1024);
    check(clipsOk, "los 13 efectos son WAV PCM 16 bits mono a 22050 Hz y se leen sin copiarlos");

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
        if (h.total != 48) continue;
        const Vec3 c = (h.box.min + h.box.max) * 0.5f;
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
    sim.hold(0, 1.5f, 1.0f, 0.0f);
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

    // Construccion: la cruceta elige la pieza; suelo y rampa aun no se colocan.
    sim.press(PSP_CTRL_CIRCLE);
    sim.press(PSP_CTRL_RIGHT);
    const int walls = match.structures().count();
    sim.press(PSP_CTRL_RTRIGGER);
    sim.wait(0.2f);
    sim.shot("25_construccion_suelo");
    check(sim.app().testBuildPiece() == 1 && match.structures().count() == walls,
          "en construccion la cruceta elige la pieza (suelo: aun no se coloca)");
    sim.press(PSP_CTRL_LEFT);
    sim.press(PSP_CTRL_CIRCLE);

    // Camara libre: TRIANGULO solo en la pagina de depuracion.
    sim.press(PSP_CTRL_SELECT);
    sim.press(PSP_CTRL_TRIANGLE);
    check(sim.app().testFreeCamera() && inv.pickaxe(), "en la pagina de depuracion TRIANGULO abre la camara libre");
    sim.press(PSP_CTRL_TRIANGLE);
    for (int i = 0; i < 3; ++i) sim.press(PSP_CTRL_SELECT);
    check(!sim.app().testFreeCamera(), "y la cierra");
    sim.press(PSP_CTRL_LEFT);
    sim.shot("26_hud_inventario");
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

    Sim sim(outDir);
    const unsigned heapBefore = heapBytes();
    sim.init();
    printf("Arranque: heap %u KB (+%u KB de recursos y escenario)\n", heapBytes() / 1024,
           (heapBytes() - heapBefore) / 1024);

    printf("\n1) Inicio de sesion\n");
    sim.wait(0.5f);
    check(sim.screen() == kLogin, "la aplicacion arranca en la pantalla de inicio de sesion");
    sim.takeAudioPeak();
    sim.wait(0.5f);
    check(sim.takeAudioPeak() > 1000, "la musica del menu suena desde el inicio");
    sim.shot("01_login");
    sim.press(PSP_CTRL_CROSS);
    sim.wait(0.7f);
    sim.shot("02_login_conectando");
    check(sim.screen() == kLogin, "X inicia la conexion simulada (sigue en la pantalla de inicio)");
    check(sim.waitUntil([&] { return sim.screen() == kLobby; }, 3.0f), "tras conectar se entra en el lobby");

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
    sim.wait(1.0f);
    sim.shot("08_partida");

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
    sim.press(PSP_CTRL_SELECT);
    sim.shot("12_hud_depuracion");
    sim.press(PSP_CTRL_SELECT);
    sim.shot("13_hud_controles");
    sim.press(PSP_CTRL_SELECT);
    sim.press(PSP_CTRL_SELECT);

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
    testNewControls(sim);
    sim.takeAudioPeak();
    killAllBots(sim);
    check(sim.takeAudioPeak() > 4000, "las eliminaciones suenan");
    check(match.phase() == game::MatchPhase::Finished && match.winner() == 0 && match.playerKills() == 10,
          "al eliminar a los 10 bots gana el jugador");
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
    const int walkable = sim.app().testNavWalkable();
    for (int round = 0; round < 8; ++round) {
        sim.wait(0.3f);
        sim.press(PSP_CTRL_CROSS);  // JUGAR
        flowOk &= sim.waitUntil([&] { return sim.screen() == kGame; }, 3.0f);
        flowOk &= match.aliveCount() == 11 && sim.app().testWorld().collision().dynamicCount() == 0;
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
    sim.app().shutdown();
    if (g_logFile) fclose(g_logFile);
    printf("\n%s (%d fallos)\n", g_failures == 0 ? "TODO CORRECTO" : "HAY FALLOS", g_failures);
    return g_failures == 0 ? 0 : 1;
}
