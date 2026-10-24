// Inventario del jugador: 5 ranuras (armas y consumibles), el pico aparte, la municion por
// tipo y los materiales de construccion.
//
// Cada ranura de arma guarda su propio Weapon (cargador y recarga); la reserva es la
// municion comun del inventario (ligera, media, pesada, cartuchos), como en Fortnite.
// Cada objeto tiene su rareza: en las armas cambia el dano, la recarga y el nombre (el
// fusil de asalto gris, verde o azul es el fusil generico; el epico o legendario, la SCAR).
// Los consumibles siguen los valores de Fortnite: vendas (+15 de vida hasta 75), botiquin
// (vida al maximo), minipocion (+25 de escudo hasta 50) y pocion de escudo (+50 hasta 100).
// La granada es un arrojadizo: ocupa una ranura como un consumible y R la lanza.
//
// Parche (fase 8): armas de Recursos/Weapons (fusil de rafagas, fusil con mira, subfusil,
// escopeta tactica, pistola, revolver, francotirador semiautomatico, lanzacohetes,
// lanzagranadas y granada). Ver docs/BOTIN.md.
#pragma once

#include <psptypes.h>

#include "game/weapon.h"
#include "world/harvest.h"

namespace game {

enum class ItemType : u8 {
    None, AssaultRifle, Shotgun, Smg, Sniper, Bandage, Medkit, SmallShield, ShieldPotion,
    // Parche (fase 8).
    BurstRifle, ScopedRifle, SubmachineGun, TacticalShotgun, Pistol, Revolver, SemiAutoSniper,
    RocketLauncher, GrenadeLauncher, Grenade,
    Count
};
enum class ItemKind : u8 { Empty, Weapon, Consumable, Throwable };
// Rareza (color del fondo de la ranura y del haz de luz del botin).
enum class Rarity : u8 { Common, Uncommon, Rare, Epic, Legendary, Count };
// Municion comun por tipo (Rockets: lanzacohetes y lanzagranadas).
enum class AmmoType : u8 { Light, Medium, Heavy, Shells, Rockets, Count };
// Familia de un arma: sonido, postura y preferencias de los bots.
enum class WeaponClass : u8 { None, Rifle, Shotgun, Smg, Sniper, Pistol, Launcher };

struct ItemDef {
    const char* name;
    ItemKind kind;
    Rarity rarity;     // rareza habitual (los consumibles tienen una fija)
    int maxStack;
    float useSeconds;  // consumibles: tiempo de uso
    float health;      // vida que da
    float healthCap;   // ...sin pasar de este valor
    float shield;      // escudo que da
    float shieldCap;
};

const ItemDef& itemDef(ItemType type);
const char* rarityName(Rarity rarity);
const char* ammoName(AmmoType type);
bool isWeapon(ItemType type);
WeaponClass weaponClass(ItemType type);
// Armas con mirilla (L mira por ella): francotiradores y fusil con mira.
bool hasScope(ItemType type);
// Estadisticas de un arma segun su rareza (dano, recarga, nombre). Ver docs/BOTIN.md.
WeaponConfig weaponConfig(ItemType type, Rarity rarity);
// La granada de mano: explota como las del lanzagranadas (dano, radio, espoleta).
WeaponConfig grenadeConfig();
// Rarezas en las que existe cada arma (p. ej. el francotirador de cerrojo, de rara arriba).
bool weaponHasRarity(ItemType type, Rarity rarity);
// Municion que trae un arma o una caja (por recogida).
int ammoPerPickup(AmmoType type);

struct InventorySlot {
    ItemType type = ItemType::None;
    Rarity rarity = Rarity::Common;
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
    static constexpr int kMaxAmmo = 999;

    void clear();
    // Anade `count` unidades (apila consumibles del mismo tipo y luego usa huecos libres;
    // cada arma ocupa una ranura). `loaded`: balas en el cargador de un arma (-1: lleno; un
    // arma soltada conserva las suyas y no se duplica municion). Devuelve lo que no cupo.
    int add(ItemType type, Rarity rarity, int count, int loaded = -1);
    // Cuantas unidades cabrian sin sustituir nada (apilando o en huecos libres).
    int room(ItemType type) const;
    // Coloca un objeto en la ranura `index` sustituyendo lo que hubiera; deja en `dropped`
    // lo que se ha soltado (para dejarlo en el suelo). Devuelve las unidades que caben
    // (un arma: 1; un consumible: hasta su pila maxima).
    int place(int index, ItemType type, Rarity rarity, int count, int loaded, InventorySlot* dropped);
    // Igual, en la ranura seleccionada.
    int replaceSelected(ItemType type, Rarity rarity, int count, InventorySlot* dropped, int loaded = -1) {
        return place(m_selected, type, rarity, count, loaded, dropped);
    }
    // Quita y devuelve el contenido de una ranura.
    InventorySlot take(int index);

    const InventorySlot& slot(int i) const { return m_slots[i]; }
    InventorySlot& mutableSlot(int i) { return m_slots[i]; }
    bool full() const;

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

    // Municion comun por tipo.
    int ammo(AmmoType type) const { return m_ammo[static_cast<int>(type)]; }
    int addAmmo(AmmoType type, int n);
    void setAmmo(AmmoType type, int n) { m_ammo[static_cast<int>(type)] = n; }

private:
    InventorySlot m_slots[kSlots];
    int m_ammo[static_cast<int>(AmmoType::Count)] = {};
    int m_selected = 0;
    bool m_pickaxe = false;
};

}  // namespace game
