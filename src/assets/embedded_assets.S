/* Recursos incrustados en el ejecutable (generados por tools/prepare_assets.py).
 * Las rutas son relativas a la raiz del proyecto, desde donde se invoca make. */

    .section .rodata

.macro ASSET name, path
    .balign 16
    .global asset_\name
asset_\name:
    .incbin "\path"
    .global asset_\name\()_end
asset_\name\()_end:
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
    ASSET music_menu, "assets/sounds/musica_menu.ima"
