# Trampas (parche, fase 8)

Recursos: `Recursos/HUD/spiketrap.webp` (icono) y `Referencias/FortniteFloorTrap.jpg.avif`
(aspecto). Son los únicos recursos de trampas del proyecto, así que solo hay un tipo: la
**trampa de pinchos**. El sistema (`game/match_traps.cpp`, `game::Trap`,
`game::TrapPlacement`) es común: `TrapType` admite más tipos con su propio efecto.

## Conseguirlas

- Botín: a veces junto a un cofre (20 %), en el suelo (4 % de los puntos de botín) y 2 en la
  llama. Se recogen con CUADRADO y van a la barra de construcción (no ocupan ranura), hasta
  `MatchConfig::maxTrapsCarried` (9).

## Colocarlas

- CÍRCULO (construcción) → cruceta hasta la cuarta casilla (la trampa) → R.
- Se apunta con la mira a una superficie a menos de `trapReach` (6 m). Si la mira no toca
  nada a esa distancia, la trampa va al suelo justo delante, como las piezas.
- **Superficies**: suelo del mapa; caras de las cajas de los edificios (techos, paredes,
  altillos); paredes construidas (cualquiera de sus caras), suelos construidos (encima =
  trampa de suelo; debajo = trampa de techo). En rampas no.
- **Ajuste**: en una pieza construida la placa cubre la cara entera (menos 25 cm de margen);
  en el mapa mide como mucho 2.4 × 2.4 m, centrada en el punto apuntado y dentro de la cara.
  La placa queda orientada con la normal de la cara (suelo hacia arriba, techo hacia
  abajo, pared hacia fuera).
- **No se puede** si: no quedan trampas, la cara es menor de 1 × 1 m (un tronco), ya hay
  otra trampa en esa cara, hay algo sólido justo delante, o hay 16 colocadas. El motivo
  sale en rojo bajo el rótulo de construcción y la placa fantasma se ve roja.

## Funcionamiento

- Salta cuando un personaje que no sea quien la puso entra en el volumen de delante de la
  placa (1.2 m sobre un suelo, 1 m delante de una pared, 1.7 m bajo un techo): **100 de
  daño a cada uno, una vez** (`trapDamage`), y después **3 s** de espera (`trapCooldown`)
  antes de poder volver a saltar. Los pinchos salen en 0.06 s y vuelven a bajar.
- Si la pieza construida o el objeto del mapa en el que estaba se rompe, la trampa
  desaparece con él.
- Sonido propio (`trampa.wav`, sintetizado).
