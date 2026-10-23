#include "audio/audio_system.h"

#include <pspaudiolib.h>

#include "assets/embedded_assets.h"

namespace audio {
namespace {

Mixer g_mixer;
ImaStream g_music;
bool g_running = false;
unsigned g_bytes = 0;

void audioCallback(void* buffer, unsigned int frames, void* /*userdata*/) {
    g_mixer.mix(static_cast<s16*>(buffer), static_cast<int>(frames));
}

void load(Sfx id, const assets::Blob& blob) {
    SoundClip clip;
    if (parseWav(blob.data, blob.size, &clip)) g_mixer.setClip(id, clip);
    g_bytes += static_cast<unsigned>(blob.size);
}

}  // namespace

bool init() {
    load(Sfx::Shot, assets::sfxDisparo());
    load(Sfx::HitMarker, assets::sfxImpacto());
    load(Sfx::Hurt, assets::sfxHerido());
    load(Sfx::Elimination, assets::sfxEliminacion());
    load(Sfx::WallBuild, assets::sfxPared());
    load(Sfx::WallBreak, assets::sfxParedRota());
    load(Sfx::Reload, assets::sfxRecarga());
    load(Sfx::MenuMove, assets::sfxMenuMover());
    load(Sfx::MenuAccept, assets::sfxMenuAceptar());
    load(Sfx::Victory, assets::sfxVictoria());
    load(Sfx::Defeat, assets::sfxDerrota());
    load(Sfx::Pickaxe, assets::sfxPico());
    load(Sfx::Heal, assets::sfxCuracion());
    load(Sfx::PlayerDeath, assets::sfxMuerteJugador());
    load(Sfx::DrinkSmallShield, assets::sfxMiniescudo());
    load(Sfx::DrinkShieldPotion, assets::sfxPocionEscudo());
    load(Sfx::Footstep, assets::sfxPaso());
    load(Sfx::StormWarning, assets::sfxAvisoTormenta());
    const assets::Blob music = assets::musicMenu();
    if (g_music.open(music.data, music.size)) g_mixer.setMusic(&g_music);
    g_bytes += static_cast<unsigned>(music.size);

    if (pspAudioInit() < 0) return false;
    pspAudioSetChannelCallback(0, audioCallback, nullptr);
    g_running = true;
    return true;
}

void shutdown() {
    if (!g_running) return;
    pspAudioSetChannelCallback(0, nullptr, nullptr);
    pspAudioEndPre();
    pspAudioEnd();
    g_running = false;
}

Mixer& mixer() { return g_mixer; }

unsigned resourceBytes() { return g_bytes; }

}  // namespace audio
