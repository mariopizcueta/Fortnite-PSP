#!/bin/sh
# Compila y ejecuta la simulacion de la aplicacion completa en el Mac (no PSP).
# Uso (desde la raiz del proyecto): sh tools/app_sim/build_and_run.sh   (o: make app-sim)
set -e
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
OUT="$ROOT/build/app_sim"
PSPDEV="${PSPDEV:-$HOME/pspdev}"
mkdir -p "$OUT"
# host_assets.S incrusta los recursos con rutas relativas a la raiz del proyecto.
cd "$ROOT"

# Fuente 8x8 de pspDebugScreen (la misma que usa el juego), sacada de libpspdebug.a.
if [ ! -f "$OUT/msx_font.bin" ] && [ -x "$PSPDEV/bin/psp-ar" ]; then
    (cd "$OUT" && "$PSPDEV/bin/psp-ar" x "$PSPDEV/psp/sdk/lib/libpspdebug.a" font.o &&
        "$PSPDEV/bin/psp-objcopy" -O binary --only-section=.data font.o msx_font.bin && rm -f font.o)
fi

# Musica de referencia para la prueba del decodificador: la misma conversion que
# tools/prepare_assets.py (afconvert a 22050 Hz mono) antes de codificarla en IMA ADPCM.
SONG="$ROOT/Recursos/Sonidos/main title song.wav"
if [ ! -f "$OUT/music_ref.wav" ] && [ -f "$SONG" ] && command -v afconvert >/dev/null 2>&1; then
    afconvert -f WAVE -d LEI16@22050 -c 1 "$SONG" "$OUT/music_ref.wav"
fi

c++ -std=c++17 -O2 -Wall -Wextra -DFORTNITEPSP_HOST_TEST \
    -I"$ROOT/tools/scene_preview/stubs" -I"$ROOT/tools/host" -I"$ROOT/src" \
    "$ROOT/tools/app_sim/app_sim.cpp" \
    "$ROOT/tools/host/host_assets.S" \
    "$ROOT/tools/host/soft_gfx.cpp" \
    "$ROOT/src/app/app.cpp" \
    "$ROOT/src/app/app_hud.cpp" \
    "$ROOT/src/app/app_menus.cpp" \
    "$ROOT/src/app/build_visuals.cpp" \
    "$ROOT/src/app/lobby_scene.cpp" \
    "$ROOT/src/app/prop_visuals.cpp" \
    "$ROOT/src/app/profile.cpp" \
    "$ROOT/src/app/ui.cpp" \
    "$ROOT/src/audio/audio_system.cpp" \
    "$ROOT/src/audio/mixer.cpp" \
    "$ROOT/src/audio/sound_data.cpp" \
    "$ROOT/src/camera/orbit_camera.cpp" \
    "$ROOT/src/camera/third_person_camera.cpp" \
    "$ROOT/src/core/timer.cpp" \
    "$ROOT/src/game/character.cpp" \
    "$ROOT/src/game/character_model.cpp" \
    "$ROOT/src/game/item_models.cpp" \
    "$ROOT/src/game/match.cpp" \
    "$ROOT/src/game/match_air.cpp" \
    "$ROOT/src/game/match_bots.cpp" \
    "$ROOT/src/game/match_loot.cpp" \
    "$ROOT/src/game/match_projectiles.cpp" \
    "$ROOT/src/game/match_traps.cpp" \
    "$ROOT/src/game/nav_grid.cpp" \
    "$ROOT/src/game/player.cpp" \
    "$ROOT/src/game/inventory.cpp" \
    "$ROOT/src/game/structures.cpp" \
    "$ROOT/src/game/storm_zone.cpp" \
    "$ROOT/src/game/weapon.cpp" \
    "$ROOT/src/gfx/debug_text.cpp" \
    "$ROOT/src/gfx/mesh.cpp" \
    "$ROOT/src/gfx/texture.cpp" \
    "$ROOT/src/input/input.cpp" \
    "$ROOT/src/math/math3d.cpp" \
    "$ROOT/src/third_party/stb_image_impl.cpp" \
    "$ROOT/src/world/collision.cpp" \
    "$ROOT/src/world/scene_builder.cpp" \
    "$ROOT/src/world/terrain.cpp" \
    "$ROOT/src/world/textures.cpp" \
    "$ROOT/src/world/world.cpp" \
    "$ROOT/src/world/world_layout.cpp" \
    -Wno-unused-function -o "$OUT/app_sim"
"$OUT/app_sim" "$OUT"
