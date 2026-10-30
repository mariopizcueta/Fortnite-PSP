# Actualización v0.15.0: Supervivencia zombi a fondo

Todo lo nuevo es del modo **Supervivencia zombi** (el battle royale y Monodisparo no cambian,
salvo un arreglo de los bots, al final). Reglas completas del modo en [ZOMBIS.md](ZOMBIS.md).

| # | Petición | Qué se ha hecho |
|---|---|---|
| 1 | Más estructuras y lugares de interés | 10 lugares en ruinas: **La Casona**, el **Almacén del Polvorín**, 4 casas (del guarda, del lago, la granja y la cabaña del bosque), 3 **puestos de avanzada** y la **gasolinera**. Paredes con el borde roto, boquetes, techos hundidos, vigas caídas, escombros y sangre. |
| 2 | Barra de hambre; comida en neveras, almacenes y cofres; los sitios grandes, más botín y más zombis | **HAMBRE** (se vacía en ~1,5 días; a cero se pierde vida). **Latas** (+30) y **raciones** (+65 y un poco de vida). Comida en las **neveras** de las casas, en las **despensas** de los almacenes y puestos, a veces en los cofres y en algún zombi. Cada lugar tiene **guardianes** que salen al acercarse (una vez al día): la casa, 2-3; la Casona, 6; el Almacén, 7 (desde el día 2 con un **bruto**). La Casona y el Almacén tienen más contenedores y más botín por el suelo. |
| 3 | Guardar partida | **Pausa > GUARDAR PARTIDA** y **GUARDAR Y SALIR**; además se guarda sola **cada amanecer**. En el lobby (panel de modo) se elige **Continuar: día N** o **Partida nueva**. Si caes (o abandonas), la partida guardada se borra. Fichero `partida_zombis.txt` junto al EBOOT. |
| 4 | Cama en los centros de crafteo; aliados que duermen fuera de guardia; necesidad de dormir | Receta **Cama** (50 madera, 15 metal). **R** con la cama en la mano la coloca delante; **CUADRADO** junto a ella, a dormir. **SUEÑO**: se vacía en ~3 días; por debajo de un tercio te **equivocas** (tiembla el pulso, tropiezas, se te escapa un tiro, se te caen balas, microsueños) y a cero pierdes vida. **No se puede dormir con monstruos cerca** (18 m) y uno que se acerque te despierta. De noche, con el jugador en la base, los **aliados con cama duermen en ella**: fuera de guardia, los zombis no van a por ellos. |
| 5 | Saco de dormir inicial: la cama llena el descanso en media noche, el saco en una noche | Se empieza con el **saco de dormir** (R: dormir donde estés). Durmiendo, el tiempo pasa **×8**; la cama llena el descanso en **media noche** de reloj y el saco en **una noche entera**. |
| 6 | Cajas de munición con todo tipo de munición | En este modo cada caja da **los 5 tipos** (medio paquete de cada uno). |
| 7 | CUADRADO sobre un aliado: gestionar su inventario | Pantalla con tus ranuras arriba y las suyas abajo: **X** le da un arma o un curativo, o se lo quita (con un cargador de su munición). |
| 8 | Un aliado caído se convierte en zombi armado | A los 4 s se **levanta** con sus armas (y su aspecto), dispara al jugador y a los aliados. |
| 9 | Los aliados construyen y mejoran la base; sin planos, un muro pequeño | Con el jugador en la base y de día, levantan piezas de los planos encontrados con tu material y, aunque no tengas ningún plano, **el muro del perímetro** (con lo que rapiñan, el triple de lento). También **reconstruyen** las piezas que rompen los zombis. |
| 10 | Sin límite de estructuras | Sin límite propio (solo el técnico de **128 piezas** en pie a la vez). |
| 11 | Los días duran más | Día **300 s** (antes 150) y noche **180 s** (antes 95). |
| 12 | Mochila al eliminar un zombi armado o un bruto | Sueltan una **mochila**: **+3 ranuras** (como mucho dos: 11). El HUD y el inventario se ajustan. |
| 13 | Más objetivos | Siempre **dos objetivos** variados (abatir zombis o corredores, un bruto, registrar neveras, comer, fabricar, construir, ir a un lugar, picar material, dormir bien, tiros a la cabeza). Al cumplir uno: **recompensa** (materiales, munición, comida o curas) y **10 pavos**; llega otro. |
| 14 | Que dé miedo | Más oscuro; **ojos rojos** que brillan en la oscuridad (se ven antes que el zombi); **latido** y bordes rojos con zombis cerca o con poca vida; **gritos lejanos** de noche; **zombis tumbados** entre las ruinas como cadáveres que se levantan al acercarte (con golpe de sonido); con sueño, **susurros**, **figuras** que aparecen y desaparecen y **microsueños**; los aliados caídos vuelven. |

