#!/bin/sh
# Compila y ejecuta el simulador de partidas en el Mac (no PSP).
# Uso (desde la raiz del proyecto): sh tools/match_sim/build_and_run.sh   (o: make match-sim)
set -e
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
OUT="$ROOT/build/match_sim"
mkdir -p "$OUT"
# host_assets.S incrusta los recursos con rutas relativas a la raiz del proyecto.
cd "$ROOT"
c++ -std=c++17 -O2 -Wall -Wextra \
    -I"$ROOT/tools/scene_preview/stubs" -I"$ROOT/src" \
    "$ROOT/tools/match_sim/match_sim.cpp" \
    "$ROOT/tools/host/host_platform.cpp" \
    "$ROOT/src/game/character.cpp" \
    "$ROOT/src/game/match.cpp" \
    "$ROOT/src/game/match_air.cpp" \
    "$ROOT/src/game/match_bots.cpp" \
    "$ROOT/src/game/match_loot.cpp" \
    "$ROOT/src/game/match_projectiles.cpp" \
    "$ROOT/src/game/match_traps.cpp" \
    "$ROOT/src/game/match_zombies.cpp" \
    "$ROOT/src/game/match_allies.cpp" \
    "$ROOT/src/game/match_teams.cpp" \
    "$ROOT/src/game/zombie_save.cpp" \
    "$ROOT/src/game/zombies.cpp" \
    "$ROOT/src/game/nav_grid.cpp" \
    "$ROOT/src/game/player.cpp" \
    "$ROOT/src/game/inventory.cpp" \
    "$ROOT/src/game/structures.cpp" \
    "$ROOT/src/game/storm_zone.cpp" \
    "$ROOT/src/game/weapon.cpp" \
    "$ROOT/tools/host/host_assets.S" \
    "$ROOT/src/gfx/mesh.cpp" \
    "$ROOT/src/gfx/texture.cpp" \
    "$ROOT/src/math/math3d.cpp" \
    "$ROOT/src/core/i18n.cpp" \
    "$ROOT/src/third_party/stb_image_impl.cpp" \
    "$ROOT/src/world/collision.cpp" \
    "$ROOT/src/world/scene_builder.cpp" \
    "$ROOT/src/world/terrain.cpp" \
    "$ROOT/src/world/textures.cpp" \
    "$ROOT/src/world/world.cpp" \
    "$ROOT/src/world/world_layout.cpp" \
    -Wno-unused-function -o "$OUT/match_sim"
"$OUT/match_sim"
