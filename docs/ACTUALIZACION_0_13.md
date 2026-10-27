# Actualización v0.13.0

Once cambios pedidos, con lo que se ha hecho, cómo se ha comprobado (pruebas de host, no en
la PSP) y lo que queda por mirar en la consola.

## 1. Las armas rompen el mapa

- **Antes:** las balas solo dañaban las piezas construidas; los objetos del mapa (paneles
  de las naves, contenedores, cajas, vallas, árboles, rocas…) solo se rompían con el pico y
  con explosiones.
- **Ahora:** cada bala que da en un objeto del mapa le quita su daño
  (`MatchConfig::shotObjectDamageScale`, 1.0) y, sin vida, el objeto desaparece igual que
  al picarlo (colisión, dibujo y navegación). El jugador ve el número de daño de estructura.
  Las balas no dan material (eso sigue siendo cosa del pico).
- Los bots también rompen lo que les tapa: una caja de 200 de vida cae con 6 disparos de
  SCAR. En las partidas simuladas, 100-185 impactos de bala en objetos por partida.
- Con las reglas clásicas de las pruebas antiguas (`MatchConfig::classic()`) sigue apagado.

## 2. Inventario con L + R "en seco"

- **Abrir/cerrar:** L y R pulsados a la vez (como mucho 4 frames, 0.07 s, entre uno y otro).
  Con L mantenida y R después (o al revés) no se abre: esa pulsación sigue siendo
  apuntar/disparar. Con START pulsado tampoco (es la salida L+R+START).
- **Pantalla:** las 5 ranuras en grande, lo elegido (nombre, rareza, cargador y reserva o
  cantidad), la munición de cada tipo, trampas y materiales.
- **Controles:** cruceta izquierda/derecha elige; **X** coge el objeto y **X** otra vez lo
  deja en la ranura elegida (se intercambian; lo que estaba en la mano sigue en la mano);
  **CUADRADO** lo suelta delante (con sus balas, se puede volver a recoger); **TRIÁNGULO**
  lo lleva a la mano; **CÍRCULO** cierra (si hay algo cogido, primero lo suelta).
- La partida sigue mientras está abierto (como en Fortnite) y el jugador no se mueve.
- Corregido antes de publicar: cerrar con CÍRCULO activaba a la vez el modo construcción.
- Código: `App::updateInventoryScreen`, `Match::moveSlot`, `Match::dropInventorySlot`,
  `Match::selectSlot`, `Inventory::swapSlots`.

## 3. Miras

- **Cruz arreglada:** el brazo izquierdo y el de arriba medían 7 px y tocaban el punto
  central; el derecho y el de abajo dejaban un hueco. Ahora los cuatro miden 6 px y están a
  la misma distancia del centro (240, 136).
- **Escopeta táctica:** la misma mira cuadrada que la de corredera (todas las escopetas).
- **Lanzagranadas:** un círculo y, debajo, tres marcas de la caída de la granada a 5, 10 y
  15 m (calculadas con su velocidad y su gravedad, `game::kGrenadeGravity`), partidas en dos
  para que no las tape el personaje.

## 4. Pavos

50 por victoria magistral y 50 por logro completado, retrocompatible: ver
[TIENDA.md](TIENDA.md).

## 5. Tienda de objetos

Sección TIENDA del lobby con los trajes de los personajes de la CPU (poco comunes, 800
pavos): ver [TIENDA.md](TIENDA.md).

## 6. Sonidos

- **De `Sonidos Limpios` (nuevos):**
  - `pistoladisparo.wav` → disparo de la pistola (sustituye al sintetizado);
  - `disparorevolver.wav` → disparo del revólver;
  - `pumpshotgun.wav` → disparo y corredera de la escopeta de corredera (la táctica sigue
    con el suyo);
  - `disparoyrecargarocketlauncher.wav` → partido en dos: el disparo y vuelo del cohete
    (0-1.9 s) y la recarga (desde 2.6 s);
  - `botiquín.wav` (recortado a 10 s, lo que dura) y `vendas.wav` → mientras se usan; se
    cortan al cancelar o al terminar (entonces suena la cura).
- **Recargas:** cada familia tiene la suya (fusiles, escopeta de corredera, escopeta
  táctica, subfusiles, pistola, revólver, francotiradores, lanzagranadas y lanzacohetes). Se
  cortan si la recarga se interrumpe (cambiar de arma, el pico, construir).
