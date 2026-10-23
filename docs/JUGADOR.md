# Jugador: movimiento, física y cámara en tercera persona

Código en `src/game/` (física común en `character.*`, entrada del jugador en `player.*`,
modelo en `character_model.*`) y `src/camera/third_person_camera.*`. Los bots usan la
misma física: ver [BOTS.md](BOTS.md).
Las coordenadas y los ejes son los de [MUNDO.md](MUNDO.md): 1 unidad = 1 m, +Y arriba.

## Estado (`game::CharacterState`, alias `game::Player`)

| Campo        | Significado                                                          |
|--------------|----------------------------------------------------------------------|
| `position`   | Pies del jugador, en coordenadas de mundo (m)                        |
| `velocity`   | m/s                                                                  |
| `yaw`        | Orientación: mira hacia `(-sin yaw, 0, -cos yaw)`; 0 = norte (-Z)    |
| `health`     | Vida (100; los impactos la restan y a 0 el personaje queda eliminado) |
| `jumpState`  | `Grounded` (en suelo), `Rising` (saltando), `Falling` (cayendo)      |
| `airTime`    | Segundos en el aire                                                  |

## Controles en partida

| Botón                      | Acción                                                      |
|----------------------------|-------------------------------------------------------------|
| Stick analógico            | Andar/correr (la velocidad depende de cuánto se incline)    |
| X                          | Saltar (mantener = salto completo, tocar = salto corto)     |
| Cruceta ← / →              | Girar la cámara alrededor del jugador                       |
| Cruceta ↑ / ↓              | Mirar arriba / abajo                                        |
| R (mantener)               | Disparar la SCAR (5,5 disparos/s, ver COMBATE_CONSTRUCCION.md) |
| CUADRADO                   | Recargar (en construcción: quitar la pared apuntada)        |
| L (mantener)               | Fijar el bot visible más centrado (cono de 35°, 45 m); sin objetivo, recoloca la cámara detrás |
| CÍRCULO                    | Modo construcción (R coloca paredes)                        |
| TRIÁNGULO                  | Cámara libre de depuración (dentro: CUADRADO niebla, START VSync) |
| SELECT                     | HUD: partida → depuración → controles → oculto              |
| START                      | Pausa (CONTINUAR / VOLVER AL LOBBY)                         |
| Resultados                 | X lobby, CUADRADO otra partida, CÍRCULO seguir mirando (derrota) |
| L + R + START (1 s)        | Salir                                                       |

El stick solo mueve al jugador; la cámara se maneja con la cruceta y los gatillos
apuntan y disparan, así que no hay combinaciones que compitan con el movimiento. Como la
PSP solo tiene un stick y el pulgar izquierdo no puede mover y girar a la vez, L fija el
objetivo y la ayuda de puntería corrige pequeños desvíos. Al mantener L + R + START para
salir, fijar y disparar no tienen otro efecto.

## Movimiento relativo a la cámara

```
adelante = (-sin camYaw, 0, -cos camYaw)        derecha = (cos camYaw, 0, -sin camYaw)
deseo    = adelante * (-stickY) + derecha * stickX        (longitud <= 1)
```

- **Stick:** zona muerta **radial** del 20 %, reescalada para que la salida empiece en 0
  justo al salir de la zona muerta y no pase de 1. Inclinar a medias equivale a andar.
- **Velocidad:** la velocidad horizontal se acerca a `deseo * 5.5 m/s` con aceleración
  limitada:

  | Situación            | Aceleración   |
  |----------------------|---------------|
  | En el suelo          | 45 m/s²       |
  | Frenando             | 60 m/s²       |
  | En el aire (control) | 12 m/s²       |

- **Giro del modelo:** el modelo gira hacia donde se mueve, como mucho 12 rad/s. Al
  disparar o fijar objetivo se orienta con la cámara, para poder andar de lado o de
  espaldas mientras apunta (pose de rifle al frente).
- **Estado en el HUD:** "quieto" por debajo de 0,2 m/s, "andando" por debajo de 3,3 m/s
  y "corriendo" por encima.

## Salto y gravedad

| Parámetro                   | Valor     | Efecto                                         |
|-----------------------------|-----------|------------------------------------------------|
| Gravedad                    | 24 m/s²   | Más que 9,8: salto ágil, "de juego"            |
| Velocidad de salto          | 8 m/s     | Altura máxima ~1,27 m (teórica 1,33 m)         |
| Recorte al soltar X subiendo | ×0,5     | Un toque corto sube ~0,4–0,65 m                |
| Velocidad terminal          | 40 m/s    |                                                |
| Buffer de salto             | 0,12 s    | Pulsar X justo antes de aterrizar también salta |
| Tiempo "coyote"             | 0,10 s    | Margen para saltar tras dejar un borde         |

**Colisiones** (detalle en MUNDO.md):
- **Movimiento horizontal:** el cuerpo (0,7 × 1,8 m) se mueve eje a eje contra las cajas
  AABB que bloquean el movimiento.
- **Escalones:** los de hasta 0,45 m se suben solos (escaleras de las naves).
- **Apoyo:** el personaje se apoya en el techo de caja más alto bajo los pies (cajas,
  altillo) o en el suelo (Y = 0).
- **Caídas y techos:** al caer se buscan las plataformas que estaban por debajo de los
  pies antes del paso, así que no hay tunelado ni a velocidad terminal. Los techos
  detienen el salto.

**Límite del mapa:** el jugador no sale de `X, Z ∈ [-95.5, 95.5]`.

## Paso fijo, delta time e interpolación