## Controles nuevos (modo zombi)

| Dónde | Botón | Qué hace |
|---|---|---|
| Comida en la mano | **R** | Comer |
| Cama en la mano | **R** | Colocarla delante |
| Saco de dormir en la mano | **R** | Dormir aquí |
| Junto a una cama | **CUADRADO** | Dormir |
| Durmiendo | **CÍRCULO** o **X** | Despertar |
| Junto a una nevera / despensa | **CUADRADO** | Registrarla |
| Junto a un aliado | **CUADRADO** | Su inventario (←→ elegir, ↑↓ cambiar de fila, X dar / quitar, O cerrar) |
| Pausa | **GUARDAR PARTIDA / GUARDAR Y SALIR** | Guardar |
| Lobby, panel de modo (CUADRADO), fila de abajo | **← →** | Continuar la partida guardada o empezar otra |

## Arreglo en el battle royale

- Un bot que iba a por un objeto **al otro lado de un remolque o una pared** sin camino se
  quedaba empujando hasta 25 s. Ahora, si no hay camino ni paso recto, lo deja y no lo vuelve
  a elegir.

## Código

- `world/zombie_sites.h`: los lugares (`kSites`) y sus contenedores (`kSiteContainers`).
- `world/world_layout.cpp`: las ruinas (`ruinWall`, `ruinedHouse`, `bigHouse`, `storehouse`,
  `outpost`, `gasStation`).
- `game/match_zombies.cpp`: hambre y sueño, camas, guardianes y zombis tumbados, aliados
  (inventario, camas, obras, caídos), objetivos y reabastecimiento al amanecer.
- `game/zombie_save.cpp`: la partida guardada.
- `game/inventory.*`: comida, cama, saco y mochilas (`Inventory::slotCount()`).
- `app/app_zombies.cpp`: avisos, HUD (objetivos, hambre y sueño), miedo (latido, gritos,
  susurros, alucinaciones, ojos), guardado; `app/app_hud.cpp`: el inventario del aliado.
- Sonidos sintetizados nuevos (`tools/generate_sounds.py`): latido, grito, susurro,
  sobresalto, comer, mochila y respirar dormido. Iconos de la lata, la ración, la cama, el saco
  y la mochila dibujados en `tools/prepare_assets.py`.

## Pruebas

- `make match-sim`: **0 fallos en 257** comprobaciones (sección 21 nueva: contenedores de los
  lugares, hambre, sueño, saco y cama, mochilas, inventario y caída de un aliado, muro de los
  aliados, camas de los aliados, sin límite de piezas, objetivos, guardianes y guardar/cargar).
- `make app-sim`: **0 fallos en 189** (sección 6c nueva: lugares, nevera, comer, dormir y
  despertar, inventario del aliado, mochila, noche, guardar, guardar y salir, continuar desde
  el lobby y abandonar).
- `make physics-test`: 0 fallos. `make preview`: sin errores.
- En las pruebas de 30 bots, la tolerancia para "dentro de un sólido" pasa de 1 a 2 cm: al
  subir por su propia rampa un bot puede rozar un escalón 1 cm durante un paso.

**Sin probar en ejecución real** (no hay emulador en el Mac): falta comprobar en la PSP-2000
los FPS con los lugares nuevos y los guardianes, que la oscuridad no sea excesiva, que se
lean los textos del HUD y que el guardado funcione en la Memory Stick.