- **Descarga de YouTube:** se buscaron grabaciones limpias de las recargas de Fortnite y se
  intentó descargarlas, pero la política de permisos de esta sesión bloqueó la descarga (la
  tomó por saltarse la protección anti-bots de YouTube), así que **las recargas son sonidos
  sintetizados propios** (`tools/generate_sounds.py`: chasquidos metálicos, cargadores,
  cartuchos y correderas, cada uno algo más corto que la recarga más rápida de su arma).
- **Para usar grabaciones:** basta con dejar en `Recursos/Sonidos/Sonidos Limpios/` un
  fichero con el mismo nombre (`recarga_fusil`, `recarga_escopeta`, `recarga_tactica`,
  `recarga_subfusil`, `recarga_pistola`, `recarga_revolver`, `recarga_francotirador`,
  `recarga_lanzagranadas`; `.wav` o `.mp3`) y ejecutar `make assets && make`: sustituye al
  sintetizado.

## 7. Bots: botín y materiales

- **Sin materiales al aterrizar.** Cada pared de cobertura les cuesta 10 de su material
  (el que más tengan); sin materiales no construyen.
- **Picar (estado nuevo).** Con arma, sin nadie cerca y con menos de 40 de material, van al
  objeto del mapa más cercano que se pueda picar desde el suelo (árboles, rocas, cajas,
  contenedores, vallas; no tejados ni paneles altos) y lo pican hasta tener 120. Se ve el
  pico y el golpe, y se oye cerca.
- **Materiales del suelo:** se quedan los montones que pisan (de los caídos o de la llama)
  y van a por ellos si les faltan.
- **Al caer** sueltan exactamente los materiales que llevaban (antes eran inventados).
- **Botín:** hasta tener un inventario completo (dos armas y curas) buscan botín a menudo
  (35 % de las decisiones, hasta 32 m): abren cofres y cambian un arma por otra mejor
  (`weaponScore`: tipo y rareza) hasta armarse; después, de vez en cuando (12 %).

## 8. Bots: con poca vida, huir y curarse

- Con vida + escudo por debajo de 45 (`MatchConfig::botFleeHealth`), si ven a un rival o
  les acaban de dar, **huyen** (estado nuevo): buscan un punto a ~16 m alejándose, mejor si
  el rival no lo ve (detrás de una nave, un contenedor…) y nunca hacia la tormenta.
- Lo primero, una **pared** entre los dos (si tienen materiales).
- **No disparan** mientras huyen. Si el rival está encima (a menos de 5 m) no tienen
  escapatoria y pelean.
- **A salvo** (nadie les ve) se **curan** con lo que lleven; si no llevan curas, van a por
  ellas (curativos del suelo, cofres, la llama).
- Vuelven a lo suyo con 75 de vida + escudo, o tras 25 s sin que nadie les vea.
- En las partidas simuladas: 20-75 huidas por partida; las partidas duran algo más
  (150-175 s con 10 bots frente a 90-120 s antes).

## 9. Escudo azul

El número de daño sale **azul** si el rival tenía escudo (más claro si es a la cabeza),
como en Fortnite; sin escudo, blanco (o amarillo a la cabeza). Vale para balas,
explosiones, pico y trampas (`DamageNumber::shield`).

## 10. Menú de administrador (mod menu)

- **AJUSTES → ADMIN → Contraseña**: teclado en pantalla (TRIÁNGULO mayúsculas/minúsculas,
  START acepta). Con `adminpezporno` se desbloquea y se abre el **MOD MENU** (no distingue
  mayúsculas). Una contraseña incorrecta no desbloquea nada.
- Desbloqueado, ADMIN tiene **Abrir MOD MENU** y **Bloquear** (quita los trucos). En la
  partida, la pausa (START) tiene una tercera opción, **MOD MENU**, que aplica los cambios
  al momento.
- **Trucos** (se guardan en `ajustes.txt` como `truco_<clave>=1`; sin `admin=1` no se cargan):

