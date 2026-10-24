#include "app/profile.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace app {

namespace {

struct Field {
    const char* key;
    int Profile::*value;
};

const Field kFields[] = {
    {"partidas", &Profile::matches}, {"victorias", &Profile::wins},       {"bajas", &Profile::kills},
    {"muertes", &Profile::deaths},   {"a_la_cabeza", &Profile::headshots}, {"max_bajas", &Profile::mostKills},
    {"mejor_puesto", &Profile::bestPlace}, {"segundos", &Profile::seconds},
};

}  // namespace

bool Profile::load(const char* path) {
    FILE* f = fopen(path, "r");
    if (!f) return false;
    *this = Profile{};
    char line[96];
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = '\0';
        char* eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        const char* value = eq + 1;
        if (strcmp(line, "nombre") == 0) {
            strncpy(name, value, kMaxName);
            name[kMaxName] = '\0';
            continue;
        }
        for (const Field& field : kFields) {
            if (strcmp(line, field.key) == 0) this->*field.value = atoi(value);
        }
    }
    fclose(f);
    return true;
}

bool Profile::save(const char* path) const {
    FILE* f = fopen(path, "w");
    if (!f) return false;
    fprintf(f, "nombre=%s\n", name);
    for (const Field& field : kFields) fprintf(f, "%s=%d\n", field.key, this->*field.value);
    const bool ok = fflush(f) == 0;
    fclose(f);
    return ok;
}

}  // namespace app
