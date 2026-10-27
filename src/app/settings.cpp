#include "app/settings.h"

#include <pspctrl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/config.h"

namespace app {

const u32 kRemappableButtons[kRemappableCount] = {PSP_CTRL_CROSS, PSP_CTRL_CIRCLE,   PSP_CTRL_SQUARE,
                                                  PSP_CTRL_TRIANGLE, PSP_CTRL_RTRIGGER, PSP_CTRL_LTRIGGER,
                                                  PSP_CTRL_DOWN,  PSP_CTRL_UP};

namespace {

// Los de siempre: X salta, CIRCULO construye, CUADRADO recarga/recoge, TRIANGULO pico,
// R dispara, L apunta, cruceta abajo se agacha y arriba baila.
const u32 kDefaultButtons[kActionCount] = {PSP_CTRL_CROSS,    PSP_CTRL_CIRCLE,   PSP_CTRL_SQUARE, PSP_CTRL_TRIANGLE,
                                           PSP_CTRL_RTRIGGER, PSP_CTRL_LTRIGGER, PSP_CTRL_DOWN,   PSP_CTRL_UP};

const char* const kActionKeys[kActionCount] = {"saltar", "construir", "recargar", "pico",
                                               "disparar", "apuntar", "agacharse", "bailar"};

struct ButtonName {
    u32 mask;
    const char* key;
};
const ButtonName kButtonNames[] = {{PSP_CTRL_CROSS, "X"},          {PSP_CTRL_CIRCLE, "CIRCULO"},
                                   {PSP_CTRL_SQUARE, "CUADRADO"},  {PSP_CTRL_TRIANGLE, "TRIANGULO"},
                                   {PSP_CTRL_RTRIGGER, "R"},       {PSP_CTRL_LTRIGGER, "L"},
                                   {PSP_CTRL_DOWN, "ABAJO"},       {PSP_CTRL_UP, "ARRIBA"}};

const CheatDef kCheats[] = {
    {"modo_dios", i18n::T::CheatGod, i18n::T::CheatGodDesc, &game::Cheats::godMode},
    {"aimbot", i18n::T::CheatAimbot, i18n::T::CheatAimbotDesc, &game::Cheats::aimbot},
    {"wallhack", i18n::T::CheatWallhack, i18n::T::CheatWallhackDesc, &game::Cheats::wallhack},
    {"municion_infinita", i18n::T::CheatAmmo, i18n::T::CheatAmmoDesc, &game::Cheats::infiniteAmmo},
    {"sin_recarga", i18n::T::CheatNoReload, i18n::T::CheatNoReloadDesc, &game::Cheats::noReload},
    {"cadencia", i18n::T::CheatRapidFire, i18n::T::CheatRapidFireDesc, &game::Cheats::rapidFire},
    {"sin_dispersion", i18n::T::CheatNoSpread, i18n::T::CheatNoSpreadDesc, &game::Cheats::noSpread},
    {"un_tiro", i18n::T::CheatOneShot, i18n::T::CheatOneShotDesc, &game::Cheats::oneShot},
    {"balas_explosivas", i18n::T::CheatExplosive, i18n::T::CheatExplosiveDesc, &game::Cheats::explosiveAmmo},
    {"atravesar_paredes", i18n::T::CheatWallbang, i18n::T::CheatWallbangDesc, &game::Cheats::wallbang},
    {"volar", i18n::T::CheatFly, i18n::T::CheatFlyDesc, &game::Cheats::fly},
    {"supervelocidad", i18n::T::CheatSpeed, i18n::T::CheatSpeedDesc, &game::Cheats::superSpeed},
    {"supersalto", i18n::T::CheatJump, i18n::T::CheatJumpDesc, &game::Cheats::superJump},
    {"gravedad_lunar", i18n::T::CheatGravity, i18n::T::CheatGravityDesc, &game::Cheats::lowGravity},
    {"materiales_infinitos", i18n::T::CheatMaterials, i18n::T::CheatMaterialsDesc, &game::Cheats::infiniteMaterials},
    {"invisible", i18n::T::CheatInvisible, i18n::T::CheatInvisibleDesc, &game::Cheats::invisible},
    {"sin_tormenta", i18n::T::CheatNoStorm, i18n::T::CheatNoStormDesc, &game::Cheats::noStorm},
    {"bots_congelados", i18n::T::CheatFreeze, i18n::T::CheatFreezeDesc, &game::Cheats::freezeBots},
    {"fiesta", i18n::T::CheatParty, i18n::T::CheatPartyDesc, &game::Cheats::partyBots},
    {"radar_botin", i18n::T::CheatRadar, i18n::T::CheatRadarDesc, &game::Cheats::lootRadar},
    {"tiempo_bala", i18n::T::CheatSlowMo, i18n::T::CheatSlowMoDesc, &game::Cheats::slowMotion},
    {"llama_siempre", i18n::T::CheatLlama, i18n::T::CheatLlamaDesc, &game::Cheats::alwaysLlama},
    {"kit_legendario", i18n::T::CheatKit, i18n::T::CheatKitDesc, &game::Cheats::legendaryKit},
};
constexpr int kCheatCount = sizeof(kCheats) / sizeof(kCheats[0]);

bool readInt(const char* text, int lo, int hi, int* out) {
    char* end = nullptr;
    const long v = strtol(text, &end, 10);
    if (end == text || v < lo || v > hi) return false;
    *out = static_cast<int>(v);
    return true;
}

bool readBool(const char* text, bool* out) {
    int v = 0;
    if (!readInt(text, 0, 1, &v)) return false;
    *out = v != 0;
    return true;
}

void tempPath(const char* path, char* out, size_t size) {
    snprintf(out, size, "%s", path);
    char* dot = strrchr(out, '.');
    const size_t base = dot ? static_cast<size_t>(dot - out) : strlen(out);
    snprintf(out + base, size - base, ".tmp");
}

}  // namespace

int cheatCount() { return kCheatCount; }

const CheatDef& cheatDef(int index) { return kCheats[index >= 0 && index < kCheatCount ? index : 0]; }

const char* buttonKey(u32 mask) {
    for (const ButtonName& b : kButtonNames) {
        if (b.mask == mask) return b.key;
    }
    return "?";
}

u32 buttonFromKey(const char* key) {
    for (const ButtonName& b : kButtonNames) {
        if (strcmp(b.key, key) == 0) return b.mask;
    }
    return 0;
}

void Settings::resetControls() {
    for (int i = 0; i < kActionCount; ++i) buttons[i] = kDefaultButtons[i];
}

void Settings::assign(Action action, u32 button) {
    const int a = static_cast<int>(action);
    if (a < 0 || a >= kActionCount) return;
    bool allowed = false;
    for (u32 b : kRemappableButtons) allowed |= b == button;
    if (!allowed) return;
    // Quien tuviera ese boton se queda con el de esta accion: siempre uno por accion.
    for (int i = 0; i < kActionCount; ++i) {
        if (i != a && buttons[i] == button) buttons[i] = buttons[a];
    }
    buttons[a] = button;
}

bool Settings::controlsValid() const {
    for (int i = 0; i < kActionCount; ++i) {
        bool allowed = false;
        for (u32 b : kRemappableButtons) allowed |= b == buttons[i];
        if (!allowed) return false;
        for (int j = i + 1; j < kActionCount; ++j) {
            if (buttons[i] == buttons[j]) return false;
        }
    }
    return true;
}

float Settings::fogStart() const {
    static const float kStart[3] = {22.0f, config::kFogStart, 40.0f};
    return kStart[drawDistance >= 0 && drawDistance < 3 ? drawDistance : 1];
}

float Settings::fogEnd() const {
    static const float kEnd[3] = {70.0f, config::kFogEnd, 125.0f};
    return kEnd[drawDistance >= 0 && drawDistance < 3 ? drawDistance : 1];
}

float Settings::lodDistance() const {
    static const float kLod[3] = {6.0f, 10.0f, 18.0f};
    return kLod[characterDetail >= 0 && characterDetail < 3 ? characterDetail : 1];
}

bool Settings::load(const char* path) {
    FILE* f = fopen(path, "r");
    if (!f) {
        char tmp[300];
        tempPath(path, tmp, sizeof(tmp));
        f = fopen(tmp, "r");
        if (!f) return false;
    }
    *this = Settings{};
    char line[96];
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = '\0';
        char* eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        const char* v = eq + 1;
        // Cada valor solo se acepta dentro de su rango; si no, queda el de por defecto.
        if (strcmp(line, "idioma") == 0) {
            if (strcmp(v, "en") == 0) language = i18n::Lang::En;
            else if (strcmp(v, "es") == 0) language = i18n::Lang::Es;
        } else if (strcmp(line, "distancia") == 0) {
            readInt(v, 0, 2, &drawDistance);
        } else if (strcmp(line, "detalle") == 0) {
            readInt(v, 0, 2, &characterDetail);
        } else if (strcmp(line, "numeros_dano") == 0) {
            readBool(v, &damageNumbers);
        } else if (strcmp(line, "trazadoras") == 0) {
            readBool(v, &tracers);
        } else if (strcmp(line, "fps") == 0) {
            readBool(v, &showFps);
        } else if (strcmp(line, "vsync") == 0) {
            readBool(v, &vsync);
        } else if (strcmp(line, "sensibilidad_camara") == 0) {
            readInt(v, 1, 10, &cameraSensitivity);
        } else if (strcmp(line, "sensibilidad_mirilla") == 0) {
            readInt(v, 1, 10, &scopeSensitivity);
        } else if (strcmp(line, "zona_muerta") == 0) {
            readInt(v, 10, 35, &deadzone);
        } else if (strcmp(line, "invertir_y") == 0) {
            readBool(v, &invertY);
        } else if (strcmp(line, "admin") == 0) {
            readBool(v, &admin);
        } else if (strncmp(line, "truco_", 6) == 0) {
            for (const CheatDef& c : kCheats) {
                if (strcmp(line + 6, c.key) == 0) readBool(v, &(cheats.*c.flag));
            }
        } else if (strncmp(line, "boton_", 6) == 0) {
            for (int i = 0; i < kActionCount; ++i) {
                if (strcmp(line + 6, kActionKeys[i]) != 0) continue;
                const u32 b = buttonFromKey(v);
                if (b) buttons[i] = b;
            }
        }
    }
    fclose(f);
    // Botones repetidos o desconocidos (fichero editado a mano o roto): los de siempre.
    if (!controlsValid()) resetControls();
    // Sin el menu de administrador desbloqueado no hay trucos (fichero editado a mano).
    if (!admin) cheats = game::Cheats{};
    return true;
}

