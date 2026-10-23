// Consultas sobre el terreno transitable.
#pragma once

namespace world {

// Altura del suelo en (x, z). De momento el terreno es plano (Y = 0) y las estructuras
// aun no tienen colision (ver docs/JUGADOR.md).
float groundHeightAt(float x, float z);

}  // namespace world
