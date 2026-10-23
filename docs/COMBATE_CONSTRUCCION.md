# Combate y construcción (fase 5)

Código: `src/game/weapon.*` (arma), `src/game/structures.*` (paredes),
`src/game/match.cpp` (disparos, IA y paredes), `src/app/build_visuals.*` (dibujo de las
paredes y números grandes) y `src/world/collision.*` (cajas dinámicas).

## Controles

| Modo           | Botón                     | Acción                                                    |
|----------------|---------------------------|-----------------------------------------------------------|
| Siempre        | Stick                     | Moverse (relativo a la cámara)                            |
| Siempre        | X                         | Saltar                                                    |
| Siempre        | L + stick                 | Apuntar (ver JUGADOR.md); la cámara sigue al andar        |
| Combate        | Cruceta ← →               | Ranura del inventario (ver INVENTARIO.md)                 |
| Combate        | TRIÁNGULO                 | Pico                                                      |
| Construcción   | Cruceta ← →               | Pieza: pared, suelo o rampa                               |
| Siempre        | **CÍRCULO**               | **Activar / desactivar el modo construcción**             |
| Combate        | R (mantener)              | Disparar la SCAR (cadencia de 5,5 disparos/s)             |
| Combate        | CUADRADO                  | Recargar                                                  |
| Combate        | L (mantener)              | Fijar el bot visible más centrado (sin objetivo: centrar la cámara) |
| Construcción   | R (pulsar)                | Colocar una pared delante (una por pulsación, 10 de madera) |
| Construcción   | CUADRADO                  | Cambiar de material (desde v0.10.0; ver DESTRUCCION.md)   |
| Construcción   | L + stick                 | Mover la mira (sin fijar objetivo)                        |
| Depuración     | TRIÁNGULO                 | Cámara libre (dentro: CUADRADO niebla, START VSync)       |
| Siempre        | SELECT                    | HUD: partida → depuración → controles → oculto            |
| Siempre        | START                     | Pausa                                                     |
| Resultados     | X / CUADRADO / CÍRCULO    | Lobby / otra partida / seguir mirando (solo en derrota)   |
| Siempre        | L + R + START (1 s)       | Salir al XMB                                              |

**Cambios respecto a la fase 4:**
- **CÍRCULO:** ahora es el modo construcción, como pide la fase.
- **Cámara libre:** pasa a TRIÁNGULO.
- **Niebla y VSync:** solo se cambian dentro de la cámara libre, porque CUADRADO es recargar
  o quitar pared.

**Cómo se nota que el modo construcción está activo:**
- **Arriba:** una franja azul con "MODO CONSTRUCCION   Paredes n/20".
- **Debajo de la franja:** la ayuda de botones o, en rojo, por qué no se puede construir.
- **En el centro:** una retícula de cuatro esquinas, azul si se puede construir y roja si no.
- **En la escena:** la pared fantasma, azul o roja, y la pared que se quitaría resaltada en
  rojo.
- **Abajo a la derecha:** el panel muestra el número de paredes en vez de la munición.

## Arma: SCAR legendaria

`WeaponConfig` (todo configurable en `MatchConfig::playerWeapon`):

| Parámetro        | Jugador (SCAR legendaria) | Bots (fusil común) |
|------------------|---------------------------|--------------------|
| Daño             | **36**                    | 10                 |
| Cargador         | **30**                    | 30                 |
| Reserva          | 120                       | infinita           |
| Cadencia         | 5,5 disparos/s (0,18 s)   | 0,45 s (±20 %)     |
| Recarga          | 2,0 s                     | 2,5 s              |
| Alcance          | 80 m                      | 60 m               |

- **Cadencia:** `tryFire()` solo dispara si ha pasado el intervalo. Mantener R da como
  mucho un disparo por paso de física (1/60 s) y nunca ráfagas tras un tirón de FPS. Para
  que la media sea exactamente 5,5 por segundo se arrastra el sobrante de un paso (como
  mucho 20 ms).
- **Recarga:** al vaciar el cargador recarga sola, o con CUADRADO antes de vaciarlo.
  Mientras recarga no dispara, y al terminar toma las balas de la reserva.
- **Sin munición:** sin balas en el cargador ni en la reserva, el arma no dispara.

### Apuntado y disparo

1. **Rayo desde la cámara** por el centro de la mira. Empieza a la altura del jugador, así
   que lo que haya entre la cámara y él no cuenta. Se busca el **primer objeto alcanzado**:
   cajas que bloquean disparos (paredes del mapa, muros, cajas, troncos, rocas y paredes
   construidas) o la caja de impacto de un bot vivo.
2. **Comprobación desde el arma:** si un sólido corta el segmento entre el pecho del jugador
   y ese punto, el disparo termina en el sólido. Así no se puede disparar por encima de un
   muro que la cámara ve y el arma no.
3. **Resultado:**
   - **Bot:** pierde 36 de vida y aparece el **número de daño** flotando sobre él, como en
     Fortnite (en blanco, sube y se desvanece en 0,9 s).
   - **Pared construida:** pierde 36 de vida y aparece un número más pequeño y azulado.
   - **Pared del mapa:** el disparo termina ahí.
