# Mundo 3D: coordenadas, escala, ejes, mapa y colisiones

Referencia para todo el código que coloque cosas en el escenario. Las constantes viven
en `src/world/world_units.h` y `src/core/config.h`.

## Escala

**1 unidad = 1 metro.** Referencias de tamaño:

| Elemento                    | Tamaño                                              |
|-----------------------------|-----------------------------------------------------|
| Personaje                   | 1,9 m (caja de colisión 0,7 × 1,8 × 0,7 m)          |
| Escalón que se sube andando | hasta 0,45 m                                        |
| Salto                       | ~1,27 m (se sube a una caja de 1,2 m)               |
| Caja de madera              | 1,2 × 1,2 × 1,2 m                                   |
| Contenedor marítimo         | 6,1 × 2,6 × 2,5 m                                   |
| Nave                        | 16 × 30 m, paredes de 8 m, cumbrera a 12,8 m        |
| Portalón / puerta           | 8 × 6,5 m / 4 × 2,8 m                               |
| Altillo                     | 3,6 m de alto, 6 m de fondo, escalera de 10 peldaños |
| Muro del recinto            | 2,4 m (no se salta)                                 |
| Meseta de roca              | Ø ~44 m en la base, 26 m de alto                    |

## Ejes

Sistema **dextrógiro con +Y arriba** (el mismo que OpenGL y `libpspgum`):

```
          -Z (norte)
             ^
             |
 -X (oeste) -+-> +X (este)        +Y sale de la pantalla (arriba)
             |
             v
          +Z (sur)
```

- **Origen** `(0, 0, 0)`: centro del mapa, a nivel del suelo.
- **Suelo**: plano `Y = 0`.
- **Límites**: `X, Z ∈ [-96, 96]` (192 × 192 m); los personajes no pasan de ±95,5.
- En espacio de vista la cámara mira hacia **-Z** (convención OpenGL).
- **Yaw** (jugador, bots, cámaras): se mira hacia `(-sin yaw, 0, -cos yaw)`; 0 = norte.

## Convenciones de matrices

- `math3d::Mat4` es **column-major**: elemento (fila r, columna c) en `m[c*4 + r]`,
  traslación en `m[12..14]`. Se multiplica como `v' = M · v`. Es el mismo layout que
  `ScePspFMatrix4`, así que se sube directamente con `sceGuSetMatrix`.
- `GU_PROJECTION`: `Mat4::perspective(60°, 480/272, 0,5 m, 140 m)`.
- `GU_VIEW`: `Mat4::lookAt(ojo, ojo + dirección de la mira, +Y)`.
- `GU_MODEL`: identidad para el escenario (ya en coordenadas de mundo). Personajes:
  `traslación · rotY(yaw) [· articulación]` sobre mallas locales que no cambian.
- **Profundidad**: `sceGuDepthRange(65535, 0)` (cerca = 65535), test `GU_GEQUAL`,
  se limpia a 0 en cada frame junto con el color. Buffer de 16 bits.
- **Plano cercano 0,5 m:** se bajó de 1 m en la fase 4 porque la cámara se acerca a las
  paredes dentro de las naves. La precisión de profundidad a 60 m es de ~0,11 m, así que
  los adornos pegados a paredes van separados al menos 5 cm.
- **Altura mínima del ojo:** con un FOV vertical de 60°, el suelo de la esquina inferior
  queda a una profundidad de al menos `altura / 1,155`. Por eso el ojo no baja de
  `1,155 × plano cercano`: la cámara en tercera persona usa 0,7 m y la libre, 1,3 m.

## Niebla y distancia de dibujado

- Lineal de **30 m** a **95 m** (espacio de vista). Color de niebla = color de limpieza
  = cielo `#8CC2D8`, así el horizonte no tiene costura.
- Los lotes enteramente más allá de 95 m no se envían a la GE (con la niebla
  desactivada, el límite es el plano lejano, a 140 m).

## Plano del mapa: depósito "Polvorín"

Se ciñe a `Referencias/Referencias Primer Mapa "Polvorin Polvoriento"`: tres naves
contiguas azul, gris y roja, con cubierta abuhardillada y lucernarios, portalones medio
subidos y un muro de bloques delante. Alrededor hay valla metálica, un camino de tierra y
una meseta de roca con pinos y depósito de agua. Para que los alrededores no sean
monótonos se añaden un bosque al oeste y un lago al sur.

```
                 bosque (x < -44)                    meseta (58, -60)
  z=-32  +-------- valla (puerta x -5..5) ---------+
         | [azul]       [gris]       [roja]        |
         | x -30..-14   x -8..8      x 14..30      |
 puerta  | z -26..4  (altillo al fondo de cada una)  | puerta este -> camino
 oeste   |                                         |
  z=14   +-muro-[puerta -25..-19]-valla-[19..25]-muro-+
                                       \ camino sur
           lago (-30, 56)
```

