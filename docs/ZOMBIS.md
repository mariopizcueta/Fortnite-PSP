# Supervivencia zombi (v0.14, ampliado en v0.15 y v0.16)

Lo nuevo de la v0.15 (lugares de interés, hambre y sueño, camas, partida guardada, aliados,
mochilas, objetivos y miedo) está resumido en [ACTUALIZACION_0_15.md](ACTUALIZACION_0_15.md) y
explicado aquí en su sección.

Modo nuevo del lobby (**CUADRADO** → modo → **SUPERVIVENCIA ZOMBI**). No hay autobús, ni
tormenta que se cierre, ni bots del battle royale: solo tú, los zombis y unos pocos
supervivientes en un Polvorín Polvoriento en ruinas. El objetivo es aguantar el mayor número
de noches.

## El mapa en ruinas

- **Más tenebroso:** niebla gris verdosa de día (se ve hasta ~70 m), casi negra de noche
  (~34 m), cielo encapotado, penumbra verdosa y una viñeta que se cierra de noche.
- **Texturas oxidadas** (tema "en ruinas", `world::TextureTheme::Ruined`): chapa y tejados
  con óxido y agujeros, contenedores oxidados, hierba seca, agua turbia, hormigón agrietado
  con musgo y madera podrida (`Recursos/Texturas/16..23`).
- **Estructuras casi derruidas:** al empezar, parte de los objetos del mapa ya están en el
  suelo (paneles y tejados de las naves 38 %, vallas y farolas 45 %, cajas 35 %, muro de
  bloques 25 %…; `Harvestable::ruinChance`). Siempre los mismos: el mapa en ruinas es fijo.
- **Bases abandonadas** (solo existen en este modo, `Harvestable::zombieOnly`):
  - **La base** (x 56..68, z 28..40, al sureste del depósito): catre, tienda, hoguera,
    bidones oxidados, mesa con el mapa de los planos, cajas y sacos terreros. Aquí
    despiertas.
  - **5 campamentos de supervivientes** (bosque, sur del lago, sur, este y suroeste): tienda,
    hoguera, sacos terreros y una caja. Los árboles del bosque dejan un claro alrededor.
  - **3 centros de crafteo** (dentro de la nave gris, al oeste del lago y al pie de la
    meseta): banco de trabajo con tornillo, panel de herramientas y tejadillo de chapa.

## El despertar

