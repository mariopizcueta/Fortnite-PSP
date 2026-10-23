// Generado por tools/prepare_assets.py: no editar a mano.
// Rectangulos (en pixeles) de cada elemento en assets/images/ui_atlas.png.
#pragma once

namespace assets {
namespace ui {

struct Rect {
    short x, y, w, h;
};

constexpr int kAtlasWidth = 512;
constexpr int kAtlasHeight = 168;
constexpr Rect kVictory{0, 0, 292, 46};
constexpr Rect kDefeat{0, 47, 292, 46};
constexpr Rect kLogoSmall{293, 47, 112, 37};
constexpr Rect kJugar{0, 94, 80, 30};
constexpr Rect kControles{401, 94, 96, 22};
constexpr Rect kSalir{0, 125, 51, 22};
constexpr Rect kMapa{52, 125, 163, 22};
constexpr Rect kIniciarSesion{81, 94, 136, 27};
constexpr Rect kCargando{216, 125, 84, 20};
constexpr Rect kConstruccion{339, 125, 116, 17};
constexpr Rect kPico{218, 94, 26, 25};
constexpr Rect kPared{245, 94, 26, 25};
constexpr Rect kRampa{272, 94, 26, 25};
constexpr Rect kSuelo{299, 94, 26, 25};
constexpr Rect kSlotScar{406, 47, 32, 32};
constexpr Rect kSlotMinipocion{439, 47, 32, 32};
constexpr Rect kMatMadera{301, 125, 18, 18};
constexpr Rect kMatLadrillo{456, 125, 18, 15};
constexpr Rect kMatMetal{320, 125, 18, 18};
constexpr Rect kIconVivos{475, 125, 14, 14};
constexpr Rect kIconBajas{490, 125, 14, 14};
constexpr Rect kIconZona{0, 148, 14, 14};
constexpr Rect kIconEscudo{15, 148, 12, 12};
constexpr Rect kIconVida{28, 148, 12, 12};
constexpr Rect kVendas{326, 94, 24, 24};
constexpr Rect kBotiquin{351, 94, 24, 24};
constexpr Rect kPocionEscudo{376, 94, 24, 24};

}  // namespace ui
}  // namespace assets