| Zona                       | X            | Z            | Notas                                    |
|----------------------------|--------------|--------------|------------------------------------------|
| Patio de hormigón          | -38 … 38     | -34 … 16     |                                          |
| Nave azul / gris / roja    | -30…-14 / -8…8 / 14…30 | -26 … 4 | Portalón al sur; puertas traseras en x = -22, -2 y 22 |
| Pasillos entre naves       | -14…-8 y 8…14 | -26 … 4     | Puertas laterales en z = -12 … -8        |
| Altillos                   | toda la nave | -26 … -20    | 3,6 m, escalera junto a la pared oeste   |
| Muro de bloques            | -36,4 … 36,4 | 14 … 14,4    | Puertas x -25…-19 y 19…25; valla en -6…6 |
| Muros laterales            | ±36          | -32 … 14     | Puerta en z -9 … -3                      |
| Valla norte                | -36,4 … 36,4 | -32          | Puerta en x -5 … 5                       |
| Meseta de roca             | centro (58, -60) | radio ~22 en la base | No escalable                |
| Lago                       | centro (-30, 56) | radios 20 × 12 | Orilla de arena, no se puede cruzar  |
| Bosque                     | -92 … -44    | -90 … 36     | Pinos y árboles de copa cúbica           |
| Camino sur / este          | `x = 22 + (z-16)·0,22` / z ≈ -6 | |                                     |

**Interiores como refugio:**
- **Cobertura:** cada nave tiene estanterías, cajas y, en la gris, un contenedor bajo el
  altillo.
- **Altillo:** da altura y está protegido por barandilla y cajas.
- **Luz:** el suelo interior es más oscuro, con la sombra horneada.
- **Pasillos:** las puertas laterales y traseras permiten flanquear.

## Colisiones: geometría visual y geometría de colisión separadas

- **Dibujo:** usa solo `StaticBatch` (las mallas que genera `SceneBuilder`).
- **Física, IA y disparos:** usan solo `world::CollisionWorld`, una lista de cajas AABB
  con una rejilla espacial de 8 m.
- **Registro:** `world_layout.cpp` registra cada pieza en los dos sitios de forma
  explícita (`solid()`, `wallAlongX()`…), y algunas piezas solo en uno de ellos.

Cada caja indica qué bloquea:

| Flag              | Bloquea                     | Ejemplos                                              |
|-------------------|-----------------------------|-------------------------------------------------------|
| `kSolid`          | movimiento y disparos       | paredes, muros, cajas, contenedores, troncos, rocas, meseta |
| `kBlocksMovement` | solo movimiento             | valla metálica, barandilla del altillo, agua del lago |
| (sin colisión)    | nada                        | tejados, copas de árbol, lucernarios, depósito de agua |

Los tejados no tienen colisión: nadie llega a ellos y todos los disparos son a ras de suelo
o desde el altillo.

**Colisión de personajes** (`game/character.cpp`):
- **Cuerpo:** caja de 0,7 × 1,8 m. Se mueve eje a eje (X y luego Z); la caja más
  restrictiva fija la posición final y anula la velocidad en ese eje.
- **Escalones:** la parte baja del cuerpo, hasta 0,45 m, no choca en horizontal; se sube
  sola en la fase vertical (escaleras, bordillos).
- **Vertical:** se apoya en el techo más alto bajo la huella. Al caer se buscan las
  plataformas que estaban por debajo de los pies antes del paso, así que no hay tunelado
  ni a 40 m/s. Los techos detienen el salto.
- **Entre personajes:** se separan con un empujón horizontal, y después se vuelven a
  sacar de cualquier sólido.

## Suelo, geometría y texturas

- **Losas de suelo:** de 2 m, de 4 m en zonas solo de hierba. Se agrupan en bloques de
  16 × 16 m que se descartan enteros. Las UV salen de las coordenadas de mundo.
- **Tipos de suelo:** hierba, hormigón (más oscuro dentro de las naves), tierra, agua y
  arena.
- **Teselado:** las caras grandes se subdividen en trozos de 3 m como máximo
  (`kMaxSegment`), por la banda de guarda de ±2048 px de la GE.
- **Iluminación:** horneada en el color de vértice. El sol viene de arriba-sureste; las
  caras interiores de las paredes de las naves van oscurecidas.
- **Texturas (fase 6):** 16 texturas originales de 32 × 32 en `Recursos/Texturas/`
  (generadas por `tools/generate_textures.py` con paleta reducida de 6 a 12 colores):
  hierba, tierra, hormigón, chapa, cubierta, contenedor, madera, damero, persiana,
  oficina, camuflaje, agua, roca, arena, muro de bloques y tablones de pared.
  `make assets` las junta en un atlas de 128 × 128 (`assets/images/textures.png`) que el
  juego trocea al arrancar en texturas 5551 con `GU_NEAREST` (32 KB en total). Si el
  atlas faltara, se usan las procedurales de fases anteriores.
- **Totales:** 23 240 triángulos en 228 lotes (~1 MB) y 276 cajas de colisión.

## Cámaras

**Tercera persona** (`camera/third_person_camera.*`, detalles en JUGADOR.md):
- **Posición:** el pivote está a 1,7 m sobre los pies; el ojo, a 5 m del pivote y unos
  1,2 m por encima.
- **Mira:** casi horizontal (unos -4°).
- **Paredes:** si hay una entre el pivote y el ojo, el ojo se acerca al jugador.

**Cámara libre de depuración** (TRIÁNGULO): orbital, como en la fase 2.
