#!/bin/sh
# Compila y ejecuta las pruebas de movimiento en el Mac (no PSP).
# Uso (desde la raiz del proyecto): sh tools/physics_test/build_and_run.sh   (o: make physics-test)
set -e
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
OUT="$ROOT/build/physics_test"
mkdir -p "$OUT"
c++ -std=c++17 -O2 -Wall -Wextra \
    -I"$ROOT/tools/scene_preview/stubs" -I"$ROOT/src" \
    "$ROOT/tools/physics_test/physics_test.cpp" \
    "$ROOT/src/camera/third_person_camera.cpp" \
    "$ROOT/src/game/character.cpp" \
    "$ROOT/src/game/player.cpp" \
    "$ROOT/src/world/collision.cpp" \
    "$ROOT/src/math/math3d.cpp" \
    "$ROOT/src/core/i18n.cpp" \
    "$ROOT/src/world/terrain.cpp" \
    -o "$OUT/physics_test"
"$OUT/physics_test"
