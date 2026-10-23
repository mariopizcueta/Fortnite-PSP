// Escala, ejes y limites del mundo. Referencia completa en docs/MUNDO.md.
//
//   1 unidad = 1 metro.
//   Sistema dextrogiro con +Y hacia arriba:
//     +X = este, -X = oeste
//     +Y = arriba (el suelo esta en Y = 0)
//     +Z = sur,  -Z = norte
//   El origen (0, 0, 0) es el centro del mapa a nivel del suelo.
//
//          -Z (norte)
//             ^
//   -X <------+------> +X (este)
//             v
//          +Z (sur)            (vista cenital, Y sale de la pantalla)
#pragma once

namespace world {

constexpr float kMapHalfSize = 96.0f;  // el mapa cubre X, Z en [-96, 96] (192 x 192 m)
constexpr float kGroundY = 0.0f;

constexpr float kTileSize = 2.0f;    // celda de suelo (una cara plana con su propio tono)
constexpr float kChunkSize = 16.0f;  // bloque de suelo que se descarta o dibuja entero
constexpr int kTilesPerChunk = static_cast<int>(kChunkSize / kTileSize);
constexpr int kChunksPerSide = static_cast<int>(2.0f * kMapHalfSize / kChunkSize);

// Lado maximo de un triangulo en las estructuras. Las caras grandes se subdividen
// para que la GE no descarte ni deforme triangulos enormes junto a la camara.
constexpr float kMaxSegment = 3.0f;

// Altura de referencia de un personaje (para fases futuras y para dar escala).
constexpr float kPlayerHeight = 1.8f;

}  // namespace world
