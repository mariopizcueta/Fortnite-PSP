// Inventario del jugador: 5 ranuras (armas y consumibles apilables), el pico aparte y los
// materiales de construccion.
//
// Cada ranura de arma guarda su propio Weapon (cargador, reserva, recarga), asi que el
// sistema de armas de la fase 5 se reutiliza tal cual. Los consumibles siguen los valores
// de Fortnite: vendas (+15 de vida hasta 75), botiquin (vida al maximo), minipocion (+25
// de escudo hasta 50) y pocion de escudo (+50 hasta 100).
#pragma once

#include <psptypes.h>

#include "game/weapon.h"
#include "world/harvest.h"

namespace game {

enum class ItemType : u8 { None, Scar, Bandage, Medkit, SmallShield, ShieldPotion, Count };
enum class ItemKind : u8 { Empty, Weapon, Consumable };
// Rareza (color del fondo de la ranura).
enum class Rarity : u8 { Common, Uncommon, Rare, Epic, Legendary };

struct ItemDef {
    const char* name;
    ItemKind kind;
    Rarity rarity;
    int maxStack;
    float useSeconds;  // consumibles: tiempo de uso
    float health;      // vida que da
    float healthCap;   // ...sin pasar de este valor
    float shield;      // escudo que da
    float shieldCap;
};

const ItemDef& itemDef(ItemType type);

struct InventorySlot {
    ItemType type = ItemType::None;
    int count = 0;
    Weapon weapon;  // solo si type es un arma
};

// Madera, piedra y metal (mismo orden que world::Resource).
struct Materials {
    static constexpr int kMax = 999;
    int amount[static_cast<int>(world::Resource::Count)] = {0, 0, 0};

    int get(world::Resource r) const { return amount[static_cast<int>(r)]; }
    // Suma sin pasar de kMax. Devuelve lo que se ha sumado de verdad.
    int add(world::Resource r, int n);
    bool spend(world::Resource r, int n);
};

class Inventory {
public:
    static constexpr int kSlots = 5;

    void clear();
    // Anade `count` unidades (apila en ranuras del mismo tipo y luego usa huecos libres).
    // Las armas ocupan una ranura cada una y se crean con `weapon`. Devuelve lo que no cupo.
    int add(ItemType type, int count, const WeaponConfig* weapon = nullptr);

    const InventorySlot& slot(int i) const { return m_slots[i]; }
    InventorySlot& mutableSlot(int i) { return m_slots[i]; }

    // Ranura seleccionada (0..4) y si se lleva el pico en la mano en su lugar.
    int selected() const { return m_selected; }
    bool pickaxe() const { return m_pickaxe; }
    void select(int index);
    void setPickaxe(bool on) { m_pickaxe = on; }
    // Siguiente / anterior ranura con algo (salta las vacias). Si se lleva el pico, lo
    // guarda. Devuelve true si cambia lo que hay en la mano.
    bool cycle(int direction);

    // Lo que hay en la mano (None con el pico o con una ranura vacia).
    ItemType activeType() const;
    Weapon* activeWeapon();
    const Weapon* activeWeapon() const;
    // Primera arma del inventario (para el HUD cuando se lleva otra cosa), o nullptr.
    const Weapon* firstWeapon() const;
    int count(ItemType type) const;
    // Gasta una unidad de la ranura; si se vacia, queda libre.
    void consumeOne(int index);

private:
    InventorySlot m_slots[kSlots];
    int m_selected = 0;
    bool m_pickaxe = false;
};

}  // namespace game
