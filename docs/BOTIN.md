# Botín, armas, autobús y bots de la fase 7

## Armas

`game::weaponConfig(tipo, rareza)` (`game/inventory.cpp`), con los valores de Fortnite
(capítulo 1). Cada arma del inventario guarda su cargador; la reserva es la **munición
común** del inventario, por tipo.

| Arma                                   | Daño por rareza (C / PC / R / É / L) | Cadencia  | Cargador | Recarga    | Munición  | Dispersión (cadera / apuntando) |
|----------------------------------------|--------------------------------------|-----------|----------|------------|-----------|---------------------------------|
| Fusil de asalto (SCAR en épica y legendaria) | 30 / 31 / 33 / 35 / 36         | 5,5/s     | 30       | 2,7–2,2 s  | Media     | 2,4° / 0,7°                     |
| Escopeta de corredera                  | — / 90 / 95 / 100 / 105 (10 perdigones) | 1 cada 0,9 s | 5   | 4,8–4,2 s  | Cartuchos | 6° / 4,5° (cono)                |
| Subfusil con silenciador               | 17 / 18 / 19 / 20 / 21               | 10/s      | 30       | 2,3–1,9 s  | Ligera    | 3,4° / 1,8°                     |
| Francotirador de cerrojo               | — / — / 105 / 110 / 116              | 1 cada 2,6 s | 1     | 3,1–2,7 s  | Pesada    | 7° / 0° (exacto apuntando)      |

- **Caída del daño:** la escopeta baja del 100 % a 8 m al 35 % a 25 m; el subfusil, del
  100 % a 20 m al 60 % a 45 m.
- **Números de daño:** los perdigones de un mismo disparo suman en un solo número, como en
  Fortnite.
- **Sonidos:** el fusil usa `scardisparo` y el francotirador `fusil de tirador de
  cerrojo` (de `Sonidos Limpios`). La escopeta y el subfusil tienen sonidos sintetizados
  nuevos.
- **Iconos del HUD:** las siluetas de `Recursos/HUD/Armas/` se recortan de las fotos de
  `Referencias/Armas` con `tools/generate_weapon_icons.py` y se ponen sobre el fondo de
  cada rareza. La SCAR legendaria usa la ranura de `Recursos/Weapons`.

### Puntería: más difícil que antes

| Antes (hasta v0.7.3)                                  | Ahora                                                       |
|-------------------------------------------------------|-------------------------------------------------------------|
| Ayuda de puntería de 3°                               | 1°                                                          |
| Ninguna dispersión: la bala iba siempre al centro de la mira | Dispersión según el arma; +1° al moverse, +3° en el aire |
| L fijaba el objetivo en un cono de 35° y la cámara se clavaba en él al instante | Cono de 22° (40 m) y la cámara gira hacia él a 1,6 rad/s como máximo: si se mueve, hay que seguirlo |

- **Primer disparo exacto:** apuntando con L, quieto y sin haber disparado en 0,35 s, el
  primer disparo va al centro (como en Fortnite). La escopeta abre siempre su cono.
- **Medido en `match_sim`:** con el fusil a 30 m, sin apuntar entran 3 de 10 disparos;
  apuntando quieto (un disparo cada 0,5 s), 10 de 10.

## Botín

Se reparte al empezar cada partida sobre puntos transitables de la rejilla de navegación,
fuera de la tormenta y separados entre sí (la mitad de los cofres cerca de los puntos de
interés, dentro de las naves):

| Qué                   | Cuántos | Contenido                                                                |
|-----------------------|---------|--------------------------------------------------------------------------|
| Cofres                | 22      | Un arma (rarezas de cofre), su munición, 65 % un consumible y 30 de un material para quien lo abre |
| Cajas de munición     | 16      | Dos paquetes de munición                                                 |
| Botín de suelo        | 55 puntos | 55 % un arma con su munición al lado, 30 % un consumible, 15 % munición |
| Airdrops              | 2 (a los 75 y 165 s) | Dos armas legendarias distintas con su munición, 2 pociones de escudo y munición media |

- **Rarezas:**

  | Origen | Común | Poco común | Rara | Épica | Legendaria |
  |--------|-------|------------|------|-------|------------|
  | Suelo  | 45 %  | 32 %       | 16 % | 6 %   | 1 %        |
  | Cofre  | 20 %  | 35 %       | 28 % | 13 %  | 4 %        |

  Si el arma no existe en esa rareza, se usa la más cercana.
- **Armas de los eliminados:** al caer un bot, su arma y munición quedan en el suelo.
- **Airdrop:**
  - **Llegada:** baja desde 60 m con su globo a 3,5 m/s (unos 17 s), a un punto cerca
    del centro de la zona.
  - **Aviso:** "Suministro aéreo en camino" en el HUD y un cuadro azul en el minimapa.
  - **Abrir:** no se puede hasta que toca el suelo.

### Recoger y abrir: CUADRADO

- **Prioridad:** CUADRADO abre o recoge lo que haya al alcance y, si no hay nada, recarga
  (como en Fortnite).
  - **Alcance:** 2 m para los objetos y 2,3 m para cofres y cajas.
  - **Con varios a la vez:** se prefiere lo que está delante.
- **Aviso en el HUD:** "[□] Abrir cofre", "[□] Recoger Escopeta (Épica)", en el color de
  la rareza.
