/* Recursos incrustados en el ejecutable (generados por tools/prepare_assets.py).
 * Las rutas son relativas a la raiz del proyecto, desde donde se invoca make. */

    .section .rodata
    .balign 16

    .global asset_loadscreen_png
asset_loadscreen_png:
    .incbin "assets/images/loadscreen.png"
    .global asset_loadscreen_png_end
asset_loadscreen_png_end:

    .balign 16
    .global asset_logo_png
asset_logo_png:
    .incbin "assets/images/logo.png"
    .global asset_logo_png_end
asset_logo_png_end:
