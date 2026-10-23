# Bots, combate y partida (fase 4)

Código en `src/game/`: `bot.h` (estructura `Bot`), `match.*` (IA, disparos, partida),
`nav_grid.*` (navegación) y `match_config.h` (parámetros).

## Entidades

| Id     | Entidad                                                     |
|--------|-------------------------------------------------------------|
| 0      | Jugador                                                     |
| 1 … N  | Bots (`Match::bot(id - 1)`), "Bot 1" … "Bot N"              |

Todas usan la misma física (`CharacterBody`). Cada entidad empieza con 100 de vida, y
cada impacto resta el daño del arma. Las que llegan a 0 quedan eliminadas:
- **No actúan:** ni se mueven ni disparan.
- **No interactúan:** no colisionan con otros personajes, no se les puede disparar y no
  se les elige como objetivo.
- **Se ven tumbadas** durante 8 s y después desaparecen.

## Estructura `Bot`

| Campo                         | Uso                                                  |
|-------------------------------|------------------------------------------------------|
| `body`                        | Posición, orientación (yaw), velocidad y vida        |
| `state`                       | Estado de IA (tabla siguiente)                       |
| `decisionTimer`               | Segundos hasta la próxima decisión                   |
| `target`, `lastSeen`, `targetVisible` | Objetivo y última posición conocida          |
| `path[]`, `pathIndex`         | Camino A* (hasta 48 puntos)                          |
| `progressAnchor`, `stuckCount`, `sidestepTimer` | Detección y salida de atascos      |
| `fireCooldown`, `reaction`, `strafeSign` | Combate                                   |

