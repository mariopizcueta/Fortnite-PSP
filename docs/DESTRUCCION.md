# Mapa destruible, materiales de construcción e inventario de los bots (v0.10.0)

## Todo el mapa se pica y se destruye

Con el pico (TRIÁNGULO y R) se puede picar todo lo que hay en el mapa salvo el suelo, el
lago y la meseta.
- **Cada golpe:** da material y quita 50 de vida al objeto.
- **Sin vida:** el objeto desaparece. Deja de dibujarse y su colisión se desactiva (física,
  disparos, línea de visión y cámara), y los bots recalculan sus caminos.
- **Siguiente partida:** todo vuelve a estar en pie.

| Objeto | Material por golpe | Vida (golpes) |
|---|---|---|
| Árbol | 8 de madera | 300 (6) |
| Caja de madera | 6 de madera | 150 (3) |
| Roca | 7 de piedra | 300 (6) |
| Muro de bloques (en tramos de 4 m como mucho) | 6 de piedra | 250 (5) |
| Pared de nave (paneles de 4 m como mucho), tejado (4 tramos por nave), altillo | 6 de metal | 300 (6) |
| Contenedor de transporte, remolque | 8 de metal | 400 (8) |
| Escalera, estantería, contenedor de basura | 6 de metal | 200 (4) |
| Valla (tramos de 4 m como mucho), farola, barandilla, pilar, persiana, marco del portalón | 4 de metal | 150 (3) |

Son 325 objetos que reúnen 547 de las 562 cajas de colisión del mapa.

**Cómo funciona** (`world/harvest.h`, `world/world_layout.cpp`):
- **Al construir el escenario:** todo lo que se dibuja y se añade a la colisión entre
  `beginObject()` y `endObject()` forma un objeto. Sus cajas quedan seguidas y sus lotes de
  dibujo van aparte, y cada lote guarda a qué objeto pertenece.
- **Destruir:** `CollisionWorld::setStaticEnabled()` desactiva sus cajas y
  `World::draw(..., destroyed)` se salta sus lotes. La vida y lo destruido los lleva
  `game::Match` (`damageObject()`, `objectDestroyed()`, `destroyedObjects()`).
- **Coste:** las vistas del depósito pasan de ~95 a ~325 lotes por fotograma, con los mismos
  triángulos. Están ordenados por textura y no se repiten cambios de textura. En la PSP no
  está medido.

## Tejados de las naves

Antes los tejados solo se dibujaban y quien caía del autobús los atravesaba.
- **Colisión:** cada tramo de tejado tiene columnas de 1 m de ancho que siguen el perfil
  abuhardillado. Se aterriza encima (a 12,8 m en la cumbrera).
- **Bots:** no eligen puntos de aterrizaje bajo techo. Si alguno acaba encima de algo (un
  tejado, un contenedor o el altillo), baja por el lado cuyo apoyo es más bajo
  (`Match::descentDirection`).

## Cofres

Al abrirse, un cofre, una caja de munición o un airdrop se ve 0,4 s abierto y desaparece.

## Construcción: materiales

- **Elegir material:** en modo construcción, CUADRADO cambia de material: madera → piedra →
  metal. En el panel de materiales se resalta el que se va a usar.
- **Material de reserva:** si no hay 10 del elegido, la pieza se construye con el primero
  que sí haya. Por eso la franja de construcción dice, por ejemplo, "Pared de madera".
- **Vida de cada pieza:**

  | Material | Vida |
  |----------|------|
  | Madera   | 150  |
  | Piedra   | 300  |
  | Metal    | 500  |

- **Quitar piezas:** ya no se hace con CUADRADO (ahora cambia el material); se rompen con el
  pico.

## Inventario de los bots

Los bots llevan un inventario de 5 ranuras como el del jugador, con su munición infinita.
- **Armas:** varias. Cambian una por otra mejor del mismo tipo y, con el inventario lleno,
  sueltan lo peor.
- **Arma en la mano:** sacan la que mejor va a la distancia del rival: escopeta de cerca,
  francotirador de lejos y, si no, la mejor. Cambiar de arma les lleva 0,3 s.
- **Consumibles:** vendas, botiquín y pociones. Los usan cuando no hay nadie a la vista,
  quietos y con el mismo tiempo de uso que el jugador. Si ven a alguien, lo dejan.
- **Al caer:** todo su inventario queda en el suelo, cada arma con su munición.

Con esto las partidas simuladas de 10 y 30 bots duran unos 90-140 s (antes, 60-85).

## Minimapa

Sobre las tres naves pone **Polvorín Polvoriento**, recortado al borde del minimapa. Es el
elemento `MapaMinimapa` del atlas, generado por `tools/prepare_assets.py`.
