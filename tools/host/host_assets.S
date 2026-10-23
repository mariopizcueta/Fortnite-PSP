/* Recursos incrustados para las herramientas de host (Mac, Mach-O): los mismos que
 * src/assets/embedded_assets.S. Los scripts compilan desde la raiz del proyecto, asi
 * que las rutas son relativas a ella. */

    .section __TEXT,__const

.macro ASSET name, path
    .p2align 4
    .globl _asset_\name
_asset_\name:
    .incbin "\path"
    .globl _asset_\name\()_end
_asset_\name\()_end:
.endm

    ASSET loadscreen_png, "assets/images/loadscreen.png"
    ASSET login_png, "assets/images/login.png"
    ASSET ui_atlas_png, "assets/images/ui_atlas.png"
    ASSET textures_png, "assets/images/textures.png"
    ASSET sfx_disparo, "assets/sounds/disparo.wav"
    ASSET sfx_impacto, "assets/sounds/impacto.wav"
    ASSET sfx_herido, "assets/sounds/herido.wav"
    ASSET sfx_eliminacion, "assets/sounds/eliminacion.wav"
    ASSET sfx_pared, "assets/sounds/pared.wav"
    ASSET sfx_pared_rota, "assets/sounds/pared_rota.wav"
    ASSET sfx_recarga, "assets/sounds/recarga.wav"
    ASSET sfx_menu_mover, "assets/sounds/menu_mover.wav"
    ASSET sfx_menu_aceptar, "assets/sounds/menu_aceptar.wav"
    ASSET sfx_victoria, "assets/sounds/victoria.wav"
    ASSET sfx_derrota, "assets/sounds/derrota.wav"
    ASSET sfx_pico, "assets/sounds/pico.wav"
    ASSET sfx_curacion, "assets/sounds/curacion.wav"
    ASSET sfx_muerte_jugador, "assets/sounds/muerte_jugador.wav"
    ASSET sfx_miniescudo, "assets/sounds/miniescudo.wav"
    ASSET sfx_pocion_escudo, "assets/sounds/pocion_escudo.wav"
    ASSET sfx_paso, "assets/sounds/paso.wav"
    ASSET music_menu, "assets/sounds/musica_menu.ima"
