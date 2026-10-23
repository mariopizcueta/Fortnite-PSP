// Sustitutos de host (Mac) de funciones de plataforma para las herramientas de prueba.
#include <chrono>

#include "core/timer.h"

namespace core {

u64 nowMicros() {
    using namespace std::chrono;
    return static_cast<u64>(duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count());
}

}  // namespace core