```
cada frame:  dt (medido con sceKernelGetSystemTimeWide)
             -> se ajusta a N pasos exactos si esta a menos del 2 % (VSync a 59.94 Hz)
             -> como mucho 0,1 s por frame (el exceso se descarta)
             -> N pasos de fisica de 1/60 s
             -> estado dibujado = lerp(estado anterior, estado actual, alpha)
```

- **Separación física/render:**
  - `App::update()` lee la entrada, simula los pasos y mueve las cámaras.
  - `App::render()` solo dibuja el estado interpolado.
  - La animación del modelo y el suavizado de la cámara son visuales y usan el `dt` del
    frame con suavizado exponencial (`1 - e^(-k·dt)`), que no depende de los FPS.
- **Límite de desplazamiento:**
  - **Por paso:** la velocidad horizontal está acotada a 5,5 m/s, es decir 9 cm por
    paso.
  - **Por frame:** como mucho 6 pasos, unos 0,55 m corriendo.
  - **Tirones:** si un frame dura más de 0,1 s, el juego se ralentiza en vez de
    teletransportar al jugador. El HUD muestra el tiempo descartado acumulado ("desc").
- **Pulsación de salto:** se guarda en un buffer, así que no se pierde aunque a más de
  60 FPS haya frames sin ningún paso de física.

## Cámara en tercera persona

Desde la fase 4 la cámara está pensada para apuntar:

```
pivote = posición suavizada del jugador + (0, 1.7, 0)
ojo    = pivote + atrás(yaw) · 5·cos(0.24) + (0, 5·sin(0.24), 0)     (~1,2 m por encima)
mira   = dirección (yaw, inclinación), con inclinación de -4° por defecto
```

- **Encuadre:** el ojo está por encima de la cabeza y la mira, casi horizontal, así que
  el personaje queda bajo la mira (como en `Referencias/Referencia gameplay.png`) y la
  mira señala a la altura del pecho de los enemigos.
- **Cruceta ↑↓:** ajusta la inclinación de la mira entre -29° y +26°.
- **Fijar objetivo (L):** encara al bot fijado en horizontal y en vertical.
- **Paredes:** se lanza un raycast del pivote al ojo deseado. Si hay una pared, el ojo se
  acerca al jugador, de modo que dentro de las naves la cámara no queda al otro lado de
  la pared.
- **Suavizado:**

  | Componente | Constante | Nota                                  |
  |------------|-----------|---------------------------------------|
  | Horizontal | k = 12    |                                       |
  | Vertical   | k = 7     | Más lento: el salto no sacude la cámara |
  | Ángulos    | k = 14    |                                       |

  Corriendo, la cámara va unos 0,4 m por detrás y al parar se centra.
- **Altura mínima:** el ojo nunca baja de 0,7 m sobre el suelo (1,155 × plano cercano de
  0,5 m; ver MUNDO.md).

## Modelo provisional

Se basa en `Referencias/Personaje/` y está hecho de 29 prismas (348 triángulos), rifle
incluido. Lo comparten el jugador y los bots con cuatro paletas: la del jugador sigue la
referencia y los bots usan "comando" (negro), "naranja" y "ranger" (rojo y azul con
casco).

- **Cabeza:** pelo rubio y ojos oscuros, que marcan el frente.
- **Torso:** camiseta oliva sin mangas, pañuelo claro, cinturón con hebilla y bolsa en
  la cadera.
- **Brazos:** piel, brazalete rojo en el brazo derecho y guantes oscuros. El derecho
  lleva un rifle de prismas del color tostado de `Referencias/Armas`.
- **Piernas:** pantalón de camuflaje (textura `10_camuflaje.png` de 32×32), correas oscuras y botas
  con rodilleras oliva.

Hay cinco piezas: cuerpo, dos piernas y dos brazos. Piernas y brazos giran sobre la
cadera y el hombro:

- **Al andar:** balanceo proporcional a la velocidad, con la fase ligada a la distancia
  recorrida (zancada de 1,6 m), así que los pies no patinan.
- **En el aire:** pose de salto.
- **Apuntando:** rifle al frente.
- **Eliminado:** tumbado boca arriba.

Bajo los pies hay una sombra circular semitransparente que encoge al subir y ayuda a
leer la altura del salto.

## Pendiente para fases siguientes

- **Iluminación del modelo:** está horneada en el espacio del modelo, así que la cara
  iluminada gira con el personaje.
- **Sombra:** cuando el personaje salta, se dibuja a la altura del suelo del mapa
  (Y = 0), no sobre cajas ni sobre el altillo.

## Verificación

`make physics-test` ejecuta en el Mac el mismo `PlayerController`, el mismo
`FixedTimestep` y la misma cámara del juego:

- **Direcciones:** comprueba las cuatro, relativas a la cámara.
- **FPS:** la posición final es idéntica a 60, 30, 20, 15 y 144 FPS.
- **Salto:** altura, salto variable y aterrizaje.
- **Suelo:** nunca se atraviesa, tampoco a 10 FPS ni con un tirón de 0,5 s.
- **Cámara:** el seguimiento es suave e independiente de los FPS.
- **Colisiones (fase 4):**
  - las paredes detienen, también en diagonal y en esquinas;
  - no hay tunelado a 10 FPS;
  - se suben escalones de 0,36 m y uno de 0,6 m bloquea;
  - saltando se sube a una caja de 1,2 m;
  - los techos detienen el salto;
  - una caída de 35 m sobre una plataforma de 10 cm aterriza encima.

`make preview` dibuja el modelo y la cámara en tercera persona por software.
