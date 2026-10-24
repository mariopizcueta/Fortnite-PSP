#include "game/inventory.h"

namespace game {
namespace {

const ItemDef kDefs[] = {
    // nombre            tipo                 rareza             pila  uso   vida  tope  escudo tope
    {"", ItemKind::Empty, Rarity::Common, 0, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f},
    {"Fusil de asalto", ItemKind::Weapon, Rarity::Common, 1, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f},
    {"Escopeta de corredera", ItemKind::Weapon, Rarity::Uncommon, 1, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f},
    {"Subfusil con silenciador", ItemKind::Weapon, Rarity::Common, 1, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f},
    {"Francotirador de cerrojo", ItemKind::Weapon, Rarity::Rare, 1, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f},
    {"Vendas", ItemKind::Consumable, Rarity::Common, 15, 3.5f, 15.0f, 75.0f, 0.0f, 0.0f},
    {"Botiquin", ItemKind::Consumable, Rarity::Uncommon, 3, 10.0f, 100.0f, 100.0f, 0.0f, 0.0f},
    {"Minipocion", ItemKind::Consumable, Rarity::Uncommon, 6, 2.0f, 0.0f, 0.0f, 25.0f, 50.0f},
    {"Pocion de escudo", ItemKind::Consumable, Rarity::Rare, 3, 5.0f, 0.0f, 0.0f, 50.0f, 100.0f},
};
static_assert(sizeof(kDefs) / sizeof(kDefs[0]) == static_cast<int>(ItemType::Count), "falta un objeto");

constexpr int kRarities = static_cast<int>(Rarity::Count);

// Valores de Fortnite (capitulo 1) por rareza: comun, poco comun, rara, epica, legendaria.
// 0 = el arma no existe en esa rareza.
struct WeaponTable {
    float damage[kRarities];
    float reload[kRarities];
};
constexpr WeaponTable kAssaultRifle{{30, 31, 33, 35, 36}, {2.7f, 2.6f, 2.5f, 2.3f, 2.2f}};
constexpr WeaponTable kShotgun{{0, 90, 95, 100, 105}, {0, 4.8f, 4.6f, 4.4f, 4.2f}};
constexpr WeaponTable kSmg{{17, 18, 19, 20, 21}, {2.3f, 2.2f, 2.1f, 2.0f, 1.9f}};
constexpr WeaponTable kSniper{{0, 0, 105, 110, 116}, {0, 0, 3.1f, 2.9f, 2.7f}};

const WeaponTable* tableFor(ItemType type) {
    switch (type) {
        case ItemType::AssaultRifle: return &kAssaultRifle;
        case ItemType::Shotgun: return &kShotgun;
        case ItemType::Smg: return &kSmg;
        case ItemType::Sniper: return &kSniper;
        default: return nullptr;
    }
}

}  // namespace

const ItemDef& itemDef(ItemType type) {
    const int i = static_cast<int>(type);
    return kDefs[i >= 0 && i < static_cast<int>(ItemType::Count) ? i : 0];
}

const char* rarityName(Rarity rarity) {
    switch (rarity) {
        case Rarity::Common: return "Comun";
        case Rarity::Uncommon: return "Poco comun";
        case Rarity::Rare: return "Rara";
        case Rarity::Epic: return "Epica";
        case Rarity::Legendary: return "Legendaria";
        case Rarity::Count: break;
    }
    return "";
}

const char* ammoName(AmmoType type) {
    switch (type) {
        case AmmoType::Light: return "Ligera";
        case AmmoType::Medium: return "Media";
        case AmmoType::Heavy: return "Pesada";
        case AmmoType::Shells: return "Cartuchos";
        case AmmoType::Count: break;
    }
    return "";
}

bool isWeapon(ItemType type) { return itemDef(type).kind == ItemKind::Weapon; }

bool weaponHasRarity(ItemType type, Rarity rarity) {
    const WeaponTable* t = tableFor(type);
    return t && rarity < Rarity::Count && t->damage[static_cast<int>(rarity)] > 0.0f;
}

int ammoPerPickup(AmmoType type) {
    switch (type) {
        case AmmoType::Light: return 30;
        case AmmoType::Medium: return 20;
        case AmmoType::Heavy: return 6;
        case AmmoType::Shells: return 8;
        case AmmoType::Count: break;
    }
    return 0;
}

WeaponConfig weaponConfig(ItemType type, Rarity rarity) {
    WeaponConfig w;
    const WeaponTable* t = tableFor(type);
    if (!t) return w;
    int r = static_cast<int>(rarity);
    while (r > 0 && t->damage[r] <= 0.0f) --r;  // rareza inexistente: la mas cercana por debajo
    while (r < kRarities - 1 && t->damage[r] <= 0.0f) ++r;
    w.rarity = rarityName(static_cast<Rarity>(r));
    w.damage = t->damage[r];
    w.reloadTime = t->reload[r];
    w.reserveAmmo = 0;  // la reserva es la municion del inventario
    switch (type) {
        case ItemType::AssaultRifle:
            w.name = r >= static_cast<int>(Rarity::Epic) ? "SCAR" : "Fusil de asalto";
            w.fireInterval = 1.0f / 5.5f;
            w.magazineSize = 30;
            w.range = 80.0f;
            w.ammoType = static_cast<int>(AmmoType::Medium);
            w.hipSpread = 2.4f;
            w.aimSpread = 0.7f;
            w.recoil = 0.55f;
            w.maxRecoil = 3.0f;
            w.crouchSpreadScale = 0.6f;
            break;
        case ItemType::Shotgun:
            w.name = "Escopeta";
            w.fireInterval = 0.9f;
            w.magazineSize = 5;
            w.range = 28.0f;
            w.ammoType = static_cast<int>(AmmoType::Shells);
            w.pellets = 10;
            w.hipSpread = 6.0f;
            w.aimSpread = 4.5f;
            w.falloffStart = 8.0f;
            w.falloffEnd = 25.0f;
            w.falloffMin = 0.35f;
            w.recoil = 1.2f;
            w.maxRecoil = 1.2f;
            w.recoilRecovery = 2.0f;
            w.crouchSpreadScale = 0.85f;  // el cono de perdigones solo se cierra un poco
            break;
        case ItemType::Smg:
            w.name = "Subfusil";
            w.fireInterval = 0.1f;
            w.magazineSize = 30;
            w.range = 45.0f;
            w.ammoType = static_cast<int>(AmmoType::Light);
            w.hipSpread = 3.4f;
            w.aimSpread = 1.8f;
            w.falloffStart = 20.0f;
            w.falloffEnd = 45.0f;
            w.falloffMin = 0.6f;
            w.recoil = 0.3f;
            w.maxRecoil = 3.5f;
            w.recoilRecovery = 9.0f;
            w.crouchSpreadScale = 0.7f;
            break;
        case ItemType::Sniper:
            w.name = "Francotirador";
            w.fireInterval = 2.6f;  // accion de cerrojo
            w.magazineSize = 1;
            w.range = 200.0f;
            w.ammoType = static_cast<int>(AmmoType::Heavy);
            w.hipSpread = 7.0f;
            w.aimSpread = 0.0f;
            w.recoil = 2.5f;
            w.maxRecoil = 2.5f;
            w.recoilRecovery = 2.5f;
            w.crouchSpreadScale = 0.5f;
            break;
        default:
            break;
    }
    return w;
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
    for (int& a : m_ammo) a = 0;
    m_selected = 0;
    m_pickaxe = false;
}

int Inventory::add(ItemType type, Rarity rarity, int count, int loaded) {
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
        s.rarity = def.kind == ItemKind::Weapon ? rarity : def.rarity;
        if (def.kind == ItemKind::Weapon) {
            s.count = 1;
            s.weapon.reset(weaponConfig(type, rarity));
            if (loaded >= 0) s.weapon.setLoaded(loaded);
            --count;
        } else {
            s.count = count < def.maxStack ? count : def.maxStack;
            count -= s.count;
        }
        if (count == 0) return 0;
    }
    return count;
}

