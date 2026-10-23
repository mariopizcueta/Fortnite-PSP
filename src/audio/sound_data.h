// Datos de sonido: efectos WAV PCM de 16 bits y musica IMA ADPCM por bloques.
//
// Los dos formatos los genera tools/prepare_assets.py y se leen directamente de los
// recursos incrustados (sin copiar): los efectos se reproducen tal cual y la musica se
// decodifica bloque a bloque mientras suena (~1.3 MB en vez de ~5 MB en PCM).
#pragma once

#include <stddef.h>

#include <psptypes.h>

namespace audio {

struct SoundClip {
    const s16* samples = nullptr;
    int count = 0;
    int rate = 0;
};

// Interpreta un WAV PCM 16 bits mono. Devuelve false si el formato no es ese.
bool parseWav(const void* data, size_t size, SoundClip* out);

// Musica IMA ADPCM ("IMA1": cabecera + bloques de blockBytes con su propio estado).
class ImaStream {
public:
    bool open(const void* data, size_t size);
    bool valid() const { return m_data != nullptr; }
    int rate() const { return m_rate; }
    int totalSamples() const { return m_total; }
    // Siguiente muestra; vuelve al principio al acabar (bucle).
    s16 next();
    void rewind();

private:
    void decodeBlock(int block);

    const u8* m_data = nullptr;
    int m_rate = 0;
    int m_total = 0;
    int m_blockBytes = 0;
    int m_samplesPerBlock = 0;
    int m_blocks = 0;
    int m_position = 0;      // muestra global
    int m_blockLoaded = -1;
    s16 m_buffer[4096];      // un bloque decodificado
};

// Decodificacion IMA ADPCM de un nibble (expuesta para las pruebas de host).
s16 imaDecodeNibble(int code, int* predictor, int* index);

}  // namespace audio
