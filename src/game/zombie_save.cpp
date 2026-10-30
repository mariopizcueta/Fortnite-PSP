// Partida guardada del modo Supervivencia zombi (v0.15, docs/ZOMBIS.md).
//
// Un fichero de texto (partida_zombis.txt junto al EBOOT): una linea por dato, con una
// palabra delante. Se guarda lo que hace falta para seguir: el dia y la hora, el jugador
// (posicion, vida, hambre, descanso, inventario, municion y materiales), las piezas
// construidas, los planos, los lugares descubiertos, los objetivos, los supervivientes y sus
// inventarios, lo abierto, las camas, los objetos del suelo y lo derruido del mapa. Los
// zombis no: al cargar, la noche (o el dia) vuelve a poblarse sola.
//
// Para que el botin y los contenedores esten en el mismo sitio, la partida se carga con la
// misma semilla (peekZombieSave) y despues se aplica el fichero (loadZombies).
#include "game/match.h"

#include <stdio.h>
#include <string.h>

#include "game/match_internal.h"

namespace game {

using math3d::Vec3;
using namespace tuning;
using namespace detail;

namespace {

constexpr int kSaveVersion = 1;
constexpr const char* kMagic = "fortnitepsp_zombis";

// Estados de un superviviente en el fichero.
enum SurvivorSave { kWaiting = 0, kAlly = 1, kFallen = 2, kGone = 3 };

void tempPath(const char* path, char* out, size_t size) { snprintf(out, size, "%s.tmp", path); }

bool validItem(int type) { return type > 0 && type < static_cast<int>(ItemType::Count); }
bool validRarity(int r) { return r >= 0 && r < static_cast<int>(Rarity::Count); }

}  // namespace

bool Match::peekZombieSave(const char* path, int* day, u32* seed) {
    char tmp[300];
    tempPath(path, tmp, sizeof(tmp));
    FILE* f = fopen(path, "r");
    if (!f) f = fopen(tmp, "r");  // se corto al guardar justo despues de borrar el bueno
    if (!f) return false;
    char line[160];
    int version = 0, d = 0;
    unsigned s = 0;
    bool magic = false, hasSeed = false;
    while (fgets(line, sizeof(line), f)) {
        char word[32];
        if (sscanf(line, "%31s", word) != 1) continue;
        if (strcmp(word, kMagic) == 0) magic = sscanf(line, "%*s %d", &version) == 1 && version == kSaveVersion;
        else if (strcmp(word, "semilla") == 0) hasSeed = sscanf(line, "%*s %u", &s) == 1;
        else if (strcmp(word, "dia") == 0) sscanf(line, "%*s %d", &d);
    }
    fclose(f);
    if (!magic || !hasSeed || d < 1) return false;
    if (day) *day = d;
    if (seed) *seed = s;
    return true;
}

bool Match::eraseZombieSave(const char* path) {
    char tmp[300];
    tempPath(path, tmp, sizeof(tmp));
    remove(tmp);
    remove(path);
    FILE* f = fopen(path, "r");
    if (f) fclose(f);
    return f == nullptr;
}

bool Match::saveZombies(const char* path, int vbucks) const {
    if (!zombieMode() || !alive(0)) return false;
    auto write = [&](const char* file) {
        FILE* f = fopen(file, "w");
        if (!f) return false;
        bool ok = fprintf(f, "%s %d\n", kMagic, kSaveVersion) > 0;
        ok &= fprintf(f, "semilla %u\n", static_cast<unsigned>(m_config.seed)) > 0;
        ok &= fprintf(f, "dia %d\n", m_day) > 0;
        ok &= fprintf(f, "reloj %d %.2f %d\n", m_night ? 1 : 0, m_zClock, m_nightsSurvived) > 0;
        ok &= fprintf(f, "marcador %d %d %d %d %d\n", m_zombieKills, m_itemsCrafted, m_rescuedTotal, m_objectivesDone, vbucks) > 0;
        const CharacterState& s = m_player.state();
        ok &= fprintf(f, "jugador %.2f %.2f %.2f %.3f %.1f %.1f %.2f %.2f\n", s.position.x, s.position.y, s.position.z, s.yaw,
                      s.health, s.shield, m_food, m_rest) > 0;
        ok &= fprintf(f, "inventario %d %d %d %d\n", m_inventory.slotCount(), m_inventory.selected(), m_inventory.pickaxe() ? 1 : 0,
                      m_trapStock) > 0;
        for (int i = 0; i < m_inventory.slotCount(); ++i) {
            const InventorySlot& slot = m_inventory.slot(i);
            if (slot.type == ItemType::None) continue;
            ok &= fprintf(f, "ranura %d %d %d %d %d\n", i, static_cast<int>(slot.type), static_cast<int>(slot.rarity), slot.count,
                          isWeapon(slot.type) ? slot.weapon.ammo() : -1) > 0;
        }
        ok &= fprintf(f, "municion") > 0;
        for (int a = 0; a < static_cast<int>(AmmoType::Count); ++a) ok &= fprintf(f, " %d", m_inventory.ammo(static_cast<AmmoType>(a))) > 0;
        ok &= fprintf(f, "\nmateriales %d %d %d\n", m_materials.amount[0], m_materials.amount[1], m_materials.amount[2]) > 0;
        for (int i = 0; i < kBlueprintCount; ++i) {
            ok &= fprintf(f, "plano %d %d %d\n", i, m_bpFound[i] ? 1 : 0, m_bpDone[i] ? 1 : 0) > 0;
        }
        for (int i = 0; i < world::zsites::kSiteCount; ++i) {
            ok &= fprintf(f, "lugar %d %d %d\n", i, m_siteFound[i] ? 1 : 0, m_siteGuardDay[i]) > 0;
        }
        for (int i = 0; i < kActiveObjectives; ++i) {
            const Objective& o = m_objectives[i];
            ok &= fprintf(f, "objetivo %d %d %d %d %d %d\n", i, static_cast<int>(o.type), o.target, o.doneTime >= 0.0f ? 0 : o.progress,
                          o.site, static_cast<int>(o.reward)) > 0;
        }
        // Piezas construidas (las de los planos, con su plano y pieza).
        for (int w = 0; w < m_structures.capacity(); ++w) {
            const BuiltWall& wall = m_structures.wall(w);
            if (!wall.active) continue;
            int bp = -1, piece = -1;
            for (int b = 0; b < kBlueprintCount && bp < 0; ++b) {
                for (int k = 0; k < blueprint(b).pieceCount; ++k) {
                    if (m_bpWall[b][k] == w && pieceBuilt(b, k)) {
                        bp = b;
                        piece = k;
                        break;
                    }
                }
            }
            ok &= fprintf(f, "pieza %d %.3f %.3f %.3f %.3f %.3f %.3f %d %d %.3f %d %.1f %.1f %d %d %d\n", static_cast<int>(wall.type),
                          wall.box.min.x, wall.box.min.y, wall.box.min.z, wall.box.max.x, wall.box.max.y, wall.box.max.z,
                          wall.alongX ? 1 : 0, wall.dir, wall.level, static_cast<int>(wall.material), wall.health, wall.maxHealth,
                          wall.owner, bp, piece) > 0;
        }
        // Supervivientes (y aliados caidos).
        for (int i = kZombieSlots; i < botCount(); ++i) {
            const Bot& b = m_bots[i];
            const bool dead = b.state == BotState::Eliminated;
            int state = kWaiting;
            if (b.kind == BotKind::Zombie) state = dead ? kGone : kFallen;
            else if (dead) state = b.riseTimer > 0.0f ? kFallen : kGone;
            else if (b.rescued) state = kAlly;
            const CharacterState& bs = b.body.state();
            ok &= fprintf(f, "superviviente %d %d %d %.2f %.2f %.2f %.1f %d\n", i - kZombieSlots, state, b.discovered ? 1 : 0,
                          bs.position.x, bs.position.y, bs.position.z, dead ? 0.0f : bs.health, b.bed) > 0;
            // v0.16: escudo, mochilas y materiales de los aliados (va antes que su bolsa).
            if (state == kAlly) {
                ok &= fprintf(f, "aliado %d %.1f %d %d %d %d\n", i - kZombieSlots, bs.shield, b.bag.slotCount(), b.materials.amount[0],
                              b.materials.amount[1], b.materials.amount[2]) > 0;
            }
            for (int k = 0; k < b.bag.slotCount(); ++k) {
                const InventorySlot& slot = b.bag.slot(k);
                if (slot.type == ItemType::None) continue;
                ok &= fprintf(f, "bolsa %d %d %d %d %d %d\n", i - kZombieSlots, k, static_cast<int>(slot.type),
                              static_cast<int>(slot.rarity), slot.count, isWeapon(slot.type) ? slot.weapon.ammo() : -1) > 0;
            }
        }
        // Contenedores: abiertos y descubiertos (las camas, aparte: se anaden al cargar).
        for (size_t i = 0; i < m_containers.size(); ++i) {
            const Container& c = m_containers[i];
            if (c.kind == ContainerKind::Bed) {
                ok &= fprintf(f, "cama %.2f %.2f %.2f %.3f\n", c.position.x, c.position.y, c.position.z, c.yaw) > 0;
            } else if (c.opened || c.discovered) {
                ok &= fprintf(f, "contenedor %d %d %d %d\n", static_cast<int>(i), static_cast<int>(c.kind), c.opened ? 1 : 0,
                              c.discovered ? 1 : 0) > 0;
            }
        }
        for (const Pickup& p : m_pickups) {
            if (!p.active) continue;
            ok &= fprintf(f, "objeto %d %d %d %d %d %d %d %.2f %.2f %.2f\n", static_cast<int>(p.kind), static_cast<int>(p.item),
                          static_cast<int>(p.rarity), static_cast<int>(p.ammo), p.count, p.loaded, static_cast<int>(p.resource),
                          p.position.x, p.position.y, p.position.z) > 0;
        }
        for (size_t i = 0; i < m_objectDestroyed.size(); ++i) {
            if (m_objectDestroyed[i]) ok &= fprintf(f, "roto %d\n", static_cast<int>(i)) > 0;
        }
        ok &= fprintf(f, "fin\n") > 0;
        ok &= fflush(f) == 0;
        ok &= fclose(f) == 0;
        return ok;
    };
    // Como el perfil: primero el temporal; en la Memory Stick renombrar sobre un fichero que
    // existe falla, asi que se borra antes.
    char tmp[300];
    tempPath(path, tmp, sizeof(tmp));
    if (!write(tmp)) {
        remove(tmp);
        return false;
    }
    remove(path);
    if (rename(tmp, path) == 0) return true;
    const bool ok = write(path);
    if (ok) remove(tmp);
    return ok;
}

bool Match::loadZombies(const char* path, int* vbucks) {
    if (!zombieMode()) return false;
    char tmp[300];
    tempPath(path, tmp, sizeof(tmp));
    FILE* f = fopen(path, "r");
    if (!f) f = fopen(tmp, "r");
    if (!f) return false;
    char line[200];
    if (!fgets(line, sizeof(line), f)) {
        fclose(f);
        return false;
    }
    int version = 0;
    char magic[32];
    if (sscanf(line, "%31s %d", magic, &version) != 2 || strcmp(magic, kMagic) != 0 || version != kSaveVersion) {
        fclose(f);
        return false;
    }
    // Lo que trae el fichero sustituye a lo de una partida recien empezada.
    m_pickups.clear();
    m_inventory.clear();
    m_materials = Materials{};
    bool ended = false;
    const int survivors = botCount() - kZombieSlots;
    while (fgets(line, sizeof(line), f)) {
        char word[24];
        if (sscanf(line, "%23s", word) != 1) continue;
        const char* rest = line + strlen(word);
        if (strcmp(word, "dia") == 0) {
            int d = 1;
            if (sscanf(rest, "%d", &d) == 1 && d >= 1) m_day = d;
        } else if (strcmp(word, "reloj") == 0) {
            int night = 0, nights = 0;
            float clock = 0.0f;
            if (sscanf(rest, "%d %f %d", &night, &clock, &nights) == 3) {
                m_night = night != 0;
                const float length = m_night ? m_config.zombie.nightLength : m_config.zombie.dayLength;
                m_zClock = math3d::clampf(clock, 0.0f, length - 1.0f);
                m_nightsSurvived = nights < 0 ? 0 : nights;
            }
        } else if (strcmp(word, "marcador") == 0) {
            int kills = 0, crafted = 0, rescued = 0, done = 0, pavos = 0;
            if (sscanf(rest, "%d %d %d %d %d", &kills, &crafted, &rescued, &done, &pavos) == 5) {
                m_zombieKills = kills;
                m_itemsCrafted = crafted;
                m_rescuedTotal = rescued;
                m_objectivesDone = done;
                if (vbucks) *vbucks = pavos;
            }
        } else if (strcmp(word, "jugador") == 0) {
            Vec3 p;
            float yaw, health, shield, food, rest2;
            if (sscanf(rest, "%f %f %f %f %f %f %f %f", &p.x, &p.y, &p.z, &yaw, &health, &shield, &food, &rest2) == 8) {
                m_player.reset(p, yaw, *m_world);
                CharacterState& s = m_player.state();
                s.health = math3d::clampf(health, 1.0f, s.maxHealth);
                s.shield = math3d::clampf(shield, 0.0f, s.maxShield);
                m_food = math3d::clampf(food, 0.0f, 100.0f);
                m_rest = math3d::clampf(rest2, 0.0f, 100.0f);
                m_hungerStage = m_food <= 0.0f ? 2 : (m_food < 25.0f ? 1 : 0);
                m_tiredStage = m_rest <= 0.0f ? 2 : (m_rest < m_config.zombie.tiredLevel ? 1 : 0);
            }
        } else if (strcmp(word, "inventario") == 0) {
            int slots = Inventory::kSlots, selected = 0, pickaxe = 1, traps = 0;
            if (sscanf(rest, "%d %d %d %d", &slots, &selected, &pickaxe, &traps) == 4) {
                m_inventory.setSlotCount(slots);
                m_inventory.select(selected);
                m_inventory.setPickaxe(pickaxe != 0);
                m_trapStock = traps < 0 ? 0 : (traps > m_config.maxTrapsCarried ? m_config.maxTrapsCarried : traps);
            }
        } else if (strcmp(word, "ranura") == 0) {
            int i, type, rarity, count, loaded;
            if (sscanf(rest, "%d %d %d %d %d", &i, &type, &rarity, &count, &loaded) == 5 && i >= 0 && i < m_inventory.slotCount() &&
                validItem(type) && validRarity(rarity) && count > 0) {
                InventorySlot dropped;
                m_inventory.place(i, static_cast<ItemType>(type), static_cast<Rarity>(rarity), count, loaded, &dropped);
            }
        } else if (strcmp(word, "municion") == 0) {
            int a[static_cast<int>(AmmoType::Count)] = {};
            if (sscanf(rest, "%d %d %d %d %d", &a[0], &a[1], &a[2], &a[3], &a[4]) == 5) {
                for (int k = 0; k < static_cast<int>(AmmoType::Count); ++k) {
                    m_inventory.setAmmo(static_cast<AmmoType>(k), a[k] < 0 ? 0 : (a[k] > Inventory::kMaxAmmo ? Inventory::kMaxAmmo : a[k]));
                }
            }
        } else if (strcmp(word, "materiales") == 0) {
            int w = 0, st = 0, m = 0;
            if (sscanf(rest, "%d %d %d", &w, &st, &m) == 3) {
                m_materials.add(world::Resource::Wood, w > 0 ? w : 0);
                m_materials.add(world::Resource::Stone, st > 0 ? st : 0);
                m_materials.add(world::Resource::Metal, m > 0 ? m : 0);
            }
        } else if (strcmp(word, "plano") == 0) {
            int i, found, done;
            if (sscanf(rest, "%d %d %d", &i, &found, &done) == 3 && i >= 0 && i < kBlueprintCount) {
                m_bpFound[i] = found != 0;
                m_bpDone[i] = done != 0;
            }
        } else if (strcmp(word, "lugar") == 0) {
            int i, found, guardDay;
            if (sscanf(rest, "%d %d %d", &i, &found, &guardDay) == 3 && i >= 0 && i < world::zsites::kSiteCount) {
                m_siteFound[i] = found != 0;
                m_siteGuardDay[i] = guardDay;
            }
        } else if (strcmp(word, "objetivo") == 0) {
            int i, type, target, progress, site, reward;
            if (sscanf(rest, "%d %d %d %d %d %d", &i, &type, &target, &progress, &site, &reward) == 6 && i >= 0 &&
                i < kActiveObjectives && type >= 0 && type < static_cast<int>(ObjectiveType::Count) && target > 0 &&
                reward >= 0 && reward < static_cast<int>(RewardType::Count)) {
                Objective& o = m_objectives[i];
                o = Objective{};
                o.type = static_cast<ObjectiveType>(type);
                o.target = target;
                o.progress = progress < 0 ? 0 : (progress >= target ? target - 1 : progress);
                o.site = site >= 0 && site < world::zsites::kSiteCount ? site : 0;
                o.reward = static_cast<RewardType>(reward);
            }
        } else if (strcmp(word, "pieza") == 0) {
            int type, alongX, dir, material, owner, bp, piece;
            float level, health, maxHealth;
            world::Aabb box;
            if (sscanf(rest, "%d %f %f %f %f %f %f %d %d %f %d %f %f %d %d %d", &type, &box.min.x, &box.min.y, &box.min.z,
                       &box.max.x, &box.max.y, &box.max.z, &alongX, &dir, &level, &material, &health, &maxHealth, &owner, &bp,
                       &piece) == 16 &&
                type >= 0 && type <= static_cast<int>(PieceType::Ramp) && material >= 0 &&
                material < static_cast<int>(world::Resource::Count) && maxHealth > 0.0f) {
                WallPlacement p;
                p.valid = true;
                p.type = static_cast<PieceType>(type);
                p.box = box;
                p.alongX = alongX != 0;
                p.dir = dir;
                p.level = level;
                p.material = static_cast<world::Resource>(material);
                const BuildConfig& c = m_config.build;
                const float base = p.material == world::Resource::Metal   ? c.metalHealth
                                   : p.material == world::Resource::Stone ? c.stoneHealth
                                                                          : c.wallHealth;
                // Solo las del jugador, las de los planos y las de los aliados.
                const int keep = owner == 0 || owner == kBlueprintOwner ? owner : 0;
                const int wall = m_structures.placeForced(p, keep, maxHealth / base);
                if (wall >= 0) {
                    m_structures.setHealth(wall, health);
                    if (bp >= 0 && bp < kBlueprintCount && piece >= 0 && piece < blueprint(bp).pieceCount) {
                        m_bpWall[bp][piece] = static_cast<short>(wall);
                    }
                }
            }
        } else if (strcmp(word, "superviviente") == 0) {
            int k, state, discovered, bed;
            Vec3 p;
            float health;
            if (sscanf(rest, "%d %d %d %f %f %f %f %d", &k, &state, &discovered, &p.x, &p.y, &p.z, &health, &bed) == 8 && k >= 0 &&
                k < survivors) {
                Bot& b = m_bots[kZombieSlots + k];
                const int self = kZombieSlots + k + 1;
                b.discovered = discovered != 0;
                b.bag.clear();
                if (state == kWaiting) continue;  // sigue en su campamento
                if (state == kGone) {
                    b.state = BotState::Eliminated;
                    b.eliminatedAt = -1000.0f;
                    b.lootDropped = true;
                    b.rescued = true;
                    b.body.place({p.x, -50.0f, p.z});
                    --m_alive;
                    continue;
                }
                b.body.reset(p, b.body.state().yaw, *m_world);
                if (state == kAlly) {
                    b.rescued = true;
                    b.discovered = true;
                    b.bed = bed;
                    b.state = BotState::Search;
                    b.searchDuration = 0.5f;
                    b.body.state().health = math3d::clampf(health, 1.0f, b.body.state().maxHealth);
                } else {
                    // Caido y levantado: el zombi armado sigue ahi (con lo que lleve, mas abajo).
                    turnFallen(kZombieSlots + k);
                    --m_alive;  // turnFallen lo cuenta como si volviera a la vida
                    m_bots[kZombieSlots + k].body.state().health = math3d::clampf(health, 1.0f, zombieDef(ZombieType::Fallen).health);
                }
                (void)self;
            }
        } else if (strcmp(word, "aliado") == 0) {
            int k, slots, wood, stone, metal;
            float shield;
            if (sscanf(rest, "%d %f %d %d %d %d", &k, &shield, &slots, &wood, &stone, &metal) == 6 && k >= 0 && k < survivors) {
                Bot& b = m_bots[kZombieSlots + k];
                b.body.state().shield = math3d::clampf(shield, 0.0f, b.body.state().maxShield);
                b.bag.setSlotCount(slots);
                b.materials = Materials{};
                b.materials.add(world::Resource::Wood, wood > 0 ? wood : 0);
                b.materials.add(world::Resource::Stone, stone > 0 ? stone : 0);
                b.materials.add(world::Resource::Metal, metal > 0 ? metal : 0);
            }
        } else if (strcmp(word, "bolsa") == 0) {
            int k, i, type, rarity, count, loaded;
            if (sscanf(rest, "%d %d %d %d %d %d", &k, &i, &type, &rarity, &count, &loaded) == 6 && k >= 0 && k < survivors &&
                validItem(type) && validRarity(rarity) && count > 0) {
                Bot& b = m_bots[kZombieSlots + k];
                b.bag.add(static_cast<ItemType>(type), static_cast<Rarity>(rarity), count, loaded);
                b.armed = false;
                botEquip(b, 15.0f);
            }
        } else if (strcmp(word, "contenedor") == 0) {
            int i, kind, opened, discovered;
            if (sscanf(rest, "%d %d %d %d", &i, &kind, &opened, &discovered) == 4 && i >= 0 &&
                i < static_cast<int>(m_containers.size()) && static_cast<int>(m_containers[i].kind) == kind) {
                m_containers[i].opened = opened != 0;
                m_containers[i].openedAt = -100.0f;
                m_containers[i].discovered = discovered != 0;
            }
        } else if (strcmp(word, "cama") == 0) {
            Vec3 p;
            float yaw;
            if (sscanf(rest, "%f %f %f %f", &p.x, &p.y, &p.z, &yaw) == 4 && bedCount() < kMaxBeds) {
                addContainer(ContainerKind::Bed, p, yaw);
            }
        } else if (strcmp(word, "objeto") == 0) {
            int kind, item, rarity, ammo, count, loaded, resource;
            Pickup p;
            if (sscanf(rest, "%d %d %d %d %d %d %d %f %f %f", &kind, &item, &rarity, &ammo, &count, &loaded, &resource, &p.position.x,
                       &p.position.y, &p.position.z) == 10 &&
                kind >= 0 && kind <= static_cast<int>(PickupKind::Backpack) && item >= 0 && item < static_cast<int>(ItemType::Count) &&
                validRarity(rarity) && ammo >= 0 && ammo < static_cast<int>(AmmoType::Count) && resource >= 0 &&
                resource < static_cast<int>(world::Resource::Count) && count > 0) {
                p.active = true;
                p.kind = static_cast<PickupKind>(kind);
                p.item = static_cast<ItemType>(item);
                p.rarity = static_cast<Rarity>(rarity);
                p.ammo = static_cast<AmmoType>(ammo);
                p.count = count;
                p.loaded = loaded;
                p.resource = static_cast<world::Resource>(resource);
                dropPickup(p);
            }
        } else if (strcmp(word, "roto") == 0) {
            int i;
            if (sscanf(rest, "%d", &i) == 1 && i >= 0 && i < static_cast<int>(m_objectDestroyed.size()) && !m_objectDestroyed[i]) {
                const world::Harvestable& h = (*m_harvestables)[i];
                m_objectHealth[i] = 0.0f;
                m_objectDestroyed[i] = 1;
                for (int c = 0; c < h.colliderCount; ++c) m_world->setStaticEnabled(h.collider + c, false);
                m_nav->updateRegion(*m_world, h.box);
            }
        } else if (strcmp(word, "fin") == 0) {
            ended = true;
        }
    }
    fclose(f);
    // Camas de los aliados: solo si existen (y se marcan como ocupadas).
    for (int i = kZombieSlots; i < botCount(); ++i) {
        Bot& b = m_bots[i];
        if (b.kind != BotKind::Survivor || b.bed < 0) continue;
        if (b.bed >= static_cast<int>(m_containers.size()) || m_containers[b.bed].kind != ContainerKind::Bed) b.bed = -1;
        else m_containers[b.bed].claimedBy = i;
    }
    if (m_inventory.activeType() == ItemType::None && !m_inventory.pickaxe()) m_inventory.setPickaxe(true);
    onStructuresChanged();
    m_eventCount = 0;
    m_spawnTimer = 4.0f;
    return ended;
}

}  // namespace game
