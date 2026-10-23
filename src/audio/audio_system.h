// Audio de la PSP: carga los sonidos incrustados y alimenta el mezclador desde el hilo de
// audio de pspaudiolib (canal 0, estereo 44.1 kHz, bloques de 1024 muestras).
#pragma once

#include "audio/mixer.h"

namespace audio {

// Devuelve false si no se pudo abrir el canal de audio (el juego sigue, en silencio).
bool init();
void shutdown();
Mixer& mixer();

// Bytes de los recursos de sonido (efectos + musica comprimida), para el log.
unsigned resourceBytes();

}  // namespace audio
