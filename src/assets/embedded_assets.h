// Acceso a los recursos incrustados mediante embedded_assets.S (y, en las herramientas
// de host, tools/host/host_assets.cpp).
#pragma once

#include <stddef.h>

#define FORTNITEPSP_ASSET(name)                    \
    extern "C" const unsigned char asset_##name[]; \
    extern "C" const unsigned char asset_##name##_end[];

FORTNITEPSP_ASSET(loadscreen_png)
FORTNITEPSP_ASSET(login_png)
FORTNITEPSP_ASSET(mirilla_png)
FORTNITEPSP_ASSET(ui_atlas_png)
FORTNITEPSP_ASSET(textures_png)
FORTNITEPSP_ASSET(sfx_disparo)
FORTNITEPSP_ASSET(sfx_impacto)
FORTNITEPSP_ASSET(sfx_herido)
FORTNITEPSP_ASSET(sfx_eliminacion)
FORTNITEPSP_ASSET(sfx_pared)
FORTNITEPSP_ASSET(sfx_pared_rota)
FORTNITEPSP_ASSET(sfx_recarga)
FORTNITEPSP_ASSET(sfx_menu_mover)
FORTNITEPSP_ASSET(sfx_menu_aceptar)
FORTNITEPSP_ASSET(sfx_victoria)
FORTNITEPSP_ASSET(sfx_derrota)
FORTNITEPSP_ASSET(sfx_pico)
FORTNITEPSP_ASSET(sfx_curacion)
FORTNITEPSP_ASSET(sfx_muerte_jugador)
FORTNITEPSP_ASSET(sfx_miniescudo)
FORTNITEPSP_ASSET(sfx_pocion_escudo)
FORTNITEPSP_ASSET(sfx_paso)
FORTNITEPSP_ASSET(sfx_aviso_tormenta)
FORTNITEPSP_ASSET(sfx_cofre_abrir)
FORTNITEPSP_ASSET(sfx_cofre_cerca)
FORTNITEPSP_ASSET(sfx_salto_autobus)
FORTNITEPSP_ASSET(sfx_ala_delta)
FORTNITEPSP_ASSET(sfx_francotirador)
FORTNITEPSP_ASSET(sfx_escopeta)
FORTNITEPSP_ASSET(sfx_subfusil)
FORTNITEPSP_ASSET(sfx_recoger)
FORTNITEPSP_ASSET(sfx_baile)
FORTNITEPSP_ASSET(music_menu)

namespace assets {

struct Blob {
    const void* data;
    size_t size;
};

#define FORTNITEPSP_BLOB(fn, name) \
    inline Blob fn() { return {asset_##name, static_cast<size_t>(asset_##name##_end - asset_##name)}; }

// Pantalla de carga de partida (Recursos/Loadscreen - match.png, 480x272).
FORTNITEPSP_BLOB(loadscreenPng, loadscreen_png)
// Inicio de sesion (Recursos/loginscreen.png, 480x272).
FORTNITEPSP_BLOB(loginPng, login_png)
// Mirilla del francotirador (Recursos/mirilla.png, 240x136; se dibuja a 2x).
FORTNITEPSP_BLOB(scopePng, mirilla_png)
// Atlas de interfaz: iconos, rotulos y banners (rectangulos en assets/ui_atlas.h).
FORTNITEPSP_BLOB(uiAtlasPng, ui_atlas_png)
// Atlas 128x128 con las 16 texturas de Recursos/Texturas (orden de world::TextureId).
FORTNITEPSP_BLOB(texturesPng, textures_png)
// Efectos (WAV PCM 16 bits mono 22050 Hz) y musica del menu (IMA ADPCM).
FORTNITEPSP_BLOB(sfxDisparo, sfx_disparo)
FORTNITEPSP_BLOB(sfxImpacto, sfx_impacto)
FORTNITEPSP_BLOB(sfxHerido, sfx_herido)
FORTNITEPSP_BLOB(sfxEliminacion, sfx_eliminacion)
FORTNITEPSP_BLOB(sfxPared, sfx_pared)
FORTNITEPSP_BLOB(sfxParedRota, sfx_pared_rota)
FORTNITEPSP_BLOB(sfxRecarga, sfx_recarga)
FORTNITEPSP_BLOB(sfxMenuMover, sfx_menu_mover)
FORTNITEPSP_BLOB(sfxMenuAceptar, sfx_menu_aceptar)
FORTNITEPSP_BLOB(sfxVictoria, sfx_victoria)
FORTNITEPSP_BLOB(sfxDerrota, sfx_derrota)
FORTNITEPSP_BLOB(sfxPico, sfx_pico)
FORTNITEPSP_BLOB(sfxCuracion, sfx_curacion)
// Sonidos de Recursos/Sonidos/Sonidos Limpios (convertidos por tools/prepare_assets.py).
FORTNITEPSP_BLOB(sfxMuerteJugador, sfx_muerte_jugador)
FORTNITEPSP_BLOB(sfxMiniescudo, sfx_miniescudo)
FORTNITEPSP_BLOB(sfxPocionEscudo, sfx_pocion_escudo)
FORTNITEPSP_BLOB(sfxPaso, sfx_paso)
FORTNITEPSP_BLOB(sfxAvisoTormenta, sfx_aviso_tormenta)
FORTNITEPSP_BLOB(sfxCofreAbrir, sfx_cofre_abrir)
FORTNITEPSP_BLOB(sfxCofreCerca, sfx_cofre_cerca)
FORTNITEPSP_BLOB(sfxSaltoAutobus, sfx_salto_autobus)
FORTNITEPSP_BLOB(sfxAlaDelta, sfx_ala_delta)
FORTNITEPSP_BLOB(sfxFrancotirador, sfx_francotirador)
FORTNITEPSP_BLOB(sfxEscopeta, sfx_escopeta)
FORTNITEPSP_BLOB(sfxSubfusil, sfx_subfusil)
FORTNITEPSP_BLOB(sfxRecoger, sfx_recoger)
FORTNITEPSP_BLOB(sfxBaile, sfx_baile)
FORTNITEPSP_BLOB(musicMenu, music_menu)

#undef FORTNITEPSP_BLOB

}  // namespace assets
