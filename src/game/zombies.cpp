#include "game/zombies.h"

#include <math.h>

namespace game {
namespace {

using i18n::T;

//                          nombre             vida   dia   noche  dano  pieza  ritmo  alcance  tamano
const ZombieDef kZombies[] = {
    {T::ZombieWalker, 100.0f, 0.42f, 0.56f, 14.0f, 40.0f, 1.10f, 1.45f, 1.0f},
    {T::ZombieRunner, 65.0f, 0.78f, 0.96f, 9.0f, 25.0f, 0.80f, 1.40f, 0.95f},
    {T::ZombieBrute, 420.0f, 0.36f, 0.46f, 32.0f, 120.0f, 1.60f, 1.90f, 1.3f},
    {T::ZombieFallen, 170.0f, 0.40f, 0.50f, 12.0f, 45.0f, 1.20f, 1.45f, 1.0f},
};
static_assert(sizeof(kZombies) / sizeof(kZombies[0]) == static_cast<int>(ZombieType::Count), "falta un zombi");

// Lo que se puede fabricar en un centro de crafteo (madera, piedra, metal). En el botin del
// modo zombi solo hay escopetas, pistolas y curativos: el resto sale de aqui.
const Recipe kRecipes[] = {
    {T::RecipeRifle, ItemType::AssaultRifle, Rarity::Rare, 1, {40, 0, 60}},
    {T::RecipeSmg, ItemType::Smg, Rarity::Uncommon, 1, {30, 0, 45}},
    {T::RecipeSniper, ItemType::Sniper, Rarity::Rare, 1, {0, 40, 90}},
    {T::RecipeRocket, ItemType::RocketLauncher, Rarity::Epic, 1, {0, 80, 150}},
    {T::RecipeGrenades, ItemType::Grenade, Rarity::Common, 3, {0, 30, 30}},
    {T::RecipeTraps, ItemType::SpikeTrap, Rarity::Uncommon, 2, {30, 0, 20}},
    {T::RecipeBandages, ItemType::Bandage, Rarity::Common, 5, {30, 0, 0}},
    {T::RecipeMedkit, ItemType::Medkit, Rarity::Uncommon, 1, {40, 20, 0}},
    {T::RecipeAmmo, ItemType::None, Rarity::Common, 2, {0, 0, 15}},
    {T::RecipeBed, ItemType::Bed, Rarity::Rare, 1, {50, 0, 15}},
};

// Planos de la base (x 56..68, z 28..40, celdas de 4 m). Muros del perimetro (la puerta,
// x 60..64 al sur, queda abierta), una torre de vigilancia con rampa en la esquina noreste
// y un refugio techado.
const Blueprint kBlueprints[] = {
    {T::BlueprintNorthWest, T::BlueprintNorthWestDesc, 6,
     {{PieceType::Wall, 56, 28, 0, true, 0}, {PieceType::Wall, 60, 28, 0, true, 0}, {PieceType::Wall, 64, 28, 0, true, 0},
      {PieceType::Wall, 56, 28, 0, false, 0}, {PieceType::Wall, 56, 32, 0, false, 0}, {PieceType::Wall, 56, 36, 0, false, 0}}},
    {T::BlueprintEastSouth, T::BlueprintEastSouthDesc, 5,
     {{PieceType::Wall, 68, 28, 0, false, 0}, {PieceType::Wall, 68, 32, 0, false, 0}, {PieceType::Wall, 68, 36, 0, false, 0},
      {PieceType::Wall, 56, 40, 0, true, 0}, {PieceType::Wall, 64, 40, 0, true, 0}}},
    {T::BlueprintTower, T::BlueprintTowerDesc, 4,
     {{PieceType::Ramp, 64, 32, 0, true, 3}, {PieceType::Floor, 64, 28, 1, true, 0}, {PieceType::Wall, 64, 28, 1, true, 0},
      {PieceType::Wall, 68, 28, 1, false, 0}}},
    {T::BlueprintShelter, T::BlueprintShelterDesc, 4,
     {{PieceType::Floor, 56, 32, 1, true, 0}, {PieceType::Floor, 60, 32, 1, true, 0}, {PieceType::Floor, 56, 36, 1, true, 0},
      {PieceType::Floor, 60, 36, 1, true, 0}}},
};
static_assert(sizeof(kBlueprints) / sizeof(kBlueprints[0]) == kBlueprintCount, "un plano por sitio del mapa");

}  // namespace

const ZombieDef& zombieDef(ZombieType type) {
    const int i = static_cast<int>(type);
    return kZombies[i >= 0 && i < static_cast<int>(ZombieType::Count) ? i : 0];
}

int recipeCount() { return static_cast<int>(sizeof(kRecipes) / sizeof(kRecipes[0])); }

const Recipe& recipe(int index) { return kRecipes[index >= 0 && index < recipeCount() ? index : 0]; }

const Blueprint& blueprint(int index) { return kBlueprints[index >= 0 && index < kBlueprintCount ? index : 0]; }

WallPlacement blueprintPlacement(const BlueprintPiece& piece, const BuildConfig& config) {
    WallPlacement p;
    p.valid = true;
    p.type = piece.type;
    const float g = config.gridSize, h = config.wallHeight, t = config.wallThickness * 0.5f;
    const float level = piece.floor * h;
    p.level = level;
    p.alongX = piece.alongX;
    p.dir = piece.dir;
    switch (piece.type) {
        case PieceType::Wall:
            p.box = piece.alongX ? world::Aabb{{piece.x0, level, piece.z0 - t}, {piece.x0 + g, level + h, piece.z0 + t}}
                                 : world::Aabb{{piece.x0 - t, level, piece.z0}, {piece.x0 + t, level + h, piece.z0 + g}};
            break;
        case PieceType::Floor:
            p.box = {{piece.x0, level, piece.z0}, {piece.x0 + g, level + config.floorThickness, piece.z0 + g}};
            break;
        case PieceType::Ramp: {
            p.alongX = piece.dir >= 2;
            p.box = {{piece.x0, level, piece.z0}, {piece.x0 + g, level + h, piece.z0 + g}};
            // Como StructureSystem::planCell: la caja engloba los escalones.
            world::Aabb boxes[kMaxPieceBoxes];
            const int n = pieceBoxes(p, config, boxes);
            p.box = boxes[0];
            for (int i = 1; i < n; ++i) {
                p.box.min = {fminf(p.box.min.x, boxes[i].min.x), fminf(p.box.min.y, boxes[i].min.y),
                             fminf(p.box.min.z, boxes[i].min.z)};
                p.box.max = {fmaxf(p.box.max.x, boxes[i].max.x), fmaxf(p.box.max.y, boxes[i].max.y),
                             fmaxf(p.box.max.z, boxes[i].max.z)};
            }
            break;
        }
    }
    return p;
}

}  // namespace game