bool Settings::save(const char* path) const {
    auto write = [&](const char* file) {
        FILE* f = fopen(file, "w");
        if (!f) return false;
        bool ok = fprintf(f, "idioma=%s\n", language == i18n::Lang::En ? "en" : "es") > 0;
        ok &= fprintf(f, "distancia=%d\ndetalle=%d\n", drawDistance, characterDetail) > 0;
        ok &= fprintf(f, "numeros_dano=%d\ntrazadoras=%d\nfps=%d\nvsync=%d\n", damageNumbers ? 1 : 0, tracers ? 1 : 0,
                      showFps ? 1 : 0, vsync ? 1 : 0) > 0;
        ok &= fprintf(f, "sensibilidad_camara=%d\nsensibilidad_mirilla=%d\nzona_muerta=%d\ninvertir_y=%d\n",
                      cameraSensitivity, scopeSensitivity, deadzone, invertY ? 1 : 0) > 0;
        for (int i = 0; i < kActionCount; ++i) ok &= fprintf(f, "boton_%s=%s\n", kActionKeys[i], buttonKey(buttons[i])) > 0;
        if (admin) {
            ok &= fprintf(f, "admin=1\n") > 0;
            for (const CheatDef& c : kCheats) {
                if (cheats.*c.flag) ok &= fprintf(f, "truco_%s=1\n", c.key) > 0;
            }
        }
        ok &= fflush(f) == 0;
        ok &= fclose(f) == 0;
        return ok;
    };
    char tmp[300];
    tempPath(path, tmp, sizeof(tmp));
    if (!write(tmp)) {
        remove(tmp);
        return false;
    }
    remove(path);
    if (rename(tmp, path) == 0) return true;
    const bool ok = write(path);
    if (ok) remove(tmp);
    return ok;
}

}  // namespace app
