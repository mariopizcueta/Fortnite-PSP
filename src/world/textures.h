// Texturas de baja resolucion para el escenario (estetica PS1).
//
// Desde la fase 6 se cargan de Recursos/Texturas (atlas incrustado generado por
// tools/generate_textures.py y retocable a mano). v0.14: 64x64 texeles cada una (antes
// 32x32: el doble de detalle con el mismo tamano en el mundo) y 24 texturas en un atlas de
// 256x384; las 8 ultimas son las del modo zombi (oxido, hierba seca, agua turbia...). Si el
// atlas no se pudiera decodificar, se generan por procedimiento (de 32x32, como antes).
//
// Formato GU_PSM_5551 (15 bits de color, como PS1); se cargan una sola vez al arrancar. Las de metal, contenedor y persiana son neutras
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
    // v0.14: modo zombi. Las 7 primeras sustituyen a las normales con el tema "en ruinas".
    kTexDeadGrass,
    kTexRustSiding,
    kTexRustRoof,
    kTexRustContainer,
    kTexMurkyWater,
    kTexCrackedConcrete,
    kTexRottenWood,
    kTexCanvas,  // lona y sacos terreros (tiendas de los campamentos)
    kTexCount
};

// Tema del escenario: el normal o el del modo zombi (Polvorin en ruinas: metal oxidado,
// hierba seca, agua turbia, hormigon agrietado y madera podrida).
enum class TextureTheme : u8 { Normal, Ruined };

class TextureLibrary {
public:
    bool build();
    void destroy();
    // true si las texturas vienen del atlas de Recursos/Texturas (no las procedurales).
    bool fromResources() const { return m_fromResources; }

    // nullptr para kTexNone o ids invalidos. Con el tema en ruinas, las normales que tienen
    // version oxidada devuelven esa.
    const gfx::Texture* get(int id) const;
    size_t totalBytes() const;
    void setTheme(TextureTheme theme) { m_theme = theme; }
    TextureTheme theme() const { return m_theme; }

private:
    bool buildFromAtlas();

    gfx::Texture m_textures[kTexCount];
    bool m_fromResources = false;
    TextureTheme m_theme = TextureTheme::Normal;
};

}  // namespace world
