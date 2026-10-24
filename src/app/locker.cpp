#include "app/locker.h"

#include <string.h>

namespace app {
namespace {

// Catalogo. El primero de cada categoria es el de por defecto. Para anadir una skin: su
// paleta en game::palettes (character_model.*) y una fila aqui (ver docs/TAQUILLA.md).
const Cosmetic kCatalog[] = {
    {"traje_recluta", "Recluta", CosmeticSlot::Outfit, game::Rarity::Common, &game::palettes::kPlayer},
    {"pico_defecto", "Pico por defecto", CosmeticSlot::Pickaxe, game::Rarity::Common, nullptr},
    {"ala_defecto", "Ala delta por defecto", CosmeticSlot::Glider, game::Rarity::Common, nullptr},
    {"baile_defecto", "Baile por defecto", CosmeticSlot::Emote, game::Rarity::Common, nullptr},
};
constexpr int kCatalogSize = sizeof(kCatalog) / sizeof(kCatalog[0]);

}  // namespace

const char* cosmeticSlotName(CosmeticSlot slot) {
    switch (slot) {
        case CosmeticSlot::Outfit: return "Traje";
        case CosmeticSlot::BackBling: return "Mochila";
        case CosmeticSlot::Pickaxe: return "Pico";
        case CosmeticSlot::Glider: return "Ala delta";
        case CosmeticSlot::Emote: return "Baile";
        case CosmeticSlot::Wrap: return "Envoltorio";
        case CosmeticSlot::Count: break;
    }
    return "";
}

int cosmeticCount() { return kCatalogSize; }

const Cosmetic& cosmetic(int index) { return kCatalog[index >= 0 && index < kCatalogSize ? index : 0]; }

int cosmeticsIn(CosmeticSlot slot, int* out, int max) {
    int n = 0;
    for (int i = 0; i < kCatalogSize && n < max; ++i) {
        if (kCatalog[i].slot == slot) out[n++] = i;
    }
    return n;
}

const Cosmetic* findCosmetic(const char* key) {
    if (!key || !key[0]) return nullptr;
    for (const Cosmetic& c : kCatalog) {
        if (strcmp(c.key, key) == 0) return &c;
    }
    return nullptr;
}

const Cosmetic* defaultCosmetic(CosmeticSlot slot) {
    for (const Cosmetic& c : kCatalog) {
        if (c.slot == slot) return &c;
    }
    return nullptr;
}

}  // namespace app
