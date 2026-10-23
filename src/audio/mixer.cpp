#include "audio/mixer.h"

namespace audio {
namespace {

// Barrera de compilador: que la orden quede escrita antes de publicar la cabeza (la PSP
// tiene un solo nucleo; basta con que el compilador no reordene).
inline void compilerBarrier() { __asm__ __volatile__("" ::: "memory"); }

int clampInt(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

}  // namespace

void Mixer::setClip(Sfx id, const SoundClip& clip) { m_clips[static_cast<int>(id)] = clip; }

void Mixer::setMusic(ImaStream* stream) {
    m_music = stream;
    m_musicPosition = 0;
}

void Mixer::setMusicVolume(float volume) { m_musicVolume = clampInt(static_cast<int>(volume * 256.0f), 0, 256); }
void Mixer::setMasterVolume(float volume) { m_masterVolume = clampInt(static_cast<int>(volume * 256.0f), 0, 256); }

void Mixer::play(Sfx id, float volume, float pan) {
    if (volume <= 0.003f) return;
    Command c;
    c.op = 0;
    c.sfx = static_cast<u8>(id);
    c.volume = static_cast<u8>(clampInt(static_cast<int>(volume * 255.0f), 0, 255));
    c.pan = static_cast<s8>(clampInt(static_cast<int>(pan * 127.0f), -127, 127));
    push(c);
}

void Mixer::stop(Sfx id) {
    Command c;
    c.op = 1;
    c.sfx = static_cast<u8>(id);
    c.volume = 0;
    c.pan = 0;
    push(c);
}

void Mixer::push(const Command& command) {
    const int head = m_head;
    const int next = (head + 1) % kQueue;
    if (next == m_tail) {  // cola llena: se descarta (mejor que bloquear el juego)
        ++m_dropped;
        return;
    }
    m_queue[head] = command;
    compilerBarrier();
    m_head = next;
}

void Mixer::startVoice(const Command& c) {
    const SoundClip& clip = m_clips[c.sfx];
    if (!clip.samples || clip.count <= 0) return;
    if (c.op == 1) {
        for (Voice& v : m_voices) {
            if (v.samples == clip.samples) v.samples = nullptr;
        }
        return;
    }
    // Voz libre o, si no hay, la mas antigua.
    Voice* target = &m_voices[0];
    for (Voice& v : m_voices) {
        if (!v.samples) {
            target = &v;
            break;
        }
        if (v.age < target->age) target = &v;
    }
    target->samples = clip.samples;
    target->count = clip.count;
    target->position = 0;
    target->step = static_cast<u32>((static_cast<u64>(clip.rate) << 16) / kOutputRate);
    // Panorama de potencia aproximadamente constante con dos rectas.
    const int volume = c.volume + 1;  // 1..256
    const int pan = c.pan;            // -127..127
    target->volumeLeft = volume * (pan > 0 ? 127 - pan : 127) / 127;
    target->volumeRight = volume * (pan < 0 ? 127 + pan : 127) / 127;
    target->age = ++m_voiceCounter;
}

void Mixer::mix(s16* out, int frames) {
    // 1) Ordenes pendientes del juego.
    while (m_tail != m_head) {
        startVoice(m_queue[m_tail]);
        compilerBarrier();
        m_tail = (m_tail + 1) % kQueue;
    }

    const int master = m_masterVolume;
    const bool music = m_musicPlaying && m_music && m_music->valid();
    const int musicVolume = m_musicVolume;
    const u32 musicStep =
        music ? static_cast<u32>((static_cast<u64>(m_music->rate()) << 16) / kOutputRate) : 0;

    for (int i = 0; i < frames; ++i) {
        int left = 0, right = 0;
        for (Voice& v : m_voices) {
            if (!v.samples) continue;
            const int index = static_cast<int>(v.position >> 16);
            if (index >= v.count) {
                v.samples = nullptr;
                continue;
            }
            const int s = v.samples[index];
            left += (s * v.volumeLeft) >> 8;
            right += (s * v.volumeRight) >> 8;
            v.position += v.step;
        }
        if (music) {
            // La musica avanza a su propia frecuencia (22050 Hz): una muestra nueva cada
            // vez que la parte entera de la posicion cambia.
            const u32 before = m_musicPosition >> 16;
            m_musicPosition += musicStep;
            if ((m_musicPosition >> 16) != before) m_musicSample = m_music->next();
            const int s = (m_musicSample * musicVolume) >> 8;
            left += s;
            right += s;
        }
        left = (left * master) >> 8;
        right = (right * master) >> 8;
        out[2 * i] = static_cast<s16>(clampInt(left, -32768, 32767));
        out[2 * i + 1] = static_cast<s16>(clampInt(right, -32768, 32767));
    }
}

int Mixer::activeVoices() const {
    int n = 0;
    for (const Voice& v : m_voices) n += v.samples ? 1 : 0;
    return n;
}

}  // namespace audio
