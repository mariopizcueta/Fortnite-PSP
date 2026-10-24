#include "app/profile.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

namespace app {

namespace {

struct Field {
    const char* key;
    int Profile::*value;
};

const Field kFields[] = {
    {"partidas", &Profile::matches},
    {"victorias", &Profile::wins},
    {"bajas", &Profile::kills},
    {"muertes", &Profile::deaths},
    {"a_la_cabeza", &Profile::headshots},
    {"max_bajas", &Profile::mostKills},
    {"mejor_puesto", &Profile::bestPlace},
    {"segundos", &Profile::seconds},
    // Parche (fase 8).
    {"top5", &Profile::top5},
    {"cofres", &Profile::chestsOpened},
    {"suministros", &Profile::airdropsOpened},
    {"llamas", &Profile::llamasOpened},
    {"piezas", &Profile::piecesBuilt},
    {"materiales_pico", &Profile::materialsHarvested},
    {"bajas_trampa", &Profile::trapKills},
    {"bajas_explosivo", &Profile::explosiveKills},
    {"curativos", &Profile::itemsUsed},
};

// Caracteres permitidos en el nombre (los del teclado en pantalla).
bool validNameChar(char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_' ||
           c == '.' || c == '!';
}

// Entero no negativo y razonable (un fichero corrupto no da valores absurdos).
bool parseCount(const char* text, int* out) {
    char* end = nullptr;
    const long v = strtol(text, &end, 10);
    if (end == text || v < 0 || v > 100000000L) return false;
    *out = static_cast<int>(v);
    return true;
}

bool plausibleDate(int d) { return d == 0 || (d >= 20000101 && d <= 21001231); }

void tempPath(const char* path, char* out, size_t size) {
    snprintf(out, size, "%s", path);
    char* dot = strrchr(out, '.');
    const size_t base = dot ? static_cast<size_t>(dot - out) : strlen(out);
    snprintf(out + base, size - base, ".tmp");
}

}  // namespace

int todayDate() {
    const time_t now = time(nullptr);
    if (now <= 0) return 0;
    const struct tm* t = localtime(&now);
    if (!t) return 0;
    const int date = (t->tm_year + 1900) * 10000 + (t->tm_mon + 1) * 100 + t->tm_mday;
    return plausibleDate(date) ? date : 0;
}

int Profile::stat(Stat s) const { return const_cast<Profile*>(this)->mutableStat(s); }

int& Profile::mutableStat(Stat s) {
    switch (s) {
        case Stat::Matches: return matches;
        case Stat::Wins: return wins;
        case Stat::Kills: return kills;
        case Stat::Deaths: return deaths;
        case Stat::Headshots: return headshots;
        case Stat::MostKills: return mostKills;
        case Stat::BestPlaceTop5: return top5;
        case Stat::Seconds: return seconds;
        case Stat::ChestsOpened: return chestsOpened;
        case Stat::AirdropsOpened: return airdropsOpened;
        case Stat::LlamasOpened: return llamasOpened;
        case Stat::PiecesBuilt: return piecesBuilt;
        case Stat::MaterialsHarvested: return materialsHarvested;
        case Stat::TrapKills: return trapKills;
        case Stat::ExplosiveKills: return explosiveKills;
        case Stat::ItemsUsed: return itemsUsed;
        case Stat::Count: break;
    }
    return matches;
}

int Profile::unlockReached(int date, AchievementId* out, int max) {
    int n = 0;
    for (int i = 0; i < kAchievementCount; ++i) {
        AchievementState& a = achievements[i];
        const AchievementDef& def = achievementDef(static_cast<AchievementId>(i));
        if (a.unlocked || stat(def.stat) < def.goal) continue;
        a.unlocked = true;  // una sola vez: ya no se vuelve a registrar
        a.date = date;
        if (n < max) out[n] = static_cast<AchievementId>(i);
        ++n;
    }
    return n;
}

int Profile::unlockedCount() const {
    int n = 0;
    for (const AchievementState& a : achievements) n += a.unlocked ? 1 : 0;
    return n;
}

const Cosmetic* Profile::equippedIn(CosmeticSlot slot) const {
    const Cosmetic* c = findCosmetic(equipped[static_cast<int>(slot)]);
    return c && c->slot == slot ? c : defaultCosmetic(slot);
}

void Profile::equip(const Cosmetic& item) {
    snprintf(equipped[static_cast<int>(item.slot)], kMaxKey, "%s", item.key);
}

bool Profile::load(const char* path) {
    FILE* f = fopen(path, "r");
    if (!f) {
        // Un guardado que se corto antes de cambiar el temporal por el bueno.
        char tmp[300];
        tempPath(path, tmp, sizeof(tmp));
        f = fopen(tmp, "r");
        if (!f) return false;
    }
    *this = Profile{};
    char line[96];
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = '\0';
        char* eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        const char* value = eq + 1;
        int number = 0;
        if (strcmp(line, "nombre") == 0) {
            // Solo los caracteres del teclado del juego; si no, el nombre se vuelve a pedir.
            char clean[kMaxName + 1];
            int n = 0;
            for (const char* c = value; *c && n < kMaxName; ++c) {
                if (validNameChar(*c)) clean[n++] = *c;
            }
            clean[n] = '\0';
            snprintf(name, sizeof(name), "%s", clean);
            continue;
        }
        if (strncmp(line, "logro_", 6) == 0) {
            for (int i = 0; i < kAchievementCount; ++i) {
                if (strcmp(line + 6, achievementDef(static_cast<AchievementId>(i)).key) != 0) continue;
                if (parseCount(value, &number) && (number == 1 || plausibleDate(number))) {
                    achievements[i].unlocked = true;
                    achievements[i].date = number == 1 ? 0 : number;
                }
            }
            continue;
        }
        if (strncmp(line, "equipado_", 9) == 0) {
            const Cosmetic* c = findCosmetic(value);
            for (int s = 0; s < kCosmeticSlotCount; ++s) {
                if (c && strcmp(line + 9, cosmeticSlotKey(static_cast<CosmeticSlot>(s))) == 0 &&
                    c->slot == static_cast<CosmeticSlot>(s)) {
                    equip(*c);
                }
            }
            continue;
        }
        for (const Field& field : kFields) {
            if (strcmp(line, field.key) == 0 && parseCount(value, &number)) this->*field.value = number;
        }
    }
    fclose(f);
    if (mostKills > kills) mostKills = kills;
    if (wins > matches) wins = matches;
    return true;
}