| Truco | Qué hace |
| --- | --- |
| Modo dios | No recibes daño |
| Aimbot | Cada disparo va a la cabeza del rival visible más centrado (35°) |
| Wallhack | Caja roja de cada rival, con su vida y escudo, a través de todo |
| Munición infinita | El cargador y las granadas no se gastan |
| Recarga instantánea | Recargar no lleva tiempo |
| Cadencia x3 | Las armas disparan tres veces más rápido |
| Sin dispersión | Precisión perfecta y sin retroceso |
| Un tiro | Tu daño x10 (balas, explosiones y pico) |
| Balas explosivas | Cada disparo explota donde da (¡también te daña!) |
| Atravesar paredes | Tus balas atraviesan paredes y objetos |
| Volar | Saltar (mantenido) sube, agacharse (mantenido) baja |
| Supervelocidad | Corres al doble |
| Supersalto | Saltas tres veces más alto |
| Gravedad lunar | Caes despacio |
| Materiales infinitos | Construir y poner trampas no gasta |
| Invisible | Los bots no te ven ni te oyen |
| Inmune a la tormenta | La tormenta no te hace daño |
| Bots congelados | Los bots se quedan quietos y no disparan |
| Fiesta | Los bots se ponen a bailar (y no pelean) |
| Radar de botín | Cofres y llamas en el minimapa |
| Tiempo bala | Todo va a la mitad de velocidad |
| Llama siempre | Hay llama en todas las partidas |
| Kit legendario | Empiezas con armas doradas, curas, munición y 500 de cada material |

- **Acciones** (solo desde la pausa, en partida): vida y escudo al máximo; llenar munición,
  materiales y trampas; eliminar a todos los bots (da la victoria y sus pavos).

## 11. Modo Monodisparo y lobby nuevo

- **Lobby sin cruceta:** **TRIÁNGULO** juega, **CUADRADO** abre el panel de modo de juego y
  bots, **SELECT** abre los controles y **CÍRCULO** sale (con confirmación). Cada botón de
  la columna derecha lleva su icono. L/R siguen cambiando de sección (JUGAR, LOGROS, TIENDA,
  TAQUILLA) y START abre los ajustes.
- **Panel de modo (CUADRADO):** arriba/abajo elige la fila, izquierda/derecha cambia el modo
  (BATTLE ROYALE / MONODISPARO) o los bots (5-30); CUADRADO, X o CÍRCULO lo cierran.
- **Monodisparo:** en todo el botín (suelo, cofres, airdrops y llama) solo hay
  francotiradores (cerrojo y semiautomático), el fusil con mira y granadas (la mitad del
  botín que no es arma), más los curativos; ni el resto de armas ni trampas. La munición que
  sale es la de esas armas (pesada y media). Los bots juegan con lo mismo.

## Comprobado (host, no PSP)

- `make match-sim`: prueba 19 nueva (escudo azul, objeto roto a tiros, bots que construyen
  solo con materiales y pican, huida sin disparar y cura a salvo, acorralado pelea,
  inventario, trucos, Monodisparo) y las partidas completas de la prueba 15 con los bots
  nuevos. **0 fallos en 206 comprobaciones.**
- `make app-sim`: sección 10 nueva con el mando simulado (perfil antiguo con pavos y logros
  por recoger, recoger una recompensa, tienda sin pavos / confirmar / cancelar / comprar /
  equipar, taquilla, contraseña incorrecta y correcta, mod menu guardado, modo dios y
  wallhack en partida, número azul, L+R a la vez y no seguidos, mover y soltar en el
  inventario, miras, sonido de recarga que se corta, MOD MENU en la pausa, victoria con 50
  pavos, bloquear el menu) y el lobby nuevo en las secciones de siempre. **0 fallos en 164
  comprobaciones.** Capturas `05g_*`, `40*`–`45*` en `build/app_sim/`.
- `make physics-test`: 0 fallos. `make preview`: sin cambios.

## Pendiente de comprobar en la PSP

- Rendimiento con 30 bots ahora que pican, huyen (más raycasts al buscar escondite) y que
  las balas rompen objetos (más actualizaciones de la navegación).
- Que L + R a la vez se detecta bien con los gatillos de la PSP-2000 (ventana de 0.07 s).
- Memoria: el ejecutable pasa de 5.0 a 6.7 MB por los sonidos nuevos (el SDK ya activa la
  memoria ampliada de la PSP-2000).
- Volumen y recorte de las grabaciones nuevas.
