#include "game/inventory.h"

namespace game {
namespace {

const ItemDef kDefs[] = {
    // nombre            tipo                 rareza             pila  uso   vida  tope  escudo tope
    {"", ItemKind::Empty, Rarity::Common, 0, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f},
    {"SCAR", ItemKind::Weapon, Rarity::Legendary, 1, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f},
    {"Vendas", ItemKind::Consumable, Rarity::Common, 15, 3.5f, 15.0f, 75.0f, 0.0f, 0.0f},
    {"Botiquin", ItemKind::Consumable, Rarity::Uncommon, 3, 10.0f, 100.0f, 100.0f, 0.0f, 0.0f},
    {"Minipocion", ItemKind::Consumable, Rarity::Uncommon, 6, 2.0f, 0.0f, 0.0f, 25.0f, 50.0f},
    {"Pocion de escudo", ItemKind::Consumable, Rarity::Rare, 3, 5.0f, 0.0f, 0.0f, 50.0f, 100.0f},
};
static_assert(sizeof(kDefs) / sizeof(kDefs[0]) == static_cast<int>(ItemType::Count), "falta un objeto");

}  // namespace

const ItemDef& itemDef(ItemType type) {
    const int i = static_cast<int>(type);
    return kDefs[i >= 0 && i < static_cast<int>(ItemType::Count) ? i : 0];
}

int Materials::add(world::Resource r, int n) {
    int& a = amount[static_cast<int>(r)];
    const int added = a + n > kMax ? kMax - a : n;
    a += added;
    return added;
}

bool Materials::spend(world::Resource r, int n) {
    int& a = amount[static_cast<int>(r)];
    if (a < n) return false;
    a -= n;
    return true;
}

void Inventory::clear() {
    for (InventorySlot& s : m_slots) s = InventorySlot{};
    m_selected = 0;
    m_pickaxe = false;
}

int Inventory::add(ItemType type, int count, const WeaponConfig* weapon) {
    const ItemDef& def = itemDef(type);
    if (def.kind == ItemKind::Empty || count <= 0) return count;
    // Primero se apila en las ranuras del mismo tipo.
    if (def.kind == ItemKind::Consumable) {
        for (InventorySlot& s : m_slots) {
            if (s.type != type || s.count >= def.maxStack) continue;
            const int take = count < def.maxStack - s.count ? count : def.maxStack - s.count;
            s.count += take;
            count -= take;
            if (count == 0) return 0;
        }
    }
    for (InventorySlot& s : m_slots) {
        if (s.type != ItemType::None) continue;
        s.type = type;
        if (def.kind == ItemKind::Weapon) {
            s.count = 1;
            if (weapon) s.weapon.reset(*weapon);
            --count;
        } else {
            s.count = count < def.maxStack ? count : def.maxStack;
            count -= s.count;
        }
        if (count == 0) return 0;
    }
    return count;
}

void Inventory::select(int index) {
    if (index < 0 || index >= kSlots) return;
    m_selected = index;
    m_pickaxe = false;
}

bool Inventory::cycle(int direction) {
    const int step = direction < 0 ? kSlots - 1 : 1;
    for (int k = 1; k <= kSlots; ++k) {
        const int i = (m_selected + step * k) % kSlots;
        if (m_slots[i].type == ItemType::None) continue;
        const bool changed = m_pickaxe || i != m_selected;
        m_selected = i;
        m_pickaxe = false;
        return changed;
    }
    // Todo vacio: solo se guarda el pico.
    const bool changed = m_pickaxe;
    m_pickaxe = false;
    return changed;
}

ItemType Inventory::activeType() const { return m_pickaxe ? ItemType::None : m_slots[m_selected].type; }

Weapon* Inventory::activeWeapon() {
    if (m_pickaxe || itemDef(m_slots[m_selected].type).kind != ItemKind::Weapon) return nullptr;
    return &m_slots[m_selected].weapon;
}

const Weapon* Inventory::activeWeapon() const { return const_cast<Inventory*>(this)->activeWeapon(); }

const Weapon* Inventory::firstWeapon() const {
    for (const InventorySlot& s : m_slots) {
        if (itemDef(s.type).kind == ItemKind::Weapon) return &s.weapon;
    }
    return nullptr;
}

int Inventory::count(ItemType type) const {
    int n = 0;
    for (const InventorySlot& s : m_slots) {
        if (s.type == type) n += s.count;
    }
    return n;
}

void Inventory::consumeOne(int index) {
    if (index < 0 || index >= kSlots || m_slots[index].count <= 0) return;
    if (--m_slots[index].count == 0) m_slots[index] = InventorySlot{};
}

}  // namespace game
