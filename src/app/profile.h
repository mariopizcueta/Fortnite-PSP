// Perfil del jugador: nombre (se pide la primera vez) y estadisticas de todas sus
// partidas. Se guarda en texto plano ("clave=valor") en perfil.txt, junto al EBOOT
// (ms0:/PSP/GAME/FortnitePSP/perfil.txt), y se carga al arrancar.
#pragma once

#include <psptypes.h>

namespace app {

struct Profile {
    static constexpr int kMaxName = 12;
    char name[kMaxName + 1] = "";
    int matches = 0;     // partidas jugadas (terminadas o abandonadas)
    int wins = 0;        // victorias magistrales
    int kills = 0;       // bajas en total
    int deaths = 0;      // veces eliminado
    int headshots = 0;   // impactos en la cabeza
    int mostKills = 0;   // mas bajas en una partida
    int bestPlace = 0;   // mejor puesto (0 = aun ninguno)
    int seconds = 0;     // tiempo jugado

    bool hasName() const { return name[0] != '\0'; }
    // Devuelven false si no se puede leer / escribir (p. ej. no existe todavia).
    bool load(const char* path);
    bool save(const char* path) const;
};

}  // namespace app
