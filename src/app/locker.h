// Taquilla (parche, fase 8; docs/TAQUILLA.md): categorias de objetos cosmeticos y su
// catalogo. De momento solo estan los objetos por defecto (el traje del jugador, el pico,
// el ala delta y el baile): la personalizacion de skins llegara en una actualizacion. La
// estructura ya permite anadir objetos y equiparlos (se guarda en el perfil).
//
// Sin tienda, monedas ni compras: todo lo del catalogo esta disponible.
#pragma once

#include "core/i18n.h"
#include "game/character_model.h"
#include "game/inventory.h"

namespace app {

enum class CosmeticSlot : u8 { Outfit, BackBling, Pickaxe, Glider, Emote, Wrap, Count };
constexpr int kCosmeticSlotCount = static_cast<int>(CosmeticSlot::Count);

struct Cosmetic {
    const char* key;   // clave en perfil.txt (unica)
    i18n::T name;
    CosmeticSlot slot;
    game::Rarity rarity;
    // Trajes: la paleta del modelo del personaje (character_model.h). nullptr en el resto.
    const game::CharacterPalette* palette;
};

i18n::T cosmeticSlotName(CosmeticSlot slot);
// Clave de la categoria en perfil.txt (no cambia con el idioma).
const char* cosmeticSlotKey(CosmeticSlot slot);
int cosmeticCount();
const Cosmetic& cosmetic(int index);
// Objetos de una categoria (indices del catalogo). Devuelve cuantos.
int cosmeticsIn(CosmeticSlot slot, int* out, int max);
// Por clave (o nullptr). El de por defecto de una categoria (o nullptr si esta vacia).
const Cosmetic* findCosmetic(const char* key);
const Cosmetic* defaultCosmetic(CosmeticSlot slot);

}  // namespace app
