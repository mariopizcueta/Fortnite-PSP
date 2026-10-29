#include "world/terrain.h"

#include <math.h>

#include "world/water.h"
#include "world/world_units.h"

namespace world {

float groundHeightAt(float /*x*/, float /*z*/) { return kGroundY; }

bool inWater(float x, float z) {
    // Por losas, como se dibuja: la losa que contiene (x, z) es de agua si su centro esta
    // dentro del lago (el lago queda lejos del recinto de hormigon, que manda sobre el agua).
    const float tx = floorf((x + kMapHalfSize) / kTileSize), tz = floorf((z + kMapHalfSize) / kTileSize);
    return lakeValue(-kMapHalfSize + (tx + 0.5f) * kTileSize, -kMapHalfSize + (tz + 0.5f) * kTileSize) < 1.0f;
}

}  // namespace world
