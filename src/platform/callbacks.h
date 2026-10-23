// Callback de salida del sistema (HOME -> "Salir del juego").
#pragma once

namespace platform {

// Crea el hilo que atiende los callbacks del sistema. Devuelve false si falla.
bool setupCallbacks();

// false cuando el sistema ha pedido cerrar la aplicacion.
bool isRunning();

}  // namespace platform
