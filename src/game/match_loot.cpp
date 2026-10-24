// Botin de game::Match (docs/BOTIN.md): cofres, objetos en el suelo, cajas de municion y
// airdrops con armas legendarias. Todo se reparte al empezar la partida sobre puntos
// transitables de la rejilla de navegacion (fuera de la tormenta, separados entre si) y
// se abre o recoge con CUADRADO. Los bots tambien abren cofres y recogen armas.
#include "game/match.h"

#include "game/match_internal.h"
#include "world/storm.h"

namespace game {

using math3d::Vec3;
using namespace tuning;
using namespace detail;

namespace {

constexpr int kMaxPickups = 256;
constexpr float kPickupReach = 2.0f;     // m (en horizontal) para recoger con CUADRADO
constexpr float kContainerReach = 2.3f;  // m para abrir
constexpr float kAmmoAutoReach = 1.4f;   // la municion se recoge sola al pasar por encima

constexpr float kLlamaReach = 2.8f;      // la llama es mas grande que un cofre

// Puntuacion de un arma para los bots (mejor rareza y tipos mas versatiles primero).
float weaponScore(ItemType type, Rarity rarity) {
    float base = 0.0f;
    switch (type) {
        case ItemType::AssaultRifle: base = 30.0f; break;
        case ItemType::BurstRifle: base = 28.0f; break;
        case ItemType::ScopedRifle: base = 27.0f; break;
        case ItemType::Shotgun: base = 27.0f; break;
        case ItemType::TacticalShotgun: base = 26.0f; break;
        case ItemType::Smg: base = 25.0f; break;
        case ItemType::SubmachineGun: base = 24.0f; break;
        case ItemType::RocketLauncher: base = 22.0f; break;
        case ItemType::GrenadeLauncher: base = 20.0f; break;
        case ItemType::Sniper: base = 20.0f; break;
        case ItemType::SemiAutoSniper: base = 21.0f; break;
        case ItemType::Revolver: base = 19.0f; break;
        case ItemType::Pistol: base = 16.0f; break;
        default: return 0.0f;
    }
    return base + 6.0f * static_cast<float>(rarity);
}

// Armas del botin con su peso (las mas comunes, mas a menudo).
struct WeaponWeight {
    ItemType type;
    float weight;
};
constexpr WeaponWeight kWeaponWeights[] = {
    {ItemType::AssaultRifle, 20}, {ItemType::BurstRifle, 9},     {ItemType::ScopedRifle, 4},
    {ItemType::Shotgun, 10},      {ItemType::TacticalShotgun, 10}, {ItemType::Smg, 8},
    {ItemType::SubmachineGun, 8}, {ItemType::Pistol, 9},         {ItemType::Revolver, 6},
    {ItemType::Sniper, 4},        {ItemType::SemiAutoSniper, 4},  {ItemType::RocketLauncher, 3},
    {ItemType::GrenadeLauncher, 3},
};

}  // namespace

Rarity Match::randomRarity(bool chest) {
    // Cofres: mejores rarezas que el suelo (aproximado a Fortnite).
    static const float kFloor[] = {45, 32, 16, 6, 1};
    static const float kChest[] = {20, 35, 28, 13, 4};
    const float* w = chest ? kChest : kFloor;
    float total = 0.0f;
    for (int i = 0; i < 5; ++i) total += w[i];
    float r = m_rng.range(0.0f, total);
    for (int i = 0; i < 5; ++i) {
        if (r < w[i]) return static_cast<Rarity>(i);
        r -= w[i];
    }
    return Rarity::Common;
}

Pickup Match::randomWeaponPickup(bool chest) {
    Pickup p;
    p.active = true;
    p.kind = PickupKind::Item;
    float total = 0.0f;
    for (const WeaponWeight& w : kWeaponWeights) total += w.weight;
    float r = m_rng.range(0.0f, total);
    p.item = kWeaponWeights[0].type;
    for (const WeaponWeight& w : kWeaponWeights) {
        if (r < w.weight) {
            p.item = w.type;
            break;
        }
        r -= w.weight;
    }
    p.rarity = randomRarity(chest);
    // Si el arma no existe en esa rareza (escopeta comun, francotirador comun...), la
    // rareza mas cercana por encima.
    int k = static_cast<int>(p.rarity);
    while (k < 4 && !weaponHasRarity(p.item, static_cast<Rarity>(k))) ++k;
    while (k > 0 && !weaponHasRarity(p.item, static_cast<Rarity>(k))) --k;
    p.rarity = static_cast<Rarity>(k);
    p.count = 1;
    return p;
}

Pickup Match::randomConsumablePickup() {
    Pickup p;
    p.active = true;
    p.kind = PickupKind::Item;
    const float r = m_rng.uniform();
    if (r < 0.30f) {
        p.item = ItemType::Bandage;
        p.count = 5;
    } else if (r < 0.40f) {
        p.item = ItemType::Grenade;
        p.count = 3;
    } else if (r < 0.65f) {
        p.item = ItemType::SmallShield;
        p.count = 3;
    } else if (r < 0.85f) {
        p.item = ItemType::ShieldPotion;
        p.count = 1;
    } else {
        p.item = ItemType::Medkit;
        p.count = 1;
    }
    p.rarity = itemDef(p.item).rarity;
    return p;
}

static Pickup trapPickup(int count) {
    Pickup p;
    p.active = true;
    p.kind = PickupKind::Item;
    p.item = ItemType::SpikeTrap;
    p.rarity = itemDef(ItemType::SpikeTrap).rarity;
    p.count = count;
    return p;
}

static Pickup ammoPickup(AmmoType type) {
    Pickup p;
    p.active = true;
    p.kind = PickupKind::Ammo;
    p.ammo = type;
    p.count = ammoPerPickup(type);
    return p;
}

Vec3 Match::lootSpot(float minSpacing, bool nearPoi) {
    Vec3 best = m_config.zoneCenter;
    for (int attempt = 0; attempt < 30; ++attempt) {
        Vec3 p;
        const Vec3 center = nearPoi && !m_pois.empty() ? m_pois[m_rng.index(static_cast<int>(m_pois.size()))]
                                                       : m_config.zoneCenter;
        if (!m_nav->randomPointNear(center, nearPoi ? 8.0f : m_config.zoneStartRadius + 2.0f, m_rng, &p)) continue;
        if (world::stormDepth(p.x, p.z) > -6.0f) continue;
        bool ok = true;
        for (const Vec3& q : m_lootSpots) {
            if (distXZ(p, q) < minSpacing) {
                ok = false;
                break;
            }
        }
        best = p;
        if (ok) break;
    }
    best.y = m_world->supportHeight(best.x, best.z, 0.3f, best.y + 0.5f);
    m_lootSpots.push_back(best);
    return best;
}

void Match::spawnLoot() {
    m_pickups.clear();
    m_pickups.reserve(kMaxPickups);
    m_containers.clear();
    m_lootSpots.clear();
    m_nextAirdrop = 0;
    if (!m_config.loot) return;
    for (int i = 0; i < m_config.chestCount; ++i) {
        const Vec3 p = lootSpot(9.0f, i % 2 == 0);
        addContainer(ContainerKind::Chest, p, 0.5f * math3d::kPi * static_cast<float>(m_rng.index(4)));
    }
    for (int i = 0; i < m_config.ammoBoxCount; ++i) {
        const Vec3 p = lootSpot(8.0f, i % 3 == 0);
        addContainer(ContainerKind::AmmoBox, p, 0.5f * math3d::kPi * static_cast<float>(m_rng.index(4)));
    }
    for (int i = 0; i < m_config.floorLootCount; ++i) {
        const Vec3 p = lootSpot(6.0f, i % 3 == 0);
        const float r = m_rng.uniform();
        if (r < 0.55f) {
            // Arma con su municion al lado.
            Pickup w = randomWeaponPickup(false);
            dropAround(p, w, 0, 2);
            dropAround(p, ammoPickup(static_cast<AmmoType>(weaponConfig(w.item, w.rarity).ammoType)), 1, 2);
        } else if (r < 0.81f) {
            dropAround(p, randomConsumablePickup(), 0, 1);
        } else if (r < 0.85f) {
            dropAround(p, trapPickup(1), 0, 1);
        } else {
            dropAround(p, ammoPickup(static_cast<AmmoType>(m_rng.index(4))), 0, 1);
        }
    }
}

void Match::spawnLlama() {
    // Un punto transitable al aire libre (sin techo encima: nada de naves ni altillos), fuera
    // de la tormenta, lejos de la aparicion y de los otros contenedores, y con sitio libre
    // para la llama (ni paredes ni objetos solapandola).
    const Vec3 spawn = body(0).state().position;
    for (int attempt = 0; attempt < 80; ++attempt) {
        Vec3 p;
        if (!m_nav->randomPointNear(m_config.zoneCenter, m_config.zoneStartRadius - 6.0f, m_rng, &p)) continue;
        if (world::stormDepth(p.x, p.z) > -18.0f || distXZ(p, spawn) < 12.0f) continue;
        p.y = m_world->supportHeight(p.x, p.z, 0.6f, p.y + 0.5f);
        const world::Aabb room{{p.x - 1.0f, p.y + 0.15f, p.z - 1.0f}, {p.x + 1.0f, p.y + 2.2f, p.z + 1.0f}};
        if (m_world->overlapsAny(room, world::kBlocksMovement)) continue;
        if (m_world->ceilingHeight(p.x, p.z, 0.8f, p.y + 0.5f) < p.y + 30.0f) continue;
        bool clear = true;
        for (const Container& c : m_containers) clear &= distXZ(c.position, p) > 3.0f;
        if (!clear) continue;
        const int index = addContainer(ContainerKind::Llama, p, m_rng.range(-math3d::kPi, math3d::kPi));
        m_containers[index].health = m_config.llamaHealth;
        m_llama = index;
        pushEvent(GameEventType::LlamaSpawned, -1, index, p);
        return;
    }
}

bool Match::llamaStanding() const {
    return m_llama >= 0 && m_llama < static_cast<int>(m_containers.size()) && !m_containers[m_llama].opened;
}

world::Aabb Match::llamaBox() const {
    // Cuerpo, cuello y cabeza de la llama (PropVisuals la dibuja dentro de esta caja).
    const Vec3 p = m_containers[m_llama].position;
    return {{p.x - 0.55f, p.y, p.z - 0.55f}, {p.x + 0.55f, p.y + 1.75f, p.z + 0.55f}};
}

void Match::damageContainer(int index, float amount, int by, const Vec3& point) {
    if (index < 0 || index >= static_cast<int>(m_containers.size())) return;
    Container& c = m_containers[index];
    if (c.opened || c.kind != ContainerKind::Llama) return;
    pushEvent(GameEventType::HitStructure, -1, by, point);
    c.health -= amount;
    if (c.health > 0.0f) return;
    // Rota: suelta lo mismo que al abrirla (una sola vez).
    Interactable it;
    it.kind = Interactable::Kind::Container;
    it.index = index;
    interact(by >= 0 ? by : -1, it);
}

int Match::addContainer(ContainerKind kind, const Vec3& position, float yaw) {
    Container c;
    c.kind = kind;
    c.position = position;
    c.yaw = yaw;
    m_containers.push_back(c);
    return static_cast<int>(m_containers.size()) - 1;
}

int Match::dropPickup(const Pickup& pickup) {
    for (size_t i = 0; i < m_pickups.size(); ++i) {
        if (!m_pickups[i].active) {
            m_pickups[i] = pickup;
            m_pickups[i].active = true;
            m_pickups[i].claimedBy = -1;
            return static_cast<int>(i);
        }
    }
    if (static_cast<int>(m_pickups.size()) >= kMaxPickups) return -1;
    m_pickups.push_back(pickup);
    m_pickups.back().active = true;
    m_pickups.back().claimedBy = -1;
    return static_cast<int>(m_pickups.size()) - 1;
}

void Match::dropAround(const Vec3& center, const Pickup& pickup, int index, int total, float radius, float turn) {
    // Repartidos en corro alrededor del cofre / del sitio (como el botin que "salta").
    Pickup p = pickup;
    if (total > 1) {
        const float a = 2.0f * math3d::kPi * static_cast<float>(index) / static_cast<float>(total) + 0.4f + turn;
        p.position = center + Vec3{cosf(a) * radius, 0.0f, sinf(a) * radius};
    } else {
        p.position = center;
    }
    // Nunca exactamente encima de otro objeto (dos bots que caen en el mismo sitio): se
    // aparta un poco hacia fuera.
    for (int attempt = 0; attempt < 4; ++attempt) {
        bool crowded = false;
        for (const Pickup& o : m_pickups) {
            if (o.active && distXZ(o.position, p.position) < 0.3f && fabsf(o.position.y - p.position.y) < 1.0f) {
                crowded = true;
                break;
            }
        }
        if (!crowded) break;
        const Vec3 out = flatten(p.position - center);
        const float len = math3d::length(out);
        const Vec3 dir = len > 0.05f ? out * (1.0f / len) : Vec3{cosf(attempt * 2.1f), 0.0f, sinf(attempt * 2.1f)};
        p.position = p.position + dir * 0.45f;
    }
    p.position.y = m_world->supportHeight(p.position.x, p.position.z, 0.2f, center.y + 0.5f);
    dropPickup(p);
}

void Match::spawnContainerLoot(int index, int opener) {
    const Container& c = m_containers[index];
    const Vec3 front = c.position + forwardFromYaw(c.yaw) * 0.6f;
    switch (c.kind) {
        case ContainerKind::Chest: {
            Pickup w = randomWeaponPickup(true);
            const AmmoType ammo = static_cast<AmmoType>(weaponConfig(w.item, w.rarity).ammoType);
            const bool consumable = m_rng.chance(0.65f);
            const int total = consumable ? 3 : 2;
            dropAround(front, w, 0, total);
            dropAround(front, ammoPickup(ammo), 1, total);
            if (consumable) dropAround(front, randomConsumablePickup(), 2, total);
            // A veces, una trampa de pinchos (al lado, fuera del corro).
            if (m_rng.chance(0.2f)) dropAround(front + forwardFromYaw(c.yaw) * 0.8f, trapPickup(1), 0, 1);
            // Materiales para quien lo abre (en Fortnite salen al suelo; aqui directos).
            if (opener == 0) m_materials.add(static_cast<world::Resource>(m_rng.index(3)), 30);
            break;
        }
        case ContainerKind::AmmoBox:
            dropAround(front, ammoPickup(static_cast<AmmoType>(m_rng.index(4))), 0, 2);
            dropAround(front, ammoPickup(static_cast<AmmoType>(m_rng.index(4))), 1, 2);
            break;
        case ContainerKind::Llama: {
            // Como en Fortnite: muchos materiales, municion de todo tipo, curas y granadas.
            // En corro alrededor de la llama (el suelo se ajusta a cada objeto).
            const int total = 13;
            int slot = 0;
            for (int r = 0; r < static_cast<int>(world::Resource::Count); ++r) {
                Pickup m;
                m.active = true;
                m.kind = PickupKind::Materials;
                m.resource = static_cast<world::Resource>(r);
                m.count = 200;
                dropAround(c.position, m, slot++, total, 1.6f);
            }
            for (int a = 0; a < static_cast<int>(AmmoType::Count); ++a) {
                dropAround(c.position, ammoPickup(static_cast<AmmoType>(a)), slot++, total, 1.6f);
            }
            Pickup potion;
            potion.active = true;
            potion.item = ItemType::ShieldPotion;
            potion.rarity = Rarity::Rare;
            potion.count = 2;
            dropAround(c.position, potion, slot++, total, 1.6f);
            Pickup grenades;
            grenades.active = true;
            grenades.item = ItemType::Grenade;
            grenades.rarity = itemDef(ItemType::Grenade).rarity;
            grenades.count = 3;
            dropAround(c.position, grenades, slot++, total, 1.6f);
            dropAround(c.position, randomConsumablePickup(), slot++, total, 1.6f);
            dropAround(c.position, trapPickup(2), slot++, total, 1.6f);
            break;
        }
        case ContainerKind::Airdrop: {
            // Dos armas legendarias distintas, su municion y dos pociones de escudo.
            const ItemType options[] = {ItemType::AssaultRifle, ItemType::Shotgun,       ItemType::Sniper,
                                        ItemType::Smg,          ItemType::BurstRifle,    ItemType::TacticalShotgun,
                                        ItemType::RocketLauncher, ItemType::GrenadeLauncher};
            constexpr int kOptions = sizeof(options) / sizeof(options[0]);
            const int first = m_rng.index(kOptions);
            const int second = (first + 1 + m_rng.index(kOptions - 1)) % kOptions;
            const ItemType chosen[2] = {options[first], options[second]};
            int slot = 0;
            for (ItemType type : chosen) {
                Pickup w;
                w.active = true;
                w.kind = PickupKind::Item;
                w.item = type;
                w.rarity = Rarity::Legendary;
                w.count = 1;
                dropAround(front, w, slot++, 6);
                dropAround(front, ammoPickup(static_cast<AmmoType>(weaponConfig(type, Rarity::Legendary).ammoType)), slot++, 6);
            }
            Pickup potion;
            potion.active = true;
            potion.item = ItemType::ShieldPotion;
            potion.rarity = Rarity::Rare;
            potion.count = 2;
            dropAround(front, potion, slot++, 6);
            dropAround(front, ammoPickup(AmmoType::Medium), slot, 6);
            break;
        }
    }
}

Interactable Match::interactableFor(int id) const {
    Interactable result;
    if (!alive(id) || m_air[id] != AirState::Ground) return result;
    const Vec3 pos = body(id).state().position;
    const Vec3 forward = id == 0 ? forwardFromYaw(m_input.cameraYaw) : forwardFromYaw(body(id).state().yaw);
    float best = 1e9f;
    // Se prefiere lo que esta delante (la mira), luego lo mas cercano.
    auto score = [&](const Vec3& p) {
        const Vec3 d = flatten(p - pos);
        const float dist = math3d::length(d);
        const float facing = dist > 0.01f ? math3d::dot(d * (1.0f / dist), forward) : 1.0f;
        return dist - 0.5f * facing;
    };
    for (size_t i = 0; i < m_containers.size(); ++i) {
        const Container& c = m_containers[i];
        if (c.opened || !c.landed) continue;
        const float reach = c.kind == ContainerKind::Llama ? kLlamaReach : kContainerReach;
        if (distXZ(c.position, pos) > reach || fabsf(c.position.y - pos.y) > 1.6f) continue;
        const float s = score(c.position);
        if (s < best) {
            best = s;
            result.kind = Interactable::Kind::Container;
            result.index = static_cast<int>(i);
        }
    }
    for (size_t i = 0; i < m_pickups.size(); ++i) {
        const Pickup& p = m_pickups[i];
        if (!p.active || p.kind != PickupKind::Item) continue;
        if (distXZ(p.position, pos) > kPickupReach || fabsf(p.position.y - pos.y) > 1.6f) continue;
        const float s = score(p.position);
        if (s < best) {
            best = s;
            result.kind = Interactable::Kind::Pickup;
            result.index = static_cast<int>(i);
        }
    }
    return result;
}

bool Match::interact(int id, const Interactable& target) {
    if (target.kind == Interactable::Kind::Pickup) return pickUp(id, target.index);
    if (target.kind != Interactable::Kind::Container) return false;
    Container& c = m_containers[target.index];
    if (c.opened || !c.landed) return false;
    c.opened = true;
    c.openedAt = m_time;
    c.claimedBy = -1;
    pushEvent(GameEventType::ContainerOpened, id, static_cast<int>(c.kind), c.position);
    spawnContainerLoot(target.index, id);
    return true;
}

bool Match::pickUp(int id, int index) {
    if (index < 0 || index >= static_cast<int>(m_pickups.size())) return false;
    Pickup& p = m_pickups[index];
    if (!p.active) return false;
    if (id > 0) {
        // Bot: armas y consumibles a su inventario (la municion no: la suya es infinita).
        Bot& b = m_bots[id - 1];
        if (!botWants(b, p)) {
            p.claimedBy = -1;
            return false;
        }
        const Pickup taken = p;
        const int vacate = isWeapon(taken.item) ? botReplacedSlot(b, taken) : -1;
        // Un arma mejor del mismo tipo, o lleno: suelta la que sustituye (con sus balas).
        if (vacate >= 0) dropSlot(b.bag.take(vacate), b.body.state().position);
        const int left = b.bag.add(taken.item, taken.rarity, taken.count, taken.loaded);
        Pickup& now = m_pickups[index];  // dropAround puede haber movido el vector
        now.count = left;
        if (left <= 0) now.active = false;
        now.claimedBy = -1;
        botEquip(b, b.target >= 0 && alive(b.target) ? distXZ(body(b.target).state().position, b.body.state().position)
                                                      : 20.0f);
        pushEvent(GameEventType::PickedUp, id, static_cast<int>(taken.item), taken.position);
        return true;
    }

    if (p.kind == PickupKind::Ammo || p.kind == PickupKind::Materials) {
        const int added = p.kind == PickupKind::Ammo ? m_inventory.addAmmo(p.ammo, p.count) : m_materials.add(p.resource, p.count);
        if (added <= 0) return false;
        p.count -= added;
        if (p.count <= 0) p.active = false;
        pushEvent(GameEventType::PickedUp, 0, -1, p.position);
        return true;
    }
    // Trampas: a la barra de construccion (no ocupan ranura).
    if (itemDef(p.item).kind == ItemKind::Trap) {
        const int room = m_config.maxTrapsCarried - m_trapStock;
        if (room <= 0) return false;
        const int taken = p.count < room ? p.count : room;
        m_trapStock += taken;
        p.count -= taken;
        if (p.count <= 0) p.active = false;
        pushEvent(GameEventType::PickedUp, 0, static_cast<int>(p.item), p.position);
        return true;
    }
    // Inventario lleno (no cabe ni una unidad): se cambia por lo de la ranura seleccionada.
    // El jugador elige antes la ranura en el selector del HUD (PlayerCommands::swapSlot).
    if (m_inventory.room(p.item) <= 0) return swapPickup(index, m_inventory.selected());
    const bool handsFree = m_inventory.activeType() == ItemType::None;
    p.count = m_inventory.add(p.item, p.rarity, p.count, p.loaded);
    if (p.count <= 0) p.active = false;
    // Con las manos libres (pico o ranura vacia) se coge lo recogido.
    if (handsFree) {
        for (int i = 0; i < Inventory::kSlots; ++i) {
            if (m_inventory.slot(i).type == p.item) m_inventory.select(i);
        }
    }
    pushEvent(GameEventType::PickedUp, 0, static_cast<int>(p.item), p.position);
    return true;
}

int Match::pickupRoom(int index) const {
    if (index < 0 || index >= static_cast<int>(m_pickups.size())) return 0;
    const Pickup& p = m_pickups[index];
    if (!p.active) return 0;
    if (p.kind == PickupKind::Ammo) return p.count;
    if (itemDef(p.item).kind == ItemKind::Trap) return m_config.maxTrapsCarried - m_trapStock;
    return m_inventory.room(p.item);
}

bool Match::pickupInReach(int index) const {
    if (index < 0 || index >= static_cast<int>(m_pickups.size()) || !alive(0) || m_air[0] != AirState::Ground) return false;
    const Pickup& p = m_pickups[index];
    const Vec3 pos = body(0).state().position;
    return p.active && p.kind == PickupKind::Item && distXZ(p.position, pos) <= kPickupReach + 0.5f &&
           fabsf(p.position.y - pos.y) <= 1.6f;
}

bool Match::swapPickup(int index, int slot) {
    if (index < 0 || index >= static_cast<int>(m_pickups.size()) || slot < 0 || slot >= Inventory::kSlots) return false;
    if (!pickupInReach(index)) return false;
    const Pickup taken = m_pickups[index];
    const Vec3 pos = body(0).state().position;
    cancelUse();
    if (Weapon* w = m_inventory.activeWeapon()) w->cancelReload();
    InventorySlot dropped;
    const int placed = m_inventory.place(slot, taken.item, taken.rarity, taken.count, taken.loaded, &dropped);
    m_inventory.select(slot);
    Pickup& now = m_pickups[index];
    now.count -= placed;
    if (now.count <= 0) now.active = false;
    // Lo soltado cae donde estaba lo recogido (o, si queda parte del monton, delante).
    const Vec3 where = now.active ? pos + forwardFromYaw(m_input.cameraYaw) * 0.6f : taken.position;
    dropSlot(dropped, where);
    pushEvent(GameEventType::PickedUp, 0, static_cast<int>(taken.item), taken.position);
    return true;
}

void Match::dropSlot(const InventorySlot& slot, const Vec3& where) {
    if (slot.type == ItemType::None || slot.count <= 0) return;
    Pickup out;
    out.kind = PickupKind::Item;
    out.item = slot.type;
    out.rarity = slot.rarity;
    out.count = slot.count;
    out.loaded = isWeapon(slot.type) ? slot.weapon.ammo() : -1;
    out.position = where;
    out.position.y = m_world->supportHeight(where.x, where.z, 0.2f, where.y + 0.5f);
    dropPickup(out);
}

void Match::autoPickupAmmo(int id) {
    // La municion y los materiales se recogen solos al pasar por encima.
    if (id != 0 || !alive(0) || m_air[0] != AirState::Ground) return;
    const Vec3 pos = body(0).state().position;
    for (size_t i = 0; i < m_pickups.size(); ++i) {
        const Pickup& p = m_pickups[i];
        if (!p.active || p.kind == PickupKind::Item) continue;
        if (distXZ(p.position, pos) < kAmmoAutoReach && fabsf(p.position.y - pos.y) < 1.5f) {
            pickUp(0, static_cast<int>(i));
        }
    }
}

void Match::updateAirdrops(float dt) {
    if (!m_config.loot) return;
    // Lanzamiento: un punto transitable cerca del centro de la zona.
    if (m_nextAirdrop < 2 && m_time >= m_config.airdropTimes[m_nextAirdrop]) {
        ++m_nextAirdrop;
        Vec3 p;
        const float radius = fmaxf(fminf(m_storm.nextRadius() * 0.6f, 50.0f), 2.0f);
        bool found = false;
        for (int attempt = 0; attempt < 10 && !found; ++attempt) {
            found = m_nav->randomPointNear(m_storm.nextCenter(), radius, m_rng, &p) &&
                    stormDepthAt(p.x, p.z) < -10.0f;
        }
        if (found) {
            const int c = addContainer(ContainerKind::Airdrop, p + Vec3{0.0f, 60.0f, 0.0f}, m_rng.range(0.0f, 6.28f));
            m_containers[c].landed = false;
            pushEvent(GameEventType::AirdropIncoming, -1, c, p);
        }
    }
    // Bajada con el globo.
    for (size_t i = 0; i < m_containers.size(); ++i) {
        Container& c = m_containers[i];
        if (c.landed) continue;
        c.position.y -= m_config.airdropFallSpeed * dt;
        const float ground = m_world->supportHeight(c.position.x, c.position.z, 0.6f, c.position.y + 0.1f);
        if (c.position.y <= ground) {
            c.position.y = ground;
            c.landed = true;
            pushEvent(GameEventType::AirdropLanded, -1, static_cast<int>(i), c.position);
        }
    }
}

void Match::dropBotWeapon(int index) {
    // Todo su inventario queda en el suelo, cada arma con un paquete de su municion.
    Bot& b = m_bots[index];
    const Vec3 pos = b.body.state().position;
    int total = 0;
    for (int i = 0; i < Inventory::kSlots; ++i) {
        const ItemType t = b.bag.slot(i).type;
        total += t == ItemType::None ? 0 : (isWeapon(t) ? 2 : 1);
    }
    // Materiales (como en Fortnite): madera siempre; piedra y metal, a veces.
    Pickup mats[3];
    int matCount = 0;
    if (m_config.botsDropMaterials) {
        const int wood = 30 + m_rng.index(31);
        const int stone = m_rng.chance(0.6f) ? 20 + m_rng.index(21) : 0;
        const int metal = m_rng.chance(0.35f) ? 10 + m_rng.index(21) : 0;
        const int amounts[3] = {wood, stone, metal};
        for (int r = 0; r < 3; ++r) {
            if (amounts[r] <= 0) continue;
            Pickup& m = mats[matCount++];
            m.kind = PickupKind::Materials;
            m.resource = static_cast<world::Resource>(r);
            m.count = amounts[r];
        }
    }
    total += matCount;
    const float turn = m_rng.range(0.0f, 2.0f * math3d::kPi);  // cada bot, su corro girado
    for (int k = 0; k < matCount; ++k) dropAround(pos, mats[k], total - matCount + k, total, 0.9f, turn);
    int slot = 0;
    for (int i = 0; i < Inventory::kSlots; ++i) {
        const InventorySlot s = b.bag.take(i);
        if (s.type == ItemType::None) continue;
        Pickup w;
        w.kind = PickupKind::Item;
        w.item = s.type;
        w.rarity = s.rarity;
        w.count = s.count;
        w.loaded = isWeapon(s.type) ? s.weapon.ammo() : -1;
        dropAround(pos, w, slot++, total, 0.9f, turn);
        if (isWeapon(s.type)) {
            dropAround(pos, ammoPickup(static_cast<AmmoType>(weaponConfig(s.type, s.rarity).ammoType)), slot++, total, 0.9f,
                       turn);
        }
    }
    b.armed = false;
    b.weaponType = ItemType::None;
}

bool Match::botWants(const Bot& b, const Pickup& p) const {
    if (!p.active || p.kind != PickupKind::Item) return false;
    // Los bots no lanzan granadas ni ponen trampas.
    if (itemDef(p.item).kind == ItemKind::Throwable || itemDef(p.item).kind == ItemKind::Trap) return false;
    if (!isWeapon(p.item)) {
        // Consumibles: si caben (en un monton suyo o en una ranura libre).
        if (!b.bag.full()) return true;
        for (int i = 0; i < Inventory::kSlots; ++i) {
            const InventorySlot& s = b.bag.slot(i);
            if (s.type == p.item && s.count < itemDef(p.item).maxStack) return true;
        }
        return false;
    }
    // Armas: un tipo que no lleva (si cabe), o mejor que la que sustituiria.
    const int slot = botReplacedSlot(b, p);
    if (slot < 0) return !b.bag.full();
    const InventorySlot& s = b.bag.slot(slot);
    return !isWeapon(s.type) || weaponScore(p.item, p.rarity) > weaponScore(s.type, s.rarity);
}

int Match::botReplacedSlot(const Bot& b, const Pickup& p) const {
    // Ranura que dejaria un arma nueva: la del mismo tipo (no lleva dos iguales); si no y
    // esta lleno, la peor (un consumible cuenta menos que cualquier arma). -1: hay hueco.
    int worst = -1;
    float worstScore = 1e9f;
    for (int i = 0; i < Inventory::kSlots; ++i) {
        const InventorySlot& s = b.bag.slot(i);
        if (isWeapon(s.type) && s.type == p.item) return i;
        const float score = isWeapon(s.type) ? weaponScore(s.type, s.rarity) : 5.0f;
        if (score < worstScore) {
            worstScore = score;
            worst = i;
        }
    }
    return b.bag.full() ? worst : -1;
}

void Match::botEquip(Bot& b, float distance) {
    // El arma que mejor va a esa distancia: escopeta de cerca, francotirador de lejos y,
    // si no, la mejor (rareza y tipo).
    int best = -1;
    float bestScore = -1e9f;
    for (int i = 0; i < Inventory::kSlots; ++i) {
        const InventorySlot& s = b.bag.slot(i);
        if (!isWeapon(s.type)) continue;
        float score = weaponScore(s.type, s.rarity);
        switch (weaponClass(s.type)) {
            case WeaponClass::Shotgun: score += distance < 9.0f ? 25.0f : (distance > 16.0f ? -20.0f : 0.0f); break;
            case WeaponClass::Sniper: score += distance > 28.0f ? 25.0f : (distance < 14.0f ? -20.0f : 0.0f); break;
            case WeaponClass::Smg: score += distance < 14.0f ? 6.0f : 0.0f; break;
            // Explosivos: nunca de cerca (el bot tambien se haria dano).
            case WeaponClass::Launcher: score += distance < 8.0f ? -60.0f : (distance < 35.0f ? 6.0f : -10.0f); break;
            default: break;
        }
        if (s.type == ItemType::ScopedRifle && distance > 25.0f) score += 6.0f;
        if (score > bestScore) {
            bestScore = score;
            best = i;
        }
    }
    if (best < 0) {
        // Sin armas en el inventario: las reglas clasicas dan un arma fija sin inventario.
        if (b.weaponType != ItemType::None) b.armed = false;
        return;
    }
    const InventorySlot& s = b.bag.slot(best);
    if (b.armed && b.weaponType == s.type && b.weaponRarity == s.rarity) return;
    const bool switching = b.armed;
    b.armed = true;
    b.weaponType = s.type;
    b.weaponRarity = s.rarity;
    WeaponConfig w = weaponConfig(s.type, s.rarity);
    w.reserveAmmo = -1;
    b.weapon.reset(w);
    if (switching && b.reaction < 0.3f) b.reaction = 0.3f;  // sacar otra arma lleva un momento
}

const char* Match::botWeaponName(int i) const {
    const Bot& b = m_bots[i];
    if (!b.armed) return "pico";
    return b.weapon.config().name;
}

}  // namespace game
