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
    load(Sfx::ChestOpen, assets::sfxCofreAbrir());
    load(Sfx::ChestHum, assets::sfxCofreCerca());
    load(Sfx::BusJump, assets::sfxSaltoAutobus());
    load(Sfx::GliderOpen, assets::sfxAlaDelta());
    load(Sfx::SniperShot, assets::sfxFrancotirador());
    load(Sfx::ShotgunShot, assets::sfxEscopeta());
    load(Sfx::SmgShot, assets::sfxSubfusil());
    load(Sfx::Pickup, assets::sfxRecoger());
    load(Sfx::Dance, assets::sfxBaile());
    load(Sfx::PistolShot, assets::sfxPistola());
    load(Sfx::RocketLaunch, assets::sfxCohete());
    load(Sfx::GrenadeLaunch, assets::sfxLanzagranadas());
    load(Sfx::GrenadeThrow, assets::sfxGranadaLanzar());
    load(Sfx::Explosion, assets::sfxExplosion());
    load(Sfx::AmmoBoxOpen, assets::sfxCajaMunicion());
    load(Sfx::LlamaOpen, assets::sfxLlama());
    load(Sfx::TrapTrigger, assets::sfxTrampa());
    load(Sfx::RevolverShot, assets::sfxRevolver());
    load(Sfx::PumpShotgunShot, assets::sfxEscopetaCorredera());
    load(Sfx::MedkitUse, assets::sfxBotiquin());
    load(Sfx::BandageUse, assets::sfxVendas());
    load(Sfx::ReloadRifle, assets::sfxRecargaFusil());
    load(Sfx::ReloadShotgun, assets::sfxRecargaEscopeta());
    load(Sfx::ReloadTactical, assets::sfxRecargaTactica());
    load(Sfx::ReloadSmg, assets::sfxRecargaSubfusil());
    load(Sfx::ReloadPistol, assets::sfxRecargaPistola());
    load(Sfx::ReloadRevolver, assets::sfxRecargaRevolver());
    load(Sfx::ReloadSniper, assets::sfxRecargaFrancotirador());
    load(Sfx::ReloadGrenadeLauncher, assets::sfxRecargaLanzagranadas());
    load(Sfx::ReloadRocket, assets::sfxRecargaCohete());
    load(Sfx::ZombieGroan, assets::sfxZombieGemido());
    load(Sfx::ZombieHit, assets::sfxZombieGolpe());
    load(Sfx::Horde, assets::sfxHorda());
    load(Sfx::Splash, assets::sfxChapoteo());
    load(Sfx::Craft, assets::sfxFabricar());
    load(Sfx::Dawn, assets::sfxAmanecer());
    load(Sfx::Heartbeat, assets::sfxLatido());
    load(Sfx::Scream, assets::sfxGrito());
    load(Sfx::Whisper, assets::sfxSusurro());
    load(Sfx::Jumpscare, assets::sfxSobresalto());
    load(Sfx::Eat, assets::sfxComer());
    load(Sfx::Backpack, assets::sfxMochila());
    load(Sfx::Sleep, assets::sfxRoncar());
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