> **Fase 7:** el estado **Botín**, el autobús y la caída, la caza del rival más cercano, el
> oído y las paredes de cobertura están en [BOTIN.md](BOTIN.md#bots-botín-caza-y-construcción).
> Con `MatchConfig::classic()` los bots se comportan como se describe aquí (fases 4-6).

## Estados de la IA

| Estado          | En el HUD     | Qué hace                                                       |
|-----------------|---------------|----------------------------------------------------------------|
| Explorar        | Explorar      | Elige un destino dentro de la zona (55 % un punto de interés: interiores de las naves, patio, pasillos, lago, bosque…) y pide un camino |
| Desplazarse     | Desplazarse   | Sigue el camino, recortando esquinas si el tramo siguiente está libre |
| Buscar objetivo | Buscar obj.   | Al llegar, se para 2–4 s girando: visión de 360°               |
| Atacar          | Atacar        | Encara al objetivo, se mueve de lado, se aleja si está a menos de 7 m, se acerca si está a más de 26 m y dispara |
| Eliminado       | Eliminado     | Nada                                                           |

**Transiciones:**
- **Ver un objetivo:** desde cualquier estado pasa a Atacar. Espera 0,35 s de reacción
  antes del primer disparo.
- **Perder al objetivo:** va a donde lo vio por última vez, y a los 6 s sin verlo pasa a
  Buscar objetivo.
- **Objetivo eliminado:** pasa a Buscar objetivo.
- **Recibir un impacto:** se gira hacia quien le ha disparado (pasa a Atacar).

### Decisiones a intervalos (no cada fotograma)

- **Qué hace cada `aiDecisionInterval`** (0,25 s por defecto):
  - Percepción: rango, campo de visión y raycasts de línea de visión.
  - Cambios de estado y comprobación de atascos.
  - Peticiones de camino.
- **Escalonado:** el primer temporizador de cada bot está desplazado (bot i:
  `intervalo · (i+1)/N`), así que en cada paso solo piensa una fracción de los bots.
- **Entre decisiones, cada paso de 1/60 s,** el bot solo ejecuta su plan, que es barato:
  - seguir el camino;
  - apartarse de otros personajes;
  - sensor de obstáculos delante;
  - apuntar y disparar.
- **Caminos A\*:** como mucho uno por paso de física. Si se agota ese presupuesto, el bot
  lo pide en su siguiente decisión.

### Percepción (comprobaciones geométricas)

Un objetivo es visible si cumple las tres condiciones:
1. **Rango:** distancia horizontal ≤ `sightRange` (35 m).
2. **Campo de visión:** `dot(adelante, dirección) ≥ cos(fov/2)`, con un FOV de 120°.
   Buscando, el campo es de 360°; y el objetivo actual se sigue aunque salga del cono.
3. **Línea de visión:** un raycast desde los ojos (1,55 m) al pecho (1,25 m) contra las
   cajas `kBlocksShots` no encuentra nada.

Entre los visibles se elige el más cercano, con preferencia por el objetivo actual. Los
bots luchan **todos contra todos** (también entre ellos), como en un Battle Royale.

### Movimiento y atascos

- **Navegación:**
  - **Rejilla:** de 4 m, generada al cargar desde la geometría de colisión. Tiene 2 099
    nodos transitables, conectados en 8 direcciones si el tramo recto está libre.
  - **Puertas:** las de las naves y del recinto están centradas en nodos de la rejilla.
- **Sensor:** si delante (0,9 m) hay una caja, prueba a desviarse 50° a cada lado.
- **Atascos:** cada 1,5 s el bot comprueba si avanzó al menos 0,6 m cuando quería
  moverse. Si no, se aparta de lado 0,7 s y salta. A la segunda vez replanifica desde
  donde está; a la tercera, cambia de destino.
- **Zona simplificada:** los destinos se eligen dentro de un círculo centrado en
  (0, -6). Su radio se cierra de 85 a 22 m en 4 minutos, para que la partida converja
  en el depósito y termine. No tiene efectos visuales.

### Disparos (raycast)

- **Trayectoria:** los disparos son instantáneos. Se recorre el segmento desde el origen
  (bots: ojos; jugador: pecho) hasta el alcance (60 m).
- **Qué cuenta como impacto:** gana lo primero que se encuentre, sea una caja
  `kBlocksShots` (paredes, muros, cajas, troncos…) o la caja de impacto de una entidad
  viva (0,8 × 1,85 m).
- **Qué no detiene un disparo:** la valla metálica, la barandilla y el agua solo bloquean
  el movimiento, así que se puede disparar a través de ellas.

Desde la fase 5 las dos armas usan la clase `Weapon`, con cargador, cadencia y recarga
(ver COMBATE_CONSTRUCCION.md):

| Parámetro (`MatchConfig`) | Bots (`botWeapon`)             | Jugador (`playerWeapon`, SCAR legendaria) |
|---------------------------|--------------------------------|-------------------------------------------|
| Daño                      | 10                             | 36                                        |
| Cargador / reserva        | 30 / infinita                  | 30 / 120                                  |
| Cadencia                  | 0,45 s (±20 %)                 | 5,5 disparos/s (R mantenido)              |
| Recarga                   | 2,5 s                          | 2,0 s                                     |
| Precisión                 | dispersión 2° + 0,07°/m, +1,5° si el objetivo corre, +1° si el bot se mueve | ayuda de puntería de 3° |
| Condición                 | encarado (±26°) y con línea de visión, o disparando a una pared que tapa a su objetivo | rayo de la cámara por la mira, comprobado desde el arma |

**Disparo del jugador:** desde la fase 5 se describe en
[COMBATE_CONSTRUCCION.md](COMBATE_CONSTRUCCION.md). En resumen:
1. Un raycast desde la cámara por la mira encuentra el primer objeto alcanzado.
2. Se comprueba desde el arma; si un sólido corta la línea de tiro, el disparo termina ahí.
3. La ayuda de puntería es de 3°.

**Paredes construidas:**
- **Movimiento y visión:** bloquean a los bots igual que el mapa.
- **Navegación:** se recalcula en la zona de cada pared y los bots replanifican.
- **Objetivo escondido:** si se esconde tras una pared, el bot la rompe a tiros.

### Partida

- **Contador de vivos:** baja una unidad en cada eliminación.
- **Fin de partida:** cuando solo queda una entidad (`Finished`).
  - **Si es el jugador:** "#1 VICTORIA MAGISTRAL", con sus bajas y el tiempo.
  - **Si el jugador cae antes:** queda en fase `Spectating` ("ELIMINADO - Puesto #N").
    La partida sigue entre bots hasta que queda uno ("Ganador: Bot N").
- **Al terminar (fase 6):** pantalla de victoria o de derrota; X vuelve al lobby y □
  empieza otra partida (docs/PRESENTACION.md).
- **Registro:** las 4 últimas eliminaciones se muestran durante 6 s.

## Número de bots y rendimiento

- **Por defecto:** 10 bots (`kDefaultBots`). Capacidad máxima de 30 (`kMaxBots`).
- **Cambiar el número:** en el lobby, con ← → sobre JUGAR, de 5 en 5. Por encima de 10 aparece un
  aviso: primero hay que medir.
- **Medir en la PSP:**
  - **HUD "Depuración"** (SELECT): muestra cada segundo los ms por frame de IA y de
    física, las decisiones por segundo, los triángulos y la lista de bots con estado,
    vida y objetivo.
  - **Registro:** al acabar cada partida se escribe una línea en `fortnitepsp_log.txt`
    con el frame medio y el máximo, el coste de la IA, los caminos, los atascos y los
    disparos.

Medido en el Mac con `make match-sim` (5 partidas de 10 bots y 3 de 30):

| Bots | IA (µs/paso, Mac) | Física (µs/paso, Mac) | Duración de partida | Atascos ≥ 1 s |
|------|-------------------|-----------------------|---------------------|---------------|
| 10   | 0,4–0,9           | 0,2–0,4               | 73–146 s            | 0–2 por partida |
| 30   | 0,9–1,2           | 0,4–0,5               | 80–131 s            | 0–1 por partida |

Estas cifras no son las de la PSP: su CPU a 333 MHz es decenas de veces más lenta. Aun con
un factor de 100, la IA de 30 bots costaría ~0,1 ms por paso, frente a 16,7 ms por frame.
El límite real en la PSP estará en el dibujado: cada bot visible añade 348 triángulos y
~7 llamadas de dibujo. Por eso la recomendación es subir a 20 o 30 bots solo después de
ver en la PSP los FPS y el frame medio que da el registro con 10.

## Verificación (`make match-sim`)

Usa el mismo código de mundo, colisiones, navegación, IA y partida que el juego.
Resultado actual: **0 fallos**.

- **Navegación:** hay camino desde la aparición a los 15 puntos de interés, incluidos
  los interiores de las naves.
- **Colisiones en el mapa real:**
  - la pared de una nave, el muro del recinto y el agua bloquean;
  - la escalera sube al altillo;
  - la barandilla impide caerse.
- **Disparos:**
  - restan vida con línea de visión;
  - se detienen en las paredes, y la pared también corta la visión de la IA;
  - atraviesan la valla metálica;
  - el disparo del jugador por la mira, con la ayuda de puntería, acierta con la mira
    desviada 4° y no atraviesa paredes.
- **Vida y contador:** eliminaciones, contador de vivos, victoria del jugador, puesto
  al caer y fin con un solo superviviente.
- **8 partidas completas (5 de 10 bots, 3 de 30), con el jugador quieto:**
  - todas terminan con un superviviente;
  - el contador de vivos siempre coincide con un recuento y nunca sube;
  - ningún personaje queda dentro de un sólido;
  - ningún impacto atraviesa una pared, comprobado con un raycast independiente de
    cada impacto;
  - ningún bot pasa más de 1 s seguido atascado, y los atascos son menos del 4 % del
    tiempo en marcha.
