// Taquilla (parche, fase 8; docs/TAQUILLA.md): categorias de objetos cosmeticos y su
// catalogo: los objetos por defecto (el traje del jugador, el pico, el ala delta y el
// baile) y, desde la v0.13, los trajes de los personajes de la CPU, que se compran en la
// TIENDA con pavos (docs/TIENDA.md). Lo equipado y lo comprado se guarda en el perfil.
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
    int price;  // pavos en la tienda (0: gratis, se tiene desde el principio)
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
// Objetos de la tienda (los que tienen precio), indices del catalogo. Devuelve cuantos.
int shopItems(int* out, int max);
// Indice en el catalogo (o -1).
int cosmeticIndex(const Cosmetic& item);

}  // namespace app
