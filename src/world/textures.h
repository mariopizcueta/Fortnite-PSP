// Texturas procedurales de baja resolucion para el escenario (estetica PS1).
//
// Todas son de 32x32 texeles en formato GU_PSM_5551 (15 bits de color, como PS1) y se
// generan una sola vez al arrancar. Las de metal, contenedor y persiana son neutras
// (grises) y se tintan con el color de vertice (GU_TFX_MODULATE), asi una misma
// textura sirve para naves azules, rojas o amarillas.
#pragma once

#include "gfx/texture.h"

namespace world {

enum TextureId : int {
    kTexNone = -1,  // color plano, sin textura
    kTexGrass = 0,
    kTexDirt,
    kTexConcrete,
    kTexMetalSiding,
    kTexRoofPanel,
    kTexContainer,
    kTexWood,
    kTexChecker,
    kTexShutter,
    kTexOfficeWindow,
    kTexCamo,  // camuflaje del pantalon del jugador (Referencias/Personaje)
    kTexWater,
    kTexRock,
    kTexSand,
    kTexBlockWall,  // muro de bloques de hormigon (perimetro de las naves)
    kTexWoodWall,   // tablones marrones de las paredes que construye el jugador
    kTexCount
};

class TextureLibrary {
public:
    bool build();
    void destroy();

    // nullptr para kTexNone o ids invalidos.
    const gfx::Texture* get(int id) const;
    size_t totalBytes() const;

private:
    gfx::Texture m_textures[kTexCount];
};

}  // namespace world