int Inventory::room(ItemType type) const {
    const ItemDef& def = itemDef(type);
    if (def.kind == ItemKind::Empty) return 0;
    int n = 0;
    for (const InventorySlot& s : m_slots) {
        if (s.type == ItemType::None) n += def.kind == ItemKind::Weapon ? 1 : def.maxStack;
        else if (def.kind != ItemKind::Weapon && s.type == type && s.count < def.maxStack) n += def.maxStack - s.count;
    }
    return n;
}

int Inventory::place(int index, ItemType type, Rarity rarity, int count, int loaded, InventorySlot* dropped) {
    if (index < 0 || index >= kSlots) index = m_selected;
    *dropped = take(index);
    InventorySlot& s = m_slots[index];
    const ItemDef& def = itemDef(type);
    s.type = type;
    s.rarity = def.kind == ItemKind::Weapon ? rarity : def.rarity;
    s.count = def.kind == ItemKind::Weapon ? 1 : (count < def.maxStack ? count : def.maxStack);
    if (def.kind == ItemKind::Weapon) {
        s.weapon.reset(weaponConfig(type, rarity));
        if (loaded >= 0) s.weapon.setLoaded(loaded);
    }
    return s.count;
}

InventorySlot Inventory::take(int index) {
    InventorySlot out = m_slots[index];
    m_slots[index] = InventorySlot{};
    return out;
}

bool Inventory::full() const {
    for (const InventorySlot& s : m_slots) {
        if (s.type == ItemType::None) return false;
    }
    return true;
}

int Inventory::addAmmo(AmmoType type, int n) {
    int& a = m_ammo[static_cast<int>(type)];
    const int added = a + n > kMaxAmmo ? kMaxAmmo - a : n;
    a += added;
    return added;
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
