// Generado por tools/prepare_assets.py: no editar a mano.
// Rectangulos (en pixeles) de cada elemento en assets/images/ui_atlas.png.
#pragma once

namespace assets {
namespace ui {

struct Rect {
    short x, y, w, h;
};

constexpr int kAtlasWidth = 512;
constexpr int kAtlasHeight = 144;
constexpr Rect kVictory{0, 0, 292, 46};
constexpr Rect kDefeat{0, 47, 292, 46};
constexpr Rect kLogoSmall{293, 47, 112, 37};
constexpr Rect kJugar{406, 47, 80, 30};
constexpr Rect kControles{294, 94, 96, 22};
constexpr Rect kSalir{391, 94, 51, 22};
constexpr Rect kMapa{0, 122, 163, 22};
constexpr Rect kIniciarSesion{0, 94, 136, 27};
constexpr Rect kCargando{164, 122, 84, 20};
constexpr Rect kConstruccion{249, 122, 116, 17};
constexpr Rect kPico{137, 94, 26, 25};
constexpr Rect kPared{164, 94, 26, 25};
constexpr Rect kRampa{191, 94, 26, 25};
constexpr Rect kSuelo{218, 94, 26, 25};
constexpr Rect kScar{245, 94, 48, 24};
constexpr Rect kMatMadera{366, 122, 16, 16};
constexpr Rect kMatLadrillo{383, 122, 16, 16};
constexpr Rect kMatMetal{400, 122, 16, 16};
constexpr Rect kIconVivos{417, 122, 14, 14};
constexpr Rect kIconBajas{432, 122, 14, 14};
constexpr Rect kIconZona{447, 122, 14, 14};
constexpr Rect kIconEscudo{462, 122, 12, 12};
constexpr Rect kIconVida{475, 122, 12, 12};

}  // namespace ui
}  // namespace assets