La pantalla se abre desde negro con dos parpadeos ("...¿dónde estoy?", "Despiertas de un
coma en una base abandonada", "Busca armas, gente y planos. Sobrevive."). Los primeros 2 s
no te puedes mover. Junto al catre hay una **pistola**, **vendas** y munición ligera, y en
la base un cofre.

## Día y noche

| | Duración | Zombis a la vez | Velocidad | Vista de los zombis |
|---|---|---|---|---|
| Día | 300 s (v0.15) | 7 + 2 por día | normal | 16 m |
| Noche | 180 s (v0.15) | 13 + 2 por día | ×1,3 | 26 m |

- Al **anochecer** suena un cuerno, aparece el aviso "ANOCHECE: viene una horda" y llega
  una **horda** (6 + 2 por día) desde ~45 m que va directa a por ti. Desde la segunda noche
  la encabeza un **bruto**.
- Al **amanecer** suena una campana y empieza el día siguiente: cuenta como **noche
  sobrevivida**.
- Arriba a la izquierda: día o noche, tiempo hasta el cambio y una barra; debajo, los
  objetivos (supervivientes, planos y centros de crafteo encontrados).
- Como máximo hay 22 zombis a la vez (`kZombieSlots`); aparecen a 30-58 m, mejor donde no
  los veas, nunca dentro de la base ni en el agua.

## Zombis

| Tipo | Vida | Velocidad (día/noche) | Golpe | A las piezas | Cuándo |
|---|---|---|---|---|---|
| Zombi | 100 | 42 % / 56 % | 14 | 40 | siempre |
| Corredor | 65 | 78 % / 96 % | 9 | 25 | de noche, y de día desde el día 2 |
| Bruto (×1,3 de tamaño) | 420 | 36 % / 46 % | 32 | 120 | desde la noche 2 (y de día desde el día 3) |

(De noche golpean un 20 % más fuerte.)

- Te notan a 5 m aunque no te vean; a su distancia de vista si te ven; al que persiguen lo
  siguen más lejos y van a donde te vieron por última vez.
- Acuden a los **disparos** (34 m de día, 48 de noche). De noche, sin nadie a la vista,
  tiran hacia ti.
- No golpean a través de las paredes: si hay una pieza construida entre tú y ellos, **la
  rompen a golpes** (el bruto, en tres golpes una de madera).
- Gimen de vez en cuando (más persiguiendo). Encorvados, con los brazos al frente y
  cojeando; el golpe es un zarpazo.
- Al caer a veces sueltan **chatarra** (metal, madera o piedra), munición de escopeta o
  ligera, o vendas: son el sustento del modo.
- Las cabezas valen doble, como siempre.

## Supervivientes

- Uno en cada campamento, agachado junto a la hoguera. Al pasar a 22 m aparece el aviso
  "X necesita ayuda" y sale en el minimapa (amarillo).
- **Necesita un arma y una curación.** Con la cosa en la mano, **CUADRADO** junto a él se
  la da (de un montón de curas, hasta 3). El texto de la ayuda dice lo que le falta.
- Con las dos se **une a ti** (verde en el minimapa): te sigue (cada uno en su hueco detrás
  de ti), dispara a los zombis que vea (28 m, con munición
  infinita) y se cura con lo suyo.
- Sin fuego amigo: tus balas, explosiones y trampas no les hacen nada (ni a ellos ni a ti
  las suyas).
- Si un zombi acaba con uno, aviso "X ha caído" y suelta lo que llevaba.

## Centros de crafteo

**CUADRADO** junto al banco abre el menú (la partida sigue: ¡cuidado!). ↑↓ eligen, **X**
fabrica, **O** cierra. Lo fabricado va al inventario (si no cabe, al suelo delante del
banco); las armas traen un cargador de reserva.

| Receta | Madera | Piedra | Metal |
|---|---|---|---|
| Fusil de asalto (raro) | 40 | | 60 |
| Subfusil (poco común) | 30 | | 45 |
| Francotirador (raro) | | 40 | 90 |
| Lanzacohetes (épico) | | 80 | 150 |
| Granadas ×3 | | 30 | 30 |
| Trampas de pinchos ×2 | 30 | | 20 |
| Vendas ×5 | 30 | | |
| Botiquín | 40 | 20 | |
| Munición ×2 (del arma en la mano) | | | 15 |
| Cama (v0.15) | 50 | | 15 |

En el botín del modo solo hay **escopetas** (de corredera y táctica), **pistolas** (y
revólver) y **curativos**; ni granadas ni trampas. Los materiales salen del pico y de los
zombis.

## Planos de la base

4 planos repartidos por el mapa (rollo azul con haz de luz). Al recogerlos aparecen sus
**piezas fantasma azules** en la base (y en el mapa grande). Junto a una, **CUADRADO** la
construye por 10 de material (el elegido para construir o el que haya). Son **reforzadas**
(el doble de vida). v0.15: en este modo no hay límite de piezas (solo el técnico, 128).

| Plano | Piezas |
|---|---|
| Muralla norte y oeste | 6 paredes |
| Muralla este y sur | 5 paredes (la puerta sur, x 60..64, queda abierta) |
| Torre de vigilancia | rampa, plataforma a 3 m y dos parapetos |
| Refugio techado | 4 suelos de tejado sobre la base |

Si los zombis rompen una pieza, su fantasma vuelve a aparecer y se puede reconstruir.

## Fin y recompensas

La partida acaba cuando caes: resultados con las **noches sobrevividas**, zombis abatidos,
rescatados, planos completados y objetos fabricados, y el récord.

- **Pavos:** 25 por noche sobrevivida, 15 por plano completado y 10 por objetivo cumplido.
- Al caer (o al abandonar desde la pausa) la partida guardada se borra.
- **Logros nuevos:** Primera noche, Tres noches, Una semana (récord de noches seguidas),
  Cazazombis (100 zombis), Rescatador (5 supervivientes), Fortaleza (4 planos), Artesano
  (10 objetos fabricados).
- En `perfil.txt`: `zombis_record_noches`, `zombis_abatidos`, `supervivientes`, `planos` y
  `fabricados`. Las bajas de zombis no cuentan como las del battle royale.

## v0.15: lugares de interés

| Lugar | Dónde | Guardianes | Contenedores |
|---|---|---|---|
| La Casona | x 39..53, z 47..59 | 6 (+ bruto desde el día 2) | 2 neveras, despensa, 2 cofres, munición |
| Almacén del Polvorín | x -2..18, z -74..-58 | 7 (+ bruto desde el día 2) | 3 despensas, 2 cajas de munición, 2 cofres, nevera |
| Casa del guarda | x 12..20, z 29..37 | 2 | nevera, cofre |
| Casa del lago | x -20..-12, z 74..82 | 2 | nevera, munición |
| Granja abandonada | x 68..76, z 12..20 | 3 | nevera, despensa |
| Cabaña del bosque | x -60..-52, z 20..28 | 2 | nevera, despensa |
| Puestos norte, este y del bosque | (34, -80), (78, 56), (-66, -56) | 3 | munición y despensa o cofre |
| Gasolinera | tienda x 48..56, z -20..-15 | 3 | nevera de bebidas, despensa, munición |

- Los guardianes salen al acercarse (a 10 m del borde del lugar), una vez al día, y van
  ganando uno cada dos días. Hasta un 40 % están **tumbados entre las ruinas** como
  cadáveres: se levantan si te acercas a 5,5 m o si les haces daño.
- Al entrar en un lugar se descubre (su nombre en pantalla y una marca en el mapa).
- Al amanecer, un 35 % de los cofres, cajas, neveras y despensas abiertos se rellenan.

## v0.15: hambre y sueño

- **Hambre** (barra naranja): empieza en 80 y se vacía en ~720 s (un día y medio). Por
  debajo de 25, aviso; a 0, se pierden 0,7 de vida por segundo. **Lata** +30, **ración** +65
  (y +10 de vida). Sin hambre no se come.
- **Sueño** (barra morada): empieza en 75 y se vacía en ~1440 s (tres días). Por debajo de
  33 se falla: más dispersión, de vez en cuando tropiezas, se te escapa un tiro o se te caen
  balas, microsueños, susurros y figuras que no están. A 0, se pierden 0,5 de vida por
  segundo.
- **Dormir:** R con el **saco de dormir** (se empieza con él) o CUADRADO junto a una
  **cama** colocada. No con un zombi a menos de 18 m; uno a 11 m o un golpe te despierta.
  Durmiendo el tiempo pasa ×8. La cama llena el sueño en media noche (90 s de reloj) y el
  saco en una noche (180 s). CÍRCULO o X despiertan.
- Hasta 8 camas. De noche, con el jugador en la base (30 m), cada aliado con cama libre se
  acuesta: fuera de guardia, los zombis no le atacan. Se levanta al amanecer o si el jugador
  se va.

## v0.15: aliados

- **CUADRADO** junto a un aliado: su inventario (arriba el tuyo, abajo el suyo). X le da un
  arma, curativo o granada, o se lo quita (las armas, con un cargador de su munición).
- **En la base, de día:** levantan una pieza cada 6 s. Las de los planos encontrados (con tu
  material; reforzadas) y, aunque no tengas planos, las del **muro del perímetro** (las de los
  planos de las murallas: sin material, con lo que rapiñan y el triple de lento, sin
  reforzar). Reconstruyen lo que rompen los zombis.
- **Si caen**, a los 4 s se levantan como **zombi armado** (170 de vida, con sus armas y su
  aspecto) que dispara a todos. Abatido, suelta su inventario y una **mochila**.

## v0.16: aliados inteligentes

- **125 de vida y hasta 125 de escudo.** Sus barras salen arriba a la izquierda.
- **Qué hacen, por orden:**
  1. De noche, si tienen **cama asignada**, a dormir pase lo que pase.
  2. Con poca vida (vida + escudo por debajo de 70): huyen hacia ti, se cubren con lo que
     construyen y se curan. Solo pelean si el zombi está encima.
  3. Si te alejas más de 15 m lo dejan todo y van contigo, disparando de camino.
  4. Si ven un zombi, pelean sin perseguirlo.
  5. Si estás en lo alto de una rampa construida, suben detrás.
  6. Si no: fabrican, recogen botín, fortifican su cama y levantan la base.
- **Botín:** armas (solo si son mejores o les falta alguna; sueltan la que sustituyen), curas,
  mochilas y materiales, a 12 m. Si van cortos de material pican lo que tengan cerca, nunca lo
  de la base ni lo de los lugares. Abren cofres, pero no neveras ni despensas: la comida es
  para ti.
- **Crafteo:** con un centro de crafteo a 14 m y material propio, se fabrican un arma mejor
  que la mejor que llevan o, si no tienen curas, curas. Traen 30 de madera al unirse.
- **Camas:** **L** junto a una cama abre la lista de aliados. Sin asignar no duermen en ella,
  y tú no puedes dormir en la de un aliado. Salen con tiempo para llegar al anochecer; si a
  los 30 s de noche no han llegado, se acuestan directamente. Durmiendo no reciben daño. Se
  levantan al amanecer.
- **Refugio:** de día, con el jugador a menos de 30 m, levantan junto a su cama tres paredes y
  un techo en la celda de la rejilla donde está. El lado desde el que colocaste la cama queda
  libre para entrar. Pagan con su material, luego con el tuyo y, sin nada, con lo que rapiñan
  (el triple de lento).

Sustituye a lo que decía la v0.15 sobre las camas: ya no se reparten solas.

## v0.15: mochilas y botín

- Los **brutos** y los aliados caídos sueltan una **mochila**: +3 ranuras (hasta 11).
- Las **cajas de munición** dan los 5 tipos (medio paquete de cada uno).
- Comida: neveras (latas y a veces una ración), despensas (latas, raciones, vendas o
  munición), cofres (30 %) y algún zombi (5 %).

## v0.15: objetivos

Dos a la vez, distintos: abatir zombis (8 + 2 por día), corredores, un bruto (desde el día
2), registrar 3 neveras o despensas, comer 2 veces, fabricar 2 objetos, construir 8 piezas,
ir a un lugar (sin descubrir si queda alguno), picar 150 de material, dormir hasta descansar
y tiros a la cabeza. Cumplido: recompensa (materiales, munición de todo, comida o curas) y
10 pavos; a los 4 s llega otro.

## v0.15: partida guardada

- **Pausa:** GUARDAR PARTIDA, GUARDAR Y SALIR (al lobby, sin que cuente como terminada) y
  VOLVER AL LOBBY (abandonar: cuenta y borra la guardada). También se guarda **al amanecer**.
- **Lobby:** CUADRADO (modo) > fila de abajo: ← → entre "Continuar: día N" y "Partida
  nueva".
- `partida_zombis.txt` (junto al EBOOT, escrito en un temporal y renombrado): semilla, día y
  hora, jugador, inventario, munición, materiales, planos, lugares, objetivos, piezas,
  supervivientes con su inventario, lo abierto, camas, objetos del suelo y lo derruido. Los
  zombis no se guardan: al seguir, el mapa se vuelve a poblar.

## v0.15: miedo

- Más oscuro (penumbra y noche) y una viñeta que se cierra con el sueño.
- **Ojos rojos** de los zombis de noche, sin niebla: se ven antes que ellos.
- **Latido** (y bordes rojos) con un zombi a menos de 14 m (un bruto, 28) o con poca vida.
- **Gritos lejanos** de noche cada 22-55 s.
- Zombis **tumbados** que se levantan con un golpe de sonido; los aliados caídos vuelven.
- Con sueño: susurros, una figura al borde de la vista que desaparece y microsueños.

## Código

- `game/zombies.h/.cpp`: tipos de zombi, recetas, planos, objetivos y `ZombieConfig`.
- `game/zombie_save.cpp` (v0.15): la partida guardada.
- `game/match_zombies.cpp`: día y noche, aparición, IA de zombis, crafteo, planos y el
  estado del mapa (`Match::applyMapState`).
- `game/match_allies.cpp` (v0.16): la IA de los aliados, las camas asignadas y sus refugios.
- `world/zombie_sites.h`: la base, campamentos, centros de crafteo y planos; los objetos,
  en `world_layout.cpp` (`zombieSites`).
- `app/app_zombies.cpp`: HUD, avisos, menú de crafteo, noche, minimapa en ruinas, cielo y
  resultados.
- `MatchConfig::zombies()`: las reglas del modo.
