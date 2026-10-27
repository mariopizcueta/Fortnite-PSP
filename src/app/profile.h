// Perfil del jugador: nombre (se pide la primera vez), estadisticas de todas sus partidas,
// logros (parche, fase 8) y lo equipado en la taquilla. Se guarda en texto plano
// ("clave=valor") en perfil.txt, junto al EBOOT (ms0:/PSP/GAME/FortnitePSP/perfil.txt), y
// se carga al arrancar.
//
// Guardado seguro: se escribe primero perfil.tmp y despues se cambia por perfil.txt, asi un
// apagado a mitad de escritura no deja el perfil vacio (al cargar, si falta perfil.txt se
// usa perfil.tmp). Al cargar se valida todo: valores negativos o fuera de rango y claves
// desconocidas se ignoran y quedan los valores por defecto.
#pragma once

#include <psptypes.h>

#include "app/achievements.h"
#include "app/locker.h"

namespace app {

struct Profile {
    static constexpr int kMaxName = 12;
    static constexpr int kMaxKey = 24;
    char name[kMaxName + 1] = "";
    int matches = 0;     // partidas jugadas (terminadas o abandonadas)
    int wins = 0;        // victorias magistrales
    int kills = 0;       // bajas en total
    int deaths = 0;      // veces eliminado
    int headshots = 0;   // impactos en la cabeza
    int mostKills = 0;   // mas bajas en una partida
    int bestPlace = 0;   // mejor puesto (0 = aun ninguno)
    int seconds = 0;     // tiempo jugado
    // Parche (fase 8): contadores de los logros.
    int top5 = 0;               // partidas terminadas entre los 5 primeros
    int chestsOpened = 0;
    int airdropsOpened = 0;
    int llamasOpened = 0;
    int piecesBuilt = 0;
    int materialsHarvested = 0;
    int trapKills = 0;
    int explosiveKills = 0;
    int itemsUsed = 0;
    AchievementState achievements[kAchievementCount];
    // v0.13: pavos y trajes comprados en la tienda (bit = indice del catalogo).
    int vbucks = 0;
    u32 owned = 0;
    // Taquilla: clave del objeto equipado en cada categoria ("" = el de por defecto).
    char equipped[kCosmeticSlotCount][kMaxKey] = {};

    bool hasName() const { return name[0] != '\0'; }
    int stat(Stat s) const;
    int& mutableStat(Stat s);
    // Desbloquea los logros cuya meta se ha alcanzado (con la fecha AAAAMMDD). Deja en
    // `out` los nuevos (como mucho `max`) y devuelve cuantos.
    int unlockReached(int date, AchievementId* out, int max);
    int unlockedCount() const;
    // Logros completados cuya recompensa aun no se ha recogido; recoger uno suma sus pavos.
    int claimableCount() const;
    bool claim(AchievementId id);
    // Tienda: lo que se tiene (lo gratis siempre) y comprar (false si no hay pavos).
    bool owns(const Cosmetic& item) const;
    bool buy(const Cosmetic& item);
    // Lo equipado en una categoria (o el objeto por defecto de la categoria).
    const Cosmetic* equippedIn(CosmeticSlot slot) const;
    void equip(const Cosmetic& item);

    // Devuelven false si no se puede leer / escribir (p. ej. no existe todavia).
    bool load(const char* path);
    bool save(const char* path) const;
    // Borra el perfil guardado (perfil.txt y su temporal). true si no queda ninguno.
    static bool erase(const char* path);
};

// Fecha de hoy como AAAAMMDD (reloj de la consola), o 0 si no se sabe.
int todayDate();

}  // namespace app
