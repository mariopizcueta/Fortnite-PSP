// Dibujo de la construccion: paredes, suelos y rampas de madera, piedra o metal, pieza
// fantasma y pieza apuntada.
//
// Las mallas se crean una vez (dos orientaciones, varios tintes) con las medidas de
// BuildConfig y se colocan con GU_MODEL a partir de la MISMA caja de colision de cada
// pared (traslacion + escala en Y si un dintel la ha recortado).
#pragma once

#include <vector>

#include "game/structures.h"
#include "world/scene_builder.h"
#include "world/textures.h"

namespace app {

class BuildVisuals {
public:
    bool build(const game::BuildConfig& config);
    void destroy();

    // Paredes construidas (madera; mas oscura con menos de la mitad de vida y clara un
    // instante al recibir un impacto). Requiere gfx::begin3D().
    void drawWalls(const game::StructureSystem& structures, const world::TextureLibrary& textures) const;
    // Pared fantasma semitransparente: azul si se puede colocar, roja si no.
    void drawGhost(const game::WallPlacement& placement) const;
    // Resalta en rojo la pared que se quitaria con CUADRADO.
    void drawHighlight(const game::BuiltWall& wall) const;

private:
    enum Variant { kNormal, kDamaged, kFlash, kGhostValid, kGhostInvalid, kVariantCount };

    static constexpr int kMaterials = static_cast<int>(world::Resource::Count);

    void drawBox(int material, const world::Aabb& box, bool alongX, Variant variant,
                 const world::TextureLibrary* textures) const;
    void drawPiece(int material, game::PieceType type, const world::Aabb& box, bool alongX, int dir, float level,
                   Variant variant, const world::TextureLibrary* textures, float lift = 0.0f) const;

    // [material][variante][0 = a lo largo de X, 1 = a lo largo de Z]
    std::vector<world::StaticBatch> m_meshes[kMaterials][kVariantCount][2];
    std::vector<world::StaticBatch> m_floor[kMaterials][kVariantCount];
    std::vector<world::StaticBatch> m_ramp[kMaterials][kVariantCount];  // sube hacia +X desde el origen
    game::BuildConfig m_config;
};

// Numeros grandes con una fuente de 3x5 bloques dibujados con GU (municion, dano).
// Devuelve el ancho dibujado en pixeles.
int drawBigNumber(float x, float y, int value, float scale, u32 color, bool shadow = true);
int bigNumberWidth(int value, float scale);

}  // namespace app
