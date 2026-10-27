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
    ASSET mirilla_png, "assets/images/mirilla.png"
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
    ASSET sfx_aviso_tormenta, "assets/sounds/aviso_tormenta.wav"
    ASSET sfx_cofre_abrir, "assets/sounds/cofre_abrir.wav"
    ASSET sfx_cofre_cerca, "assets/sounds/cofre_cerca.wav"
    ASSET sfx_salto_autobus, "assets/sounds/salto_autobus.wav"
    ASSET sfx_ala_delta, "assets/sounds/ala_delta.wav"
    ASSET sfx_francotirador, "assets/sounds/francotirador.wav"
    ASSET sfx_escopeta, "assets/sounds/escopeta.wav"
    ASSET sfx_subfusil, "assets/sounds/subfusil.wav"
    ASSET sfx_recoger, "assets/sounds/recoger.wav"
    ASSET sfx_baile, "assets/sounds/baile.wav"
    ASSET sfx_pistola, "assets/sounds/pistola.wav"
    ASSET sfx_cohete, "assets/sounds/cohete.wav"
    ASSET sfx_lanzagranadas, "assets/sounds/lanzagranadas.wav"
    ASSET sfx_granada_lanzar, "assets/sounds/granada_lanzar.wav"
    ASSET sfx_explosion, "assets/sounds/explosion.wav"
    ASSET sfx_caja_municion, "assets/sounds/caja_municion.wav"
    ASSET sfx_llama, "assets/sounds/llama.wav"
    ASSET sfx_trampa, "assets/sounds/trampa.wav"
    ASSET sfx_revolver, "assets/sounds/revolver.wav"
    ASSET sfx_escopeta_corredera, "assets/sounds/escopeta_corredera.wav"
    ASSET sfx_botiquin, "assets/sounds/botiquin.wav"
    ASSET sfx_vendas, "assets/sounds/vendas.wav"
    ASSET sfx_recarga_fusil, "assets/sounds/recarga_fusil.wav"
    ASSET sfx_recarga_escopeta, "assets/sounds/recarga_escopeta.wav"
    ASSET sfx_recarga_tactica, "assets/sounds/recarga_tactica.wav"
    ASSET sfx_recarga_subfusil, "assets/sounds/recarga_subfusil.wav"
    ASSET sfx_recarga_pistola, "assets/sounds/recarga_pistola.wav"
    ASSET sfx_recarga_revolver, "assets/sounds/recarga_revolver.wav"
    ASSET sfx_recarga_francotirador, "assets/sounds/recarga_francotirador.wav"
    ASSET sfx_recarga_lanzagranadas, "assets/sounds/recarga_lanzagranadas.wav"
    ASSET sfx_recarga_cohete, "assets/sounds/recarga_cohete.wav"
    ASSET music_menu, "assets/sounds/musica_menu.ima"
