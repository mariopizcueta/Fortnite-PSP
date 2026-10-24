#include "app/locker.h"

#include <string.h>

namespace app {
namespace {

// Catalogo. El primero de cada categoria es el de por defecto. Para anadir una skin: su
// paleta en game::palettes (character_model.*) y una fila aqui (ver docs/TAQUILLA.md).
const Cosmetic kCatalog[] = {
    {"traje_recluta", i18n::T::CosmeticRecruit, CosmeticSlot::Outfit, game::Rarity::Common, &game::palettes::kPlayer},
    {"pico_defecto", i18n::T::CosmeticPickaxe, CosmeticSlot::Pickaxe, game::Rarity::Common, nullptr},
    {"ala_defecto", i18n::T::CosmeticGlider, CosmeticSlot::Glider, game::Rarity::Common, nullptr},
    {"baile_defecto", i18n::T::CosmeticEmote, CosmeticSlot::Emote, game::Rarity::Common, nullptr},
};
constexpr int kCatalogSize = sizeof(kCatalog) / sizeof(kCatalog[0]);

}  // namespace

i18n::T cosmeticSlotName(CosmeticSlot slot) {
    switch (slot) {
        case CosmeticSlot::Outfit: return i18n::T::SlotOutfit;
        case CosmeticSlot::BackBling: return i18n::T::SlotBackBling;
        case CosmeticSlot::Pickaxe: return i18n::T::SlotPickaxe;
        case CosmeticSlot::Glider: return i18n::T::SlotGlider;
        case CosmeticSlot::Emote: return i18n::T::SlotEmote;
        case CosmeticSlot::Wrap: return i18n::T::SlotWrap;
        case CosmeticSlot::Count: break;
    }
    return i18n::T::None;
}

const char* cosmeticSlotKey(CosmeticSlot slot) {
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
