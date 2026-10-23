// Acceso a los recursos incrustados mediante embedded_assets.S.
#pragma once

#include <stddef.h>

extern "C" {
extern const unsigned char asset_loadscreen_png[];
extern const unsigned char asset_loadscreen_png_end[];
extern const unsigned char asset_logo_png[];
extern const unsigned char asset_logo_png_end[];
}

namespace assets {

struct Blob {
    const void* data;
    size_t size;
};

// Pantalla de carga (Recursos/Loadscreen.png reescalada a 480x272).
inline Blob loadscreenPng() {
    return {asset_loadscreen_png,
            static_cast<size_t>(asset_loadscreen_png_end - asset_loadscreen_png)};
}

// Logotipo de Recursos/fortnite_PNG25.png: 256 px de ancho, RGBA con halo transparente.
inline Blob logoPng() {
    return {asset_logo_png, static_cast<size_t>(asset_logo_png_end - asset_logo_png)};
}

}  // namespace assets
