// Modelos 3D low-poly de los objetos: todas las armas (cada una con el suyo, a partir de
// los iconos de Recursos/Weapons y Recursos/HUD/Armas y de Referencias/Armas), el pico, los
// consumibles, la granada y los proyectiles.
//
// Un mismo juego de mallas sirve para el arma en la mano de cualquier personaje (no
// depende de la paleta) y para el botin del suelo. Se crean una vez, en dos niveles de
// detalle (el bajo, para los personajes lejanos), con luz horneada en el color de vertice
// y sin textura.
//
// Espacio del arma (metros): el origen es la empunadura (donde la cierra la mano derecha),
// -Z hacia la boca del canon, +Y hacia arriba y +X a la derecha. `support` es donde va la
// mano izquierda (guardamanos) y `muzzle`, la boca del canon.
#pragma once

#include "game/inventory.h"
#include "gfx/mesh.h"
#include "math/math3d.h"

namespace game {

enum class ItemModel : u8 {
    None, Pickaxe,
    AssaultRifle, Scar, BurstRifle, ScopedRifle, SuppressedSmg, Smg, PumpShotgun, TacticalShotgun,
    Pistol, Revolver, BoltSniper, SemiAutoSniper, RocketLauncher, GrenadeLauncher,
    Grenade, Bandage, Medkit, SmallShield, ShieldPotion,
    Rocket, LauncherGrenade,  // proyectiles en vuelo
    Count
};

// Como se sujeta (lo usa la animacion para colocar los brazos).
enum class GripStyle : u8 {
    None,      // nada en la mano (o un consumible: una mano)
    Rifle,     // fusiles, subfusiles, escopetas y francotiradores: dos manos, culata al hombro
    Pistol,    // pistola y revolver: dos manos juntas en la empunadura, brazos estirados
    Shoulder,  // lanzacohetes: sobre el hombro
    Pickaxe,   // una mano, para golpear
    Throw,     // granada: una mano, por encima del hombro
};

struct ItemModelInfo {
    math3d::Vec3 support;  // mano izquierda (espacio del arma)
    math3d::Vec3 muzzle;   // boca del canon
    math3d::Vec3 center;   // centro de la malla (para el botin, que gira sobre si mismo)
    GripStyle grip = GripStyle::None;
};

// Modelo de un objeto del inventario (el fusil de asalto morado o dorado es la SCAR).
ItemModel itemModelFor(ItemType type, Rarity rarity = Rarity::Common);

class ItemModels {
public:
    bool build();
    void destroy();

    // Dibuja el modelo con la matriz dada (espacio del arma -> mundo). Sin textura.
    void draw(ItemModel model, const math3d::Mat4& transform, bool lowDetail = false) const;
    const ItemModelInfo& info(ItemModel model) const { return m_info[static_cast<int>(model)]; }
    int triangles(ItemModel model, bool lowDetail = false) const {
        return m_triangles[lowDetail ? 1 : 0][static_cast<int>(model)];
    }
    bool ready() const { return m_ready; }

private:
    static constexpr int kCount = static_cast<int>(ItemModel::Count);
    gfx::Mesh m_meshes[2][kCount];
    int m_triangles[2][kCount] = {};
    ItemModelInfo m_info[kCount];
    bool m_ready = false;
};

}  // namespace game
