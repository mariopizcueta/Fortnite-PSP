// Generado por tools/prepare_assets.py: no editar a mano.
// Rectangulos (en pixeles) de cada elemento en assets/images/ui_atlas.png.
#pragma once

namespace assets {
namespace ui {

struct Rect {
    short x, y, w, h;
};

constexpr int kAtlasWidth = 512;
constexpr int kAtlasHeight = 160;
constexpr Rect kVictory{0, 0, 292, 46};
constexpr Rect kDefeat{0, 47, 292, 46};
constexpr Rect kLogoSmall{293, 47, 112, 37};
constexpr Rect kJugar{406, 47, 80, 30};
constexpr Rect kControles{394, 94, 96, 22};
constexpr Rect kSalir{0, 122, 51, 22};
constexpr Rect kMapa{52, 122, 163, 22};
constexpr Rect kIniciarSesion{0, 94, 136, 27};
constexpr Rect kCargando{216, 122, 84, 20};
constexpr Rect kConstruccion{301, 122, 116, 17};
constexpr Rect kPico{137, 94, 26, 25};
constexpr Rect kPared{164, 94, 26, 25};
constexpr Rect kRampa{191, 94, 26, 25};
constexpr Rect kSuelo{218, 94, 26, 25};
constexpr Rect kScar{245, 94, 48, 24};
constexpr Rect kMatMadera{418, 122, 16, 16};
constexpr Rect kMatLadrillo{435, 122, 16, 16};
constexpr Rect kMatMetal{452, 122, 16, 16};
constexpr Rect kIconVivos{469, 122, 14, 14};
constexpr Rect kIconBajas{484, 122, 14, 14};
constexpr Rect kIconZona{0, 145, 14, 14};
constexpr Rect kIconEscudo{15, 145, 12, 12};
constexpr Rect kIconVida{28, 145, 12, 12};
constexpr Rect kVendas{294, 94, 24, 24};
constexpr Rect kBotiquin{319, 94, 24, 24};
constexpr Rect kMinipocion{344, 94, 24, 24};
constexpr Rect kPocionEscudo{369, 94, 24, 24};

}  // namespace ui
}  // namespace assets
