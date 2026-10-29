// Mezclador de audio: 8 voces de efectos + 1 pista de musica, salida estereo 44.1 kHz.
//
// El juego llama a play()/setMusic() desde su hilo; el hilo de audio llama a mix(). La
// comunicacion es una cola circular de un productor y un consumidor (sin bloqueos): el
// productor escribe la orden y despues avanza la cabeza; el consumidor la lee y avanza
// la cola. El remuestreo es por vecino mas cercano (22050 -> 44100 Hz: cada muestra se
// repite), barato y con el caracter "crujiente" de las consolas de la epoca.
#pragma once

#include <psptypes.h>

#include "audio/sound_data.h"

namespace audio {

enum class Sfx : u8 {
    Shot, HitMarker, Hurt, Elimination, WallBuild, WallBreak, Reload,
    MenuMove, MenuAccept, Victory, Defeat, Pickaxe, Heal,
    PlayerDeath, DrinkSmallShield, DrinkShieldPotion, Footstep, StormWarning,
    ChestOpen, ChestHum, BusJump, GliderOpen, SniperShot, ShotgunShot, SmgShot, Pickup, Dance,
    PistolShot, RocketLaunch, GrenadeLaunch, GrenadeThrow, Explosion, AmmoBoxOpen, LlamaOpen, TrapTrigger,
    // v0.13.
    RevolverShot, PumpShotgunShot, MedkitUse, BandageUse, ReloadRifle, ReloadShotgun, ReloadTactical, ReloadSmg,
    ReloadPistol, ReloadRevolver, ReloadSniper, ReloadGrenadeLauncher, ReloadRocket,
    // v0.14: modo zombi y agua.
    ZombieGroan, ZombieHit, Horde, Splash, Craft, Dawn, Count
};

class Mixer {
public:
    static constexpr int kOutputRate = 44100;
    static constexpr int kVoices = 8;

    void setClip(Sfx id, const SoundClip& clip);
    void setMusic(ImaStream* stream);  // puede ser nullptr

    // --- Hilo del juego -------------------------------------------------------------
    // volume 0..1, pan -1 (izquierda) .. +1 (derecha).
    void play(Sfx id, float volume = 1.0f, float pan = 0.0f);
    // Corta todas las voces que esten sonando ese efecto (p. ej. beber al cancelar).
    void stop(Sfx id);
    void stopAll();  // corta todos los efectos (no la musica)
    void playMusic(bool playing) { m_musicPlaying = playing ? 1 : 0; }
    void setMusicVolume(float volume);
    void setMasterVolume(float volume);

    // --- Hilo de audio -----------------------------------------------------------------
    // Rellena `frames` muestras estereo intercaladas (L, R) de 16 bits.
    void mix(s16* out, int frames);

    // Estadisticas para el HUD de depuracion.
    int activeVoices() const;
    // La voz `v` esta sonando ese efecto (pruebas de host; lee el estado del hilo de audio).
    bool voicePlaying(int v, Sfx id) const {
        return v >= 0 && v < kVoices && m_voices[v].samples && m_voices[v].samples == m_clips[static_cast<int>(id)].samples;
    }
    u32 droppedCommands() const { return m_dropped; }

private:
    struct Command {
        u8 op;      // 0 = reproducir, 1 = parar
        u8 sfx;
        u8 volume;  // 0..255
        s8 pan;     // -127..127
    };
    static constexpr int kFracBits = 12;  // parte fraccionaria de la posicion de las voces
    struct Voice {
        const s16* samples = nullptr;
        int count = 0;
        // 20.12: con 16.16 la posicion se desbordaba a las 65536 muestras (~3 s a 22 kHz)
        // y los sonidos largos (victoria, autobus, pocion) volvian a empezar sin fin.
        u32 position = 0;  // 20.12 (hasta ~47 s a 22050 Hz)
        u32 step = 0;      // 20.12
        int volumeLeft = 0, volumeRight = 0;  // Q8
        u32 age = 0;
    };

    void startVoice(const Command& c);
    void push(const Command& c);

    SoundClip m_clips[static_cast<int>(Sfx::Count)];
    ImaStream* m_music = nullptr;
    u32 m_musicPosition = 0;  // 16.16 respecto a la frecuencia de la musica
    s16 m_musicSample = 0;

    static constexpr int kQueue = 64;
    Command m_queue[kQueue];
    volatile int m_head = 0;  // lo escribe el hilo del juego
    volatile int m_tail = 0;  // lo escribe el hilo de audio
    volatile int m_musicPlaying = 0;
    volatile int m_musicVolume = 140;  // Q8
    volatile int m_masterVolume = 256;  // Q8
    u32 m_dropped = 0;

    Voice m_voices[kVoices];
    u32 m_voiceCounter = 0;
};

}  // namespace audio
