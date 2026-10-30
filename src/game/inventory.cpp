#include "game/inventory.h"

namespace game {
namespace {

const ItemDef kDefs[] = {
    // nombre            tipo                 rareza             pila  uso   vida  tope  escudo tope
    {i18n::T::None, ItemKind::Empty, Rarity::Common, 0, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f},
    {i18n::T::ItemAssaultRifle, ItemKind::Weapon, Rarity::Common, 1, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f},
    {i18n::T::ItemPumpShotgun, ItemKind::Weapon, Rarity::Uncommon, 1, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f},
    {i18n::T::ItemSuppressedSmg, ItemKind::Weapon, Rarity::Common, 1, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f},
    {i18n::T::ItemBoltSniper, ItemKind::Weapon, Rarity::Rare, 1, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f},
    {i18n::T::ItemBandage, ItemKind::Consumable, Rarity::Common, 15, 3.5f, 15.0f, 75.0f, 0.0f, 0.0f},
    {i18n::T::ItemMedkit, ItemKind::Consumable, Rarity::Uncommon, 3, 10.0f, 100.0f, 100.0f, 0.0f, 0.0f},
    {i18n::T::ItemSmallShield, ItemKind::Consumable, Rarity::Uncommon, 6, 2.0f, 0.0f, 0.0f, 25.0f, 50.0f},
    {i18n::T::ItemShieldPotion, ItemKind::Consumable, Rarity::Rare, 3, 5.0f, 0.0f, 0.0f, 50.0f, 100.0f},
    {i18n::T::ItemBurstRifle, ItemKind::Weapon, Rarity::Common, 1, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f},
    {i18n::T::ItemScopedRifle, ItemKind::Weapon, Rarity::Rare, 1, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f},
    {i18n::T::ItemSmg, ItemKind::Weapon, Rarity::Common, 1, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f},
    {i18n::T::ItemTacticalShotgun, ItemKind::Weapon, Rarity::Common, 1, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f},
    {i18n::T::ItemPistol, ItemKind::Weapon, Rarity::Common, 1, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f},
    {i18n::T::ItemRevolver, ItemKind::Weapon, Rarity::Common, 1, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f},
    {i18n::T::ItemSemiAutoSniper, ItemKind::Weapon, Rarity::Uncommon, 1, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f},
    {i18n::T::ItemRocketLauncher, ItemKind::Weapon, Rarity::Rare, 1, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f},
    {i18n::T::ItemGrenadeLauncher, ItemKind::Weapon, Rarity::Rare, 1, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f},
    {i18n::T::ItemGrenade, ItemKind::Throwable, Rarity::Common, 6, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f},
    {i18n::T::ItemSpikeTrap, ItemKind::Trap, Rarity::Uncommon, 9, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f},
    // v0.15: comida (el ultimo valor es el hambre que quita) y muebles para dormir.
    {i18n::T::ItemCannedFood, ItemKind::Consumable, Rarity::Common, 6, 2.5f, 0.0f, 0.0f, 0.0f, 0.0f, 30.0f},
    {i18n::T::ItemRation, ItemKind::Consumable, Rarity::Uncommon, 3, 4.0f, 10.0f, 100.0f, 0.0f, 0.0f, 65.0f},
    {i18n::T::ItemBed, ItemKind::Furniture, Rarity::Rare, 3, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f},
    {i18n::T::ItemSleepingBag, ItemKind::Furniture, Rarity::Uncommon, 1, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f},
};
static_assert(sizeof(kDefs) / sizeof(kDefs[0]) == static_cast<int>(ItemType::Count), "falta un objeto");

constexpr int kRarities = static_cast<int>(Rarity::Count);

// Ficha de cada arma (valores aproximados a Fortnite, capitulo 1). Dano y recarga por
// rareza: comun, poco comun, rara, epica, legendaria; 0 = el arma no existe en esa rareza.
struct WeaponSpec {
    ItemType type;
    i18n::T name;
    WeaponClass family;
    float damage[kRarities];
    float reload[kRarities];
    float fireInterval;
    int magazine;
    float range;
    AmmoType ammo;
    int pellets;
    float hipSpread, aimSpread;
    float recoil, maxRecoil, recoilRecovery, crouchSpreadScale;
    float falloffStart, falloffEnd, falloffMin;  // falloffStart >= 1000: sin caida
    int burstCount;
    ProjectileKind projectile;
    float projectileSpeed, blastRadius, structureDamage, fuseSeconds;
    float scopeFov;
};

constexpr float kNoFalloff = 1000.0f;
constexpr WeaponSpec kSpecs[] = {
    // Fusil de asalto (gris, verde y azul: el generico; morado y dorado: la SCAR).
    {ItemType::AssaultRifle, i18n::T::ItemAssaultRifle, WeaponClass::Rifle, {30, 31, 33, 35, 36}, {2.7f, 2.6f, 2.5f, 2.3f, 2.2f},
     1.0f / 5.5f, 30, 80.0f, AmmoType::Medium, 1, 2.4f, 0.7f, 0.55f, 3.0f, 8.0f, 0.6f, kNoFalloff, kNoFalloff, 1.0f, 1,
     ProjectileKind::None, 0, 0, 0, 0, 0},
    {ItemType::Shotgun, i18n::T::WeaponShotgun, WeaponClass::Shotgun, {0, 90, 95, 100, 105}, {0, 4.8f, 4.6f, 4.4f, 4.2f},
     0.9f, 5, 28.0f, AmmoType::Shells, 10, 6.0f, 4.5f, 1.2f, 1.2f, 2.0f, 0.85f, 8.0f, 25.0f, 0.35f, 1,
     ProjectileKind::None, 0, 0, 0, 0, 0},
    {ItemType::Smg, i18n::T::ItemSuppressedSmg, WeaponClass::Smg, {17, 18, 19, 20, 21}, {2.3f, 2.2f, 2.1f, 2.0f, 1.9f},
     0.1f, 30, 45.0f, AmmoType::Light, 1, 3.4f, 1.8f, 0.3f, 3.5f, 9.0f, 0.7f, 20.0f, 45.0f, 0.6f, 1,
     ProjectileKind::None, 0, 0, 0, 0, 0},
    {ItemType::Sniper, i18n::T::WeaponSniper, WeaponClass::Sniper, {0, 0, 105, 110, 116}, {0, 0, 3.1f, 2.9f, 2.7f},
     2.6f, 1, 200.0f, AmmoType::Heavy, 1, 7.0f, 0.0f, 2.5f, 2.5f, 2.5f, 0.5f, kNoFalloff, kNoFalloff, 1.0f, 1,
     ProjectileKind::None, 0, 0, 0, 0, 15.0f},
    // Fusil de rafagas (Recursos/Weapons/Burst_Assault_Rifle): 3 balas por pulsacion.
    {ItemType::BurstRifle, i18n::T::ItemBurstRifle, WeaponClass::Rifle, {27, 29, 30, 32, 34}, {2.9f, 2.7f, 2.6f, 2.5f, 2.3f},
     0.45f, 30, 80.0f, AmmoType::Medium, 1, 2.8f, 0.9f, 0.45f, 3.0f, 8.0f, 0.6f, kNoFalloff, kNoFalloff, 1.0f, 3,
     ProjectileKind::None, 0, 0, 0, 0, 0},
    // Fusil con mira (Scoped_Assault_Rifle): L mira por la mirilla con menos zoom.
    {ItemType::ScopedRifle, i18n::T::ItemScopedRifle, WeaponClass::Rifle, {0, 0, 23, 24, 0}, {0, 0, 2.3f, 2.2f, 0},
     1.0f / 3.5f, 20, 120.0f, AmmoType::Medium, 1, 3.0f, 0.25f, 0.6f, 2.0f, 6.0f, 0.6f, kNoFalloff, kNoFalloff, 1.0f, 1,
     ProjectileKind::None, 0, 0, 0, 0, 30.0f},
    // Subfusil (Submachine_Gun v1.6): mas cadencia y menos dano que el silenciado.
    {ItemType::SubmachineGun, i18n::T::ItemSmg, WeaponClass::Smg, {16, 17, 18, 0, 0}, {2.2f, 2.1f, 2.0f, 0, 0},
     1.0f / 12.0f, 30, 40.0f, AmmoType::Light, 1, 3.8f, 2.0f, 0.3f, 4.0f, 9.0f, 0.7f, 18.0f, 40.0f, 0.6f, 1,
     ProjectileKind::None, 0, 0, 0, 0, 0},
    // Escopeta tactica (Tactical_Shotgun): mas cadencia y cargador, menos dano.
    {ItemType::TacticalShotgun, i18n::T::ItemTacticalShotgun, WeaponClass::Shotgun, {67, 70, 74, 77, 81}, {4.6f, 4.4f, 4.2f, 4.0f, 3.8f},
     0.67f, 8, 25.0f, AmmoType::Shells, 9, 5.5f, 4.5f, 1.0f, 1.0f, 2.5f, 0.85f, 6.0f, 22.0f, 0.3f, 1,
     ProjectileKind::None, 0, 0, 0, 0, 0},
    {ItemType::Pistol, i18n::T::ItemPistol, WeaponClass::Pistol, {23, 24, 25, 0, 0}, {1.5f, 1.5f, 1.4f, 0, 0},
     0.15f, 16, 50.0f, AmmoType::Light, 1, 1.8f, 0.9f, 0.6f, 2.5f, 7.0f, 0.65f, 25.0f, 50.0f, 0.65f, 1,
     ProjectileKind::None, 0, 0, 0, 0, 0},
    {ItemType::Revolver, i18n::T::ItemRevolver, WeaponClass::Pistol, {54, 57, 60, 0, 0}, {2.2f, 2.1f, 2.0f, 0, 0},
     1.1f, 6, 60.0f, AmmoType::Medium, 1, 2.6f, 0.4f, 2.0f, 2.0f, 3.0f, 0.6f, kNoFalloff, kNoFalloff, 1.0f, 1,
     ProjectileKind::None, 0, 0, 0, 0, 0},
    // Francotirador semiautomatico (Icon_Weapons_SK_Sniper_02): 10 balas, menos dano.
    {ItemType::SemiAutoSniper, i18n::T::ItemSemiAutoSniper, WeaponClass::Sniper, {0, 63, 66, 0, 0}, {0, 2.5f, 2.4f, 0, 0},
     0.85f, 10, 180.0f, AmmoType::Heavy, 1, 6.0f, 0.0f, 1.6f, 3.2f, 3.0f, 0.5f, kNoFalloff, kNoFalloff, 1.0f, 1,
     ProjectileKind::None, 0, 0, 0, 0, 15.0f},
    // Lanzacohetes (Bazooka): cohete a 45 m/s que derriba construcciones.
    {ItemType::RocketLauncher, i18n::T::ItemRocketLauncher, WeaponClass::Launcher, {0, 0, 110, 116, 121}, {0, 0, 3.4f, 3.1f, 2.8f},
     1.33f, 1, 120.0f, AmmoType::Rockets, 1, 1.5f, 0.4f, 0.0f, 0.0f, 8.0f, 0.7f, kNoFalloff, kNoFalloff, 1.0f, 1,
     ProjectileKind::Rocket, 45.0f, 3.5f, 400.0f, 0.0f, 0},
    // Lanzagranadas (Grenade_Launcher): granadas que rebotan y explotan a los 2 s o al
    // tocar a alguien.
    {ItemType::GrenadeLauncher, i18n::T::ItemGrenadeLauncher, WeaponClass::Launcher, {0, 0, 100, 105, 110}, {0, 0, 3.0f, 2.8f, 2.5f},
     1.0f, 6, 80.0f, AmmoType::Rockets, 1, 2.0f, 1.0f, 0.0f, 0.0f, 8.0f, 0.7f, kNoFalloff, kNoFalloff, 1.0f, 1,
     ProjectileKind::LauncherGrenade, 26.0f, 3.5f, 300.0f, 2.0f, 0},
};

const WeaponSpec* specFor(ItemType type) {
    for (const WeaponSpec& s : kSpecs) {
        if (s.type == type) return &s;
    }
    return nullptr;
}

}  // namespace

const ItemDef& itemDef(ItemType type) {
    const int i = static_cast<int>(type);
    return kDefs[i >= 0 && i < static_cast<int>(ItemType::Count) ? i : 0];
}

const char* itemName(ItemType type) { return i18n::tr(itemDef(type).name); }

const char* rarityName(Rarity rarity) {
    switch (rarity) {
        case Rarity::Common: return i18n::tr(i18n::T::RarityCommon);
        case Rarity::Uncommon: return i18n::tr(i18n::T::RarityUncommon);
        case Rarity::Rare: return i18n::tr(i18n::T::RarityRare);
        case Rarity::Epic: return i18n::tr(i18n::T::RarityEpic);
        case Rarity::Legendary: return i18n::tr(i18n::T::RarityLegendary);
        case Rarity::Count: break;
    }
    return "";
}

const char* ammoName(AmmoType type) {
    switch (type) {
        case AmmoType::Light: return i18n::tr(i18n::T::AmmoLight);
        case AmmoType::Medium: return i18n::tr(i18n::T::AmmoMedium);
        case AmmoType::Heavy: return i18n::tr(i18n::T::AmmoHeavy);
        case AmmoType::Shells: return i18n::tr(i18n::T::AmmoShells);
        case AmmoType::Rockets: return i18n::tr(i18n::T::AmmoRockets);
        case AmmoType::Count: break;
    }
    return "";
}

bool isWeapon(ItemType type) { return itemDef(type).kind == ItemKind::Weapon; }

bool isFood(ItemType type) { return itemDef(type).food > 0.0f; }

WeaponClass weaponClass(ItemType type) {
    const WeaponSpec* s = specFor(type);
    return s ? s->family : WeaponClass::None;
}

bool hasScope(ItemType type) {
    const WeaponSpec* s = specFor(type);
    return s && s->scopeFov > 0.0f;
}

bool weaponHasRarity(ItemType type, Rarity rarity) {
    const WeaponSpec* s = specFor(type);
    return s && rarity < Rarity::Count && s->damage[static_cast<int>(rarity)] > 0.0f;
}

int ammoPerPickup(AmmoType type) {
    switch (type) {
        case AmmoType::Light: return 30;
        case AmmoType::Medium: return 20;
        case AmmoType::Heavy: return 6;
        case AmmoType::Shells: return 8;
        case AmmoType::Rockets: return 3;
        case AmmoType::Count: break;
    }
    return 0;
}

WeaponConfig weaponConfig(ItemType type, Rarity rarity) {
    WeaponConfig w;
    const WeaponSpec* s = specFor(type);
    if (!s) return w;
    int r = static_cast<int>(rarity);
    while (r > 0 && s->damage[r] <= 0.0f) --r;  // rareza inexistente: la mas cercana por debajo
    while (r < kRarities - 1 && s->damage[r] <= 0.0f) ++r;
    w.rarity = rarityName(static_cast<Rarity>(r));
    // El nombre se traduce al crear el arma (el idioma solo cambia fuera de la partida).
    w.name = i18n::tr(type == ItemType::AssaultRifle && r >= static_cast<int>(Rarity::Epic) ? i18n::T::WeaponScar : s->name);
    w.damage = s->damage[r];
    w.reloadTime = s->reload[r];
    w.reserveAmmo = 0;  // la reserva es la municion del inventario
    w.fireInterval = s->fireInterval;
    w.magazineSize = s->magazine;
    w.range = s->range;
    w.ammoType = static_cast<int>(s->ammo);
    w.pellets = s->pellets;
    w.hipSpread = s->hipSpread;
    w.aimSpread = s->aimSpread;
    w.recoil = s->recoil;
    w.maxRecoil = s->maxRecoil;
    w.recoilRecovery = s->recoilRecovery;
    w.crouchSpreadScale = s->crouchSpreadScale;
    w.falloffStart = s->falloffStart;
    w.falloffEnd = s->falloffEnd;
    w.falloffMin = s->falloffMin;
    w.burstCount = s->burstCount;
    w.projectile = s->projectile;
    w.projectileSpeed = s->projectileSpeed;
    w.blastRadius = s->blastRadius;
    w.structureDamage = s->structureDamage;
    w.fuseSeconds = s->fuseSeconds;
    w.scopeFov = s->scopeFov;
    return w;
}

// Granada de mano (no es un arma: es un arrojadizo, pero explota como las del lanzagranadas).
WeaponConfig grenadeConfig() {
    WeaponConfig w;
    w.name = i18n::tr(i18n::T::ItemGrenade);
    w.rarity = rarityName(Rarity::Common);
    w.damage = 105.0f;
    w.range = 40.0f;
    w.projectile = ProjectileKind::HandGrenade;
    w.projectileSpeed = 17.0f;
    w.blastRadius = 4.0f;
    w.structureDamage = 300.0f;
    w.fuseSeconds = 2.2f;
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
    m_capacity = kSlots;
}

bool Inventory::addBackpack() {
    if (m_capacity + kBackpackSlots > kMaxSlots) return false;
    m_capacity += kBackpackSlots;
    return true;
}

int Inventory::add(ItemType type, Rarity rarity, int count, int loaded) {
    const ItemDef& def = itemDef(type);
    if (def.kind == ItemKind::Empty || count <= 0) return count;
    // Primero se apila en las ranuras del mismo tipo.
    if (def.kind == ItemKind::Consumable || (def.kind == ItemKind::Furniture && def.maxStack > 1)) {
        for (int i = 0; i < m_capacity; ++i) {
            InventorySlot& s = m_slots[i];
            if (s.type != type || s.count >= def.maxStack) continue;
            const int take = count < def.maxStack - s.count ? count : def.maxStack - s.count;
            s.count += take;
            count -= take;
            if (count == 0) return 0;
        }
    }
    for (int i = 0; i < m_capacity; ++i) {
        InventorySlot& s = m_slots[i];
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
    for (int i = 0; i < m_capacity; ++i) {
        const InventorySlot& s = m_slots[i];
        if (s.type == ItemType::None) n += def.kind == ItemKind::Weapon ? 1 : def.maxStack;
        else if (def.kind != ItemKind::Weapon && s.type == type && s.count < def.maxStack) n += def.maxStack - s.count;
    }
    return n;
}

int Inventory::place(int index, ItemType type, Rarity rarity, int count, int loaded, InventorySlot* dropped) {
    if (index < 0 || index >= m_capacity) index = m_selected;
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

void Inventory::swapSlots(int a, int b) {
    if (a < 0 || b < 0 || a >= m_capacity || b >= m_capacity || a == b) return;
    const InventorySlot tmp = m_slots[a];
    m_slots[a] = m_slots[b];
    m_slots[b] = tmp;
}

InventorySlot Inventory::take(int index) {
    InventorySlot out = m_slots[index];
    m_slots[index] = InventorySlot{};
    return out;
}

bool Inventory::full() const {
    for (int i = 0; i < m_capacity; ++i) {
        if (m_slots[i].type == ItemType::None) return false;
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
    if (index < 0 || index >= m_capacity) return;
    m_selected = index;
    m_pickaxe = false;
}

bool Inventory::cycle(int direction) {
    const int step = direction < 0 ? m_capacity - 1 : 1;
    for (int k = 1; k <= m_capacity; ++k) {
        const int i = (m_selected + step * k) % m_capacity;
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
    if (index < 0 || index >= m_capacity || m_slots[index].count <= 0) return;
    if (--m_slots[index].count == 0) m_slots[index] = InventorySlot{};
}

}  // namespace game
