# Inventario, escudo, curas, pico y materiales (fase 7, paso 1)

## Inventario

`game/inventory.*`. Tiene 5 ranuras, como en Fortnite, y el pico va aparte.

- **Armas:** cada una ocupa una ranura y guarda su propio `Weapon` (cargador, reserva y
  recarga de la fase 5, sin cambios). Al cambiar de ranura se interrumpe la recarga, pero
  las balas se conservan.
- **Consumibles:** se apilan hasta su máximo.
- **Selección:** la cruceta ← → pasa a la siguiente o anterior ranura que tenga algo; las
  vacías se saltan.
- **Pico:** TRIÁNGULO lo saca y lo guarda. La cruceta también lo guarda.
- **Inventario inicial:** `MatchConfig::startItems` y `startMaterials`. Hasta que exista el
  botín (paso 2), se empieza con:
  - SCAR;
  - 5 vendas;
  - 3 minipociones;
  - 30 de madera (3 paredes).
- **Botín (preparado):** `Match::giveItem()` y `giveMaterials()` ya permiten dar objetos;
  el paso 2 los usará al abrir cofres y recoger del suelo.

| Objeto           | Rareza     | Pila | Uso    | Efecto                          |
|------------------|------------|------|--------|---------------------------------|
| SCAR             | Legendaria | 1    | —      | Fusil de la fase 5              |
| Vendas           | Común      | 15   | 3,5 s  | +15 de vida, hasta 75           |
| Botiquín         | Poco común | 3    | 10 s   | Vida al máximo (100)            |
| Minipoción       | Poco común | 6    | 2 s    | +25 de escudo, hasta 50         |
| Poción de escudo | Rara       | 3    | 5 s    | +50 de escudo, hasta 100        |

El botiquín y la poción de escudo ya están definidos, con su icono, pero solo se
conseguirán con el botín.

## Escudo y curas

- **Escudo:** `CharacterState::shield` (máximo 100). `Match::applyDamage` lo descuenta
  primero y el resto pasa a la vida. Los bots no tienen escudo: su sistema no se ha
  tocado.
- **Usar un consumible:** con él en la mano, R empieza a usarlo. Se termina tras su
  tiempo de uso.
  - **Durante el uso:** se puede andar, y recibir daño no lo interrumpe.
  - **Interrupciones:** cambiar de ranura, sacar el pico o entrar en construcción lo
    cancelan, y no se gasta.
  - **Si no tendría efecto:** no se empieza y el HUD avisa "Vida al máximo" o "Escudo al
    máximo" (por ejemplo, vendas con 75 de vida o más).
- **HUD:** la barra de escudo es azul y muestra el valor real. Durante el uso aparece
  bajo la mira "Usando Minipocion 1.2 s" con una barra de progreso. Al terminar suena la
  curación.

## Pico y materiales

**El golpe** (`Match::swingPickaxe`):
- **Ritmo:** con el pico en la mano, mantener R golpea cada 0,5 s.
- **Alcance:** el golpe sale del pecho en la dirección de la mira, con un alcance de
  2,2 m.
- **Troncos finos:** si el rayo no toca nada, vale el objeto recolectable que esté justo
  delante, sin obstáculos en medio.
- **Daño:**

| Objetivo             | Efecto                                                  |
|----------------------|---------------------------------------------------------|
| Bot                  | 20 de daño (número de daño y marcador como un disparo)  |
| Pared construida     | 50 de daño                                              |
| Árbol                | +8 de madera por golpe (48 en total)                    |
| Caja de madera       | +6 de madera (30)                                       |
| Roca                 | +7 de piedra (49)                                       |
| Contenedor, remolque | +8 de metal (64)                                        |
| Contenedor de basura | +6 de metal (30)                                        |

**Objetos recolectables:** hay 153 en el mapa (116 árboles, 13 rocas, 6 de metal y las
cajas).
- **Registro:** se apuntan al construir el escenario (`world/harvest.h`,
  `world_layout.cpp`) con el índice de su caja de colisión. El raycast ya devuelve ese
  índice, así que no hubo que cambiar el sistema de colisiones.
- **Lo que queda:** lo lleva la partida y se repone en cada una. Un objeto agotado deja
  de dar material, pero **sigue en su sitio** (ver limitaciones).

**Materiales:** madera, piedra y metal, hasta 999 de cada uno.
- **Coste:** una pared cuesta **10 de madera** (`BuildConfig::wallCost`). Sin madera, la
  pared fantasma sale roja con "Falta madera (10 por pared)".
- **Piedra y metal:** se acumulan para cuando se pueda elegir el material de
  construcción.

**HUD y sonido:**
- **Materiales:** los contadores bajo el minimapa son reales. Los iconos de madera, ladrillo
  y metal son los de `Recursos/HUD/` (`Wood_icon`, `stone_orig`, `Metal_icon`).
- **Ranuras:** la SCAR y la minipoción se dibujan con sus ranuras de `Recursos`
  (`Weapons/AssaultRiffleLegendary.png`, `HUD/minipocion.png`), que ya traen el fondo de
  rareza.
- **Al picar:** cada golpe con material muestra su icono y "+8" junto a la mira.
- **Sonido:** el golpe suena con `pico.wav`, un sonido original nuevo.

**Modelo del personaje:** el jugador lleva en la mano lo que tiene seleccionado:
- el rifle;
- el pico, con la cabeza azulada de `Recursos/HUD/Pico.png`;
- o la mano vacía con un consumible.

Al golpear, el brazo baja desde encima de la cabeza.

## Verificación en el Mac

- **`make match-sim` (sección 12):**
  - **Inventario:** el inventario inicial y que la cruceta cambia de ranura.
  - **Consumibles:** la minipoción (2 s, +25 de escudo, gasta una unidad); el escudo
    absorbe antes que la vida; las vendas con la vida alta se rechazan; cambiar de ranura
    cancela sin gastar; las vendas dan +15 en 3,5 s.
  - **Materiales:** el coste de 10 de madera y el aviso sin madera.
  - **Pico:** 3 golpes por segundo y 8 de madera por golpe. Un árbol se agota en 48. La
    roca da piedra y un bot pierde 20 de vida. La cruceta vuelve del pico a una ranura.
  - **Bots:** las partidas completas de 10 y 30 bots siguen terminando igual.
- **`make app-sim` (sección 5b), con los botones reales:** cruceta, R con vendas (aviso
  de vida llena) y con minipoción (escudo y sonido), TRIÁNGULO con el pico frente a un
  árbol (madera y sonido), cámara que sigue al andar, L + stick para mover la mira sin
  andar, cruceta en construcción y la cámara libre en la página de depuración.

## Limitaciones de este paso

- **Objetos agotados:** no desaparecen (ni árboles ni cajas). Hacerlo exige dibujarlos
  por separado y poder desactivar su caja de colisión; se puede abordar más adelante.
- **Copas de los árboles:** no tienen colisión, tampoco para la cámara. Junto a un árbol
  de copa cuadrada, la cámara puede quedar dentro de las hojas.
- **Suelo y rampa:** se pueden elegir con la cruceta, pero aún no se colocan (paso de
  construcción).
- **Los bots** no usan pico, escudo ni consumibles. No se ha tocado su IA.
