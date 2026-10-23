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

// Puntuacion de un arma para los bots (mejor rareza y tipos mas versatiles primero).
float weaponScore(ItemType type, Rarity rarity) {
    float base = 0.0f;
    switch (type) {
        case ItemType::AssaultRifle: base = 30.0f; break;
        case ItemType::Shotgun: base = 27.0f; break;
        case ItemType::Smg: base = 25.0f; break;
        case ItemType::Sniper: base = 20.0f; break;
        default: return 0.0f;
    }
    return base + 6.0f * static_cast<float>(rarity);
}

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
    const float r = m_rng.uniform();
    p.item = r < 0.40f ? ItemType::AssaultRifle : r < 0.65f ? ItemType::Shotgun : r < 0.87f ? ItemType::Smg : ItemType::Sniper;
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
    if (r < 0.35f) {
        p.item = ItemType::Bandage;
        p.count = 5;
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
        } else if (r < 0.85f) {
            dropAround(p, randomConsumablePickup(), 0, 1);
        } else {
            dropAround(p, ammoPickup(static_cast<AmmoType>(m_rng.index(4))), 0, 1);
        }
    }
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

void Match::dropAround(const Vec3& center, const Pickup& pickup, int index, int total) {
    // Repartidos en corro alrededor del cofre / del sitio (como el botin que "salta").
    Pickup p = pickup;
    if (total > 1) {
        const float a = 2.0f * math3d::kPi * static_cast<float>(index) / static_cast<float>(total) + 0.4f;
        p.position = center + Vec3{cosf(a) * 0.9f, 0.0f, sinf(a) * 0.9f};
    } else {
        p.position = center;
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
            // Materiales para quien lo abre (en Fortnite salen al suelo; aqui directos).
            if (opener == 0) m_materials.add(static_cast<world::Resource>(m_rng.index(3)), 30);
            break;
        }
        case ContainerKind::AmmoBox:
            dropAround(front, ammoPickup(static_cast<AmmoType>(m_rng.index(4))), 0, 2);
            dropAround(front, ammoPickup(static_cast<AmmoType>(m_rng.index(4))), 1, 2);
            break;
        case ContainerKind::Airdrop: {
            // Dos armas legendarias distintas, su municion y dos pociones de escudo.
            const ItemType options[] = {ItemType::AssaultRifle, ItemType::Shotgun, ItemType::Sniper, ItemType::Smg};
            const int first = m_rng.index(4);
            const int second = (first + 1 + m_rng.index(3)) % 4;
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
        if (distXZ(c.position, pos) > kContainerReach || fabsf(c.position.y - pos.y) > 1.6f) continue;
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
        // Bot: solo armas (municion infinita) y solo si mejora la suya.
        Bot& b = m_bots[id - 1];
        if (p.kind != PickupKind::Item || !isWeapon(p.item)) return false;
        if (b.armed && weaponScore(p.item, p.rarity) <= weaponScore(b.weaponType, b.weaponRarity)) {
            p.claimedBy = -1;
            return false;
        }
        const Pickup taken = p;
        p.active = false;
        if (b.armed) dropBotWeapon(id - 1);
        b.armed = true;
        b.weaponType = taken.item;
        b.weaponRarity = taken.rarity;
        WeaponConfig w = weaponConfig(taken.item, taken.rarity);
        w.reserveAmmo = -1;
        b.weapon.reset(w);
        pushEvent(GameEventType::PickedUp, id, static_cast<int>(taken.item), taken.position);
        return true;
    }

    if (p.kind == PickupKind::Ammo) {
        const int added = m_inventory.addAmmo(p.ammo, p.count);
        if (added <= 0) return false;
        p.count -= added;
        if (p.count <= 0) p.active = false;
        pushEvent(GameEventType::PickedUp, 0, -1, p.position);
        return true;
    }
    const bool handsFree = m_inventory.activeType() == ItemType::None;
    const int before = p.count;
    int left = m_inventory.add(p.item, p.rarity, p.count);
    if (left == before) {
        // Inventario lleno: se cambia por lo que hay en la ranura seleccionada (que cae al suelo).
        InventorySlot dropped;
        cancelUse();
        m_inventory.replaceSelected(p.item, p.rarity, p.count, &dropped);
        left = p.count - (isWeapon(p.item) ? 1 : (p.count < itemDef(p.item).maxStack ? p.count : itemDef(p.item).maxStack));
        if (dropped.type != ItemType::None) {
            Pickup out;
            out.kind = PickupKind::Item;
            out.item = dropped.type;
            out.rarity = dropped.rarity;
            out.count = dropped.count;
            out.position = body(0).state().position + forwardFromYaw(m_input.cameraYaw) * 0.6f;
            out.position.y = body(0).state().position.y;
            // El objeto recogido deja libre su hueco; lo soltado va ahi despues.
            p.count = left;
            if (p.count <= 0) p.active = false;
            dropPickup(out);
        } else {
            p.count = left;
            if (p.count <= 0) p.active = false;
        }
    } else {
        p.count = left;
        if (p.count <= 0) p.active = false;
        // Con las manos libres (pico o ranura vacia) se coge lo recogido.
        if (handsFree) {
            for (int i = 0; i < Inventory::kSlots; ++i) {
                if (m_inventory.slot(i).type == p.item) m_inventory.select(i);
            }
        }
    }
    pushEvent(GameEventType::PickedUp, 0, static_cast<int>(p.item), p.position);
    return true;
}

void Match::autoPickupAmmo(int id) {
    if (id != 0 || !alive(0) || m_air[0] != AirState::Ground) return;
    const Vec3 pos = body(0).state().position;
    for (size_t i = 0; i < m_pickups.size(); ++i) {
        const Pickup& p = m_pickups[i];
        if (!p.active || p.kind != PickupKind::Ammo) continue;
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
        const float radius = fminf(zoneRadius() * 0.6f, 50.0f);
        bool found = false;
        for (int attempt = 0; attempt < 10 && !found; ++attempt) {
            found = m_nav->randomPointNear(m_config.zoneCenter, radius, m_rng, &p) &&
                    world::stormDepth(p.x, p.z) < -10.0f;
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
    Bot& b = m_bots[index];
    if (!b.armed) return;
    Pickup w;
    w.kind = PickupKind::Item;
    w.item = b.weaponType;
    w.rarity = b.weaponRarity;
    w.count = 1;
    const Vec3 pos = b.body.state().position;
    if (b.weaponType != ItemType::None) {
        dropAround(pos, w, 0, 2);
        dropAround(pos, ammoPickup(static_cast<AmmoType>(weaponConfig(b.weaponType, b.weaponRarity).ammoType)), 1, 2);
    }
    b.armed = false;
}

const char* Match::botWeaponName(int i) const {
    const Bot& b = m_bots[i];
    if (!b.armed) return "pico";
    return b.weapon.config().name;
}

}  // namespace game