- **Munición:** se recoge sola al pasar por encima, hasta 999 por tipo.
- **Inventario lleno:** se cambia por lo que se lleva en la mano, que cae al suelo ("[□]
  Cambiar por…").
- **Manos libres:** con el pico o una ranura vacía, se coge en la mano lo recogido.
- **Inventario inicial:** como en Fortnite, sin nada (solo el pico) y sin materiales.

### Cómo se ve

En `app/prop_visuals.*`, todo low-poly como el escenario y siguiendo `Referencias/`:
- **Contenedores:** cofre de madera dorada (con la tapa abierta una vez abierto), caja de
  munición verde con franja amarilla y airdrop azul con globo.
- **Botín de suelo:** armas, consumibles y munición que giran sobre un haz de luz del
  color de su rareza.
- **Autobús y caída:** autobús de batalla azul con globo y ala delta azul.
- **Sonido:** cerca de un cofre sin abrir se oye su zumbido (`Fortnite Chest(sin abrir)`,
  5 s que se repiten) y al abrirlo suena `Chestopening`.

## Autobús de batalla y caída

Todos empiezan dentro de un autobús **quieto en el cielo**, a 90 m de altura sobre el
centro del mapa (`MatchConfig::busPosition`).

1. **En el autobús:** X salta (si no, salto automático a los 25 s). El stick gira la
   cámara alrededor del autobús. Suena `Saltar del autobus`.
2. **Caída libre:** el personaje cae boca abajo con brazos y piernas abiertos.
   - **Velocidades:** 20 m/s hacia abajo y 13 m/s en horizontal, dirigiendo con el stick.
   - **Abrir el ala delta:** con X; si no, se abre sola a 26 m del suelo. Suena `Opertura
     ala delta`.
3. **Ala delta:** se baja a 6 m/s y se avanza a 9 m/s.
4. **Suelo:** al aterrizar empieza la partida a pie.

En el autobús no se puede recibir daño ni ser objetivo. En el aire no se dispara ni se
construye.

- **Cámara:** más lejos y mirando hacia abajo.
- **Niebla:** se aleja con la altura, para que desde arriba se vea el mapa.
- **HUD:** "[X] Saltar del autobús", la cuenta atrás del salto automático, "[X] Abrir el
  ala delta" y la altura.

**Medido en `match_sim`:** al saltar y avanzar, la caída libre dura 3 s, el ala delta se
abre sola a 26 m y se aterriza a los 7,6 s, a unos 70 m del autobús.

## Bots: botín, caza y construcción

Los bots siguen el mismo ciclo que el jugador: autobús, caída y botín.
- **Salto:** cada uno salta en un momento distinto, normalmente hacia un cofre, y abre el
  ala delta a una altura distinta.
- **Botín (estado nuevo):** aterrizan solo con el pico y lo primero que hacen es ir a por
  el cofre o el arma más cercana.
  - **Reparto:** cada cofre u objeto lo "reserva" un solo bot.
  - **Con arma:** de vez en cuando van a por una mejor o a por un airdrop caído.
- **Combate según el arma:** cada uno pelea a su distancia (escopeta 7 m, subfusil 15,
  fusil 26, francotirador 38) con el daño real del arma por `botDamageScale` (0,55, la
  dificultad).
  - **Sin arma:** solo pelean si el rival está encima, a golpe de pico.
- **Caza:** sin objetivo, casi la mitad de las veces van hacia el rival más cercano (hasta
  80 m).
- **Oído:** los disparos se oyen hasta 50 m y los bots armados que no pelean van a mirar.
- **Cobertura:**
  - **Cuándo:** al recibir disparos desde más de 5 m, levantan una pared hacia el
    atacante el 60 % de las veces, con 9-12 s de espera entre paredes.
  - **Límites:** máximo 3 por bot y 24 entre todos, sin gastar materiales.
  - **Momento:** se construyen en el paso siguiente al disparo, no a mitad de él.
  - **Tu límite:** no cuentan en tu límite de 20 paredes.

**Partidas simuladas** (`match_sim`, sección 15; el jugador se queda quieto):

| Bots | Duración | Armados al minuto | Paredes de bots |
|------|----------|-------------------|-----------------|
| 10   | 63–69 s  | 10 de 10          | 7–10            |
| 30   | 84 s     | 30 de 30          | 18              |

En todas: sin atascos de más de 2 s, nadie dentro de un sólido y ningún disparo a través
de paredes.

## Parámetros (`game::MatchConfig`)

| Grupo | Parámetros |
|---|---|
| Autobús y caída | `startInBus`, `busPosition`, `busSeconds`, `freefallSpeed`, `freefallFallSpeed`, `glideSpeed`, `glideFallSpeed`, `glideAutoHeight` |
| Botín | `loot`, `chestCount`, `floorLootCount`, `ammoBoxCount`, `airdropTimes`, `airdropFallSpeed` |
| Bots | `botsStartArmed`, `botDamageScale`, `hearingRange`, `huntRange`, `botsBuild`, `botWallCooldown`, `BuildConfig::maxBotWalls` / `maxWallsPerBot` |
| Puntería | `aimAssistDegrees`, `weaponSpread`; en app.cpp, `kLockAngleDegrees` y `kLockDistance` |

`MatchConfig::classic()` conserva las reglas de las fases 4-6 (sin autobús ni botín, con
la SCAR) para las pruebas de regresión.

## Limitaciones

- **Ala delta:** no se puede volver a abrir una vez en el suelo; el autobús no se mueve
  (como se pidió).
- **Bots:** no usan consumibles ni escudo.
- **Mapa sin botín:** la meseta y los altillos no tienen; se reparte sobre el suelo
  transitable.
- **Duración:** las partidas son cortas (1-1,5 min con el jugador quieto), porque los bots
  aterrizan cerca del centro y se buscan. Se ajusta con `huntRange`, `hearingRange`,
  `botDamageScale` y la probabilidad de caza (`match_bots.cpp`).
