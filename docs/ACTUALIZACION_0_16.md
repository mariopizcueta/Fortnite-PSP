# Actualización v0.16.0: dúo y escuadrón, aliados inteligentes y camas asignadas

| # | Petición | Qué se ha hecho |
|---|---|---|
| 1 | No se oye la escopeta de corredera al acertar | **Causa:** cada perdigón (10) hacía sonar un impacto y entre todos ocupaban las 8 voces del mezclador, así que el disparo se cortaba. **Arreglo:** los impactos del mismo frame son un solo sonido, más bajo durante 0,25 s tras tu disparo, y el mezclador tiene prioridades: **ningún efecto puede quitarle la voz a tu disparo** (`audio::SfxPriority`). Vale para todas las armas. |
| 2 | Vestigios de los edificios zombi en el mapa del Battle Royale | Suelos de baldosas, vigas caídas, sangre, franjas de peligro y chapas colgando no eran "objetos" del mapa, así que se dibujaban siempre. Ahora todo lo de los lugares zombi va marcado (`StaticBatch::zombieOnly`) y solo se dibuja en ese modo. |
| 3 | Dúo y escuadrón, derribados y reanimar | Submodos en el lobby. Dúo: 15 parejas. Escuadrón: 40 personajes, 10 equipos de 4. A cuatro patas, reanimar con CUADRADO mantenido y muerte si nadie llega. Todo en [EQUIPOS.md](EQUIPOS.md). |
| 4 | Barras de los aliados arriba a la izquierda | Nombre, escudo y vida en pequeño, en el modo zombi y en los equipos. Además, marca sobre su cabeza. |
| 5 | Aliados inteligentes | **125 de vida y hasta 125 de escudo.** Recogen armas (cambian la suya si es mejor), curas, materiales y mochilas; pican si van cortos de material; fabrican lo que les falta con sus materiales junto a un centro de crafteo. Con poca vida huyen hacia ti y se cubren con lo que construyen. **Lo primero es seguirte:** a más de 15 m lo dejan todo, disparando de camino. |
| 6 | Camas asignadas | **L junto a una cama** abre la lista de aliados. De noche, quien tenga cama asignada **se acuesta pase lo que pase** (sale con tiempo para llegar) y los zombis no pueden hacerle daño. Las camas ya no se reparten solas, y tú no puedes dormir en la de un aliado. De día, el aliado **levanta un refugio** junto a su cama: tres paredes y techo, con el lado por el que colocaste la cama libre para entrar. |
| 7 | Te siguen por las rampas | Si subes por una rampa construida, suben detrás (también por varias seguidas) y bajan cuando bajas. |

## Controles nuevos

| Dónde | Botón | Qué hace |
|---|---|---|
| Lobby, panel de modo (CUADRADO), fila EQUIPOS | **← →** | Solo / Dúo / Escuadrón |
| Junto a un compañero derribado | **CUADRADO mantenido** | Reanimarle (10 s) |
| Junto a una cama, con aliados (modo zombi) | **L** | Asignarla: ↑↓ elegir, X asignar, O cerrar |

## Decisiones

- **39 bots en escuadrón, no 40:** así los 10 equipos quedan completos. Son 40 personajes en
  partida.
- **Dúo con 29 bots:** 30 personajes, los mismos que una partida en solitario. No se pedía un
  número concreto.
- **Solo el battle royale tiene equipos.** Monodisparo sigue en solitario.
- **Camino de su cama, un aliado no pelea y los zombis sí pueden golpearle:** solo es intocable
  una vez acostado, como se pidió. Si tras 30 s de noche no ha llegado, se acuesta directamente.
- **L junto a una cama, si tienes aliados, abre la lista en vez de apuntar.**
- **Las partidas guardadas de la v0.15 siguen valiendo.** Las camas que ya tenían dueño lo
  conservan.

## Código

- `audio/mixer.*`: prioridades de voz (`SfxPriority`).
- `game/match_teams.cpp` (nuevo): equipos, derribar y reanimar.
- `game/match_allies.cpp` (nuevo): toda la IA de los aliados, antes en `match_zombies.cpp`.
- `world/scene_builder.h`, `world/world.cpp`, `world/world_layout.cpp`: `zombieOnly` por lote.
- `app/app_hud.cpp`: barras y marcas; `app/app_zombies.cpp`: menú de la cama y avisos;
  `app/app_menus.cpp`: fila de equipos y resultados por equipos.
- `game/zombie_save.cpp`: línea nueva `aliado` (escudo, mochilas y materiales).

## Pruebas

- `make match-sim`: **0 fallos en 296** comprobaciones (sección 22 nueva).
- `make app-sim`: **0 fallos en 211** (sección 12 nueva: sonido con el mezclador real, lobby,
  partida de escuadrones, reanimar con el botón, derrota por equipos, capturas del mapa,
  barras, menú de la cama y aliados subiendo por una rampa).
- `make physics-test`: 0 fallos. `make preview`: sin errores.

**Sin probar en ejecución real** (no hay emulador en el Mac). Hay que comprobar en la PSP-2000:

- Los **FPS con 40 personajes** en escuadrón: es la primera vez que se pasa de 31.
- Cómo suena la escopeta por los altavoces.
- Si **30 s** para desangrarse y **10 s** para reanimar se sienten bien. En las partidas
  simuladas los bots reaniman poco (3 de 31 derribos en escuadrón) porque casi todos caen
  peleando juntos al aterrizar.
