#!/bin/sh
# Compila el previsualizador de host y genera capturas en build/preview/.
# Uso (desde la raiz del proyecto): sh tools/scene_preview/build_and_run.sh   (o: make preview)
set -e
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
OUT="$ROOT/build/preview"
mkdir -p "$OUT"
# host_assets.S incrusta los recursos con rutas relativas a la raiz del proyecto.
cd "$ROOT"

c++ -std=c++17 -O2 -Wall -Wextra \
    -I"$ROOT/tools/scene_preview/stubs" -I"$ROOT/tools/host" -I"$ROOT/src" \
    "$ROOT/tools/scene_preview/preview.cpp" \
    "$ROOT/src/app/build_visuals.cpp" \
    "$ROOT/src/game/nav_grid.cpp" \
    "$ROOT/src/game/structures.cpp" \
    "$ROOT/src/camera/orbit_camera.cpp" \
    "$ROOT/src/camera/third_person_camera.cpp" \
    "$ROOT/src/game/character.cpp" \
    "$ROOT/src/game/character_model.cpp" \
    "$ROOT/tools/host/host_assets.S" \
    "$ROOT/tools/host/soft_gfx.cpp" \
    "$ROOT/src/gfx/mesh.cpp" \
    "$ROOT/src/gfx/texture.cpp" \
    "$ROOT/src/math/math3d.cpp" \
    "$ROOT/src/third_party/stb_image_impl.cpp" \
    "$ROOT/src/world/collision.cpp" \
    "$ROOT/src/world/scene_builder.cpp" \
    "$ROOT/src/world/terrain.cpp" \
    "$ROOT/src/world/textures.cpp" \
    "$ROOT/src/world/world.cpp" \
    "$ROOT/src/world/world_layout.cpp" \
    -Wno-unused-function -o "$OUT/scene_preview"

"$OUT/scene_preview" "$OUT"
echo "Capturas en $OUT"