- **Ayuda de puntería:** pequeña, de 3° (`aimAssistDegrees`). Solo actúa si un bot visible
  está a menos de 3° de la mira; entonces el rayo de la cámara se dirige a su pecho. Con
  más desviación manda el centro de la mira.
- **L fija objetivo:** gira la cámara hacia el bot, de modo que el centro de la mira queda
  sobre él.

**HUD de combate:**
- **Retícula:** cruz con hueco y punto central, roja si hay objetivo.
- **Impacto:** marcador en X al acertar.
- **Munición (fase 6, como `Referencias/Referencia gameplay.png`):** "30 | 120" en grande
  abajo a la izquierda; durante la recarga, barra dorada y "RECARGANDO"; "SIN BALAS" si no
  queda nada. La SCAR ocupa la ranura dorada de la fila de armas. Ver docs/PRESENTACION.md.

## Construcción

**Pared:**
- **Medidas:** 4 m de ancho, 3 m de alto y 0,25 m de grosor, de madera marrón (textura
  `Recursos/Texturas/15_madera_pared.png`).
- **Posición:** se ajusta a una rejilla de 4 m. Va sobre la primera línea de la rejilla
  que quede al menos 1 m por delante, perpendicular a la dirección cardinal más cercana a
  la de la cámara.
- **Base:** el suelo o la plataforma bajo la pared.
- **Bajo un dintel:** se recorta para encajar (en las puertas de 2,8 m de las naves, por
  ejemplo).

**Validación antes de colocar** (`StructureSystem::plan`):

| Comprobación                                   | Mensaje                       |
|------------------------------------------------|-------------------------------|
| Hay ya `maxWalls` paredes (20 por defecto)     | Limite de paredes alcanzado   |
| Fuera del mapa                                 | Fuera del mapa                |
| Solaparía el cuerpo del jugador                | Estas dentro de la pared      |
| Choca con el mapa o duplica otra pared         | Choca con otra estructura     |
| Hay un sólido entre el jugador y la pared      | Hay un obstaculo en medio     |
| Solaparía el cuerpo de un bot vivo             | Hay alguien en medio          |

Dos paredes perpendiculares que comparten esquina son compatibles: en la rejilla solo
pueden tocarse por los extremos. Así se pueden cerrar recintos.

**Quitar paredes:** hasta v0.9.1, CUADRADO quitaba la pared de la mira. Desde v0.10.0
cambia el material y las piezas se rompen con el pico (ver DESTRUCCION.md).

**Vida:** cada pared tiene 150 de vida. La SCAR la rompe en 5 impactos y el fusil de los
bots en 15.

### Mismos datos para física, combate y dibujo

Cada pared es una caja dinámica de `CollisionWorld`. Esa misma caja se usa en todas
partes, así que lo que se ve es exactamente lo que bloquea:
- **Física:** la de jugador y bots.
- **IA:** línea de visión y sensor de obstáculos.
- **Disparos:** los raycasts.
- **Cámara:** su colisión con las paredes.
- **Dibujo:** la malla se coloca con esa caja (traslación más escala en Y si está
  recortada).

### La IA y las paredes

- **Movimiento:** las paredes bloquean el movimiento de los bots (misma física) y su
  sensor de obstáculos las detecta.
- **Navegación:** al colocar o romper una pared se recalcula la navegación de la zona
  (`NavGrid::updateRegion`, unas decenas de nodos). Los bots que se estaban moviendo piden
  un camino nuevo, que rodea las paredes.
- **Visión:** los bots no ven a través de las paredes.
- **Romper paredes:** si su objetivo se esconde tras una pared construida a distancia de
  combate, disparan a esa pared hasta romperla. Si no, un jugador encerrado sería
  invulnerable.

## Verificación (`make match-sim`, 0 fallos)

- **Arma:**
  - 10 s con el gatillo mantenido: nunca más de un disparo por paso; las 30 balas salen
    a 5,5/s y la recarga dura 2 s.
  - Mismo número de disparos a 20, 60 y 144 FPS.
  - Con la reserva agotada no dispara, y mientras recarga tampoco.
- **Disparo por la mira:**
  - De dos bots en línea, solo el primero recibe los 36, con su número de daño.
  - Tres impactos eliminan a un bot.
  - Si la cámara ve por encima de un murete pero el arma no, el disparo termina en el
    murete.
  - 2 s con R mantenido dan 11 disparos y gastan 11 balas.
  - La ayuda de puntería corrige 2° de desviación pero no 5°.
- **Construcción:**
  - Coloca una pared válida y rechaza una pared repetida, una encima de un bot, una más
    allá del límite, una dentro del contenedor de la nave gris y otra al otro lado de la
    fachada de una nave.
  - Permite la esquina perpendicular y recorta la pared bajo el dintel de una puerta.
  - Quita la pared apuntada.
  - Una partida nueva empieza sin paredes.
  - La pared bloquea al jugador, los disparos y la visión de la IA, y se rompe en el quinto
    impacto.
- **IA:**
  - Con una barrera de 3 paredes, el camino A* la rodea y el bot llega al otro lado sin
    atravesarla.
  - Un bot que dispara al jugador no le acierta mientras este está encerrado entre 4
    paredes; rompe una a tiros (16 disparos) y entonces sí.
  - Ningún impacto atraviesa paredes, comprobado con un raycast independiente.