bool Profile::save(const char* path) const {
    auto write = [&](const char* file) {
        FILE* f = fopen(file, "w");
        if (!f) return false;
        bool ok = fprintf(f, "nombre=%s\n", name) > 0;
        for (const Field& field : kFields) ok &= fprintf(f, "%s=%d\n", field.key, this->*field.value) > 0;
        for (int i = 0; i < kAchievementCount; ++i) {
            if (!achievements[i].unlocked) continue;
            ok &= fprintf(f, "logro_%s=%d\n", achievementDef(static_cast<AchievementId>(i)).key,
                          achievements[i].date > 0 ? achievements[i].date : 1) > 0;
        }
        for (int s = 0; s < kCosmeticSlotCount; ++s) {
            if (!equipped[s][0]) continue;
            ok &= fprintf(f, "equipado_%s=%s\n", cosmeticSlotKey(static_cast<CosmeticSlot>(s)), equipped[s]) > 0;
        }
        ok &= fflush(f) == 0;
        ok &= fclose(f) == 0;
        return ok;
    };
    // Primero el temporal entero; despues se cambia por el bueno. En la Memory Stick
    // renombrar sobre un fichero que existe falla: se borra antes. Si se corta justo ahi,
    // al cargar se usa el temporal.
    char tmp[300];
    tempPath(path, tmp, sizeof(tmp));
    if (!write(tmp)) {
        remove(tmp);
        return false;
    }
    remove(path);
    if (rename(tmp, path) == 0) return true;
    // Si no se puede renombrar, se escribe directamente (y se quita el temporal).
    const bool ok = write(path);
    if (ok) remove(tmp);
    return ok;
}

bool Profile::erase(const char* path) {
    char tmp[300];
    tempPath(path, tmp, sizeof(tmp));
    remove(path);
    remove(tmp);
    // Comprobacion: no queda ninguno de los dos.
    FILE* a = fopen(path, "r");
    FILE* b = fopen(tmp, "r");
    const bool gone = !a && !b;
    if (a) fclose(a);
    if (b) fclose(b);
    return gone;
}

}  // namespace app
