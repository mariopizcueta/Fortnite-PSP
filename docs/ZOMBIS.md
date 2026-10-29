# Supervivencia zombi (v0.14)

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
| Día | 150 s | 7 + 2 por día | normal | 16 m |
| Noche | 95 s | 13 + 2 por día | ×1,3 | 26 m |

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

En el botín del modo solo hay **escopetas** (de corredera y táctica), **pistolas** (y
revólver) y **curativos**; ni granadas ni trampas. Los materiales salen del pico y de los
zombis.

## Planos de la base

4 planos repartidos por el mapa (rollo azul con haz de luz). Al recogerlos aparecen sus
**piezas fantasma azules** en la base (y en el mapa grande). Junto a una, **CUADRADO** la
construye por 10 de material (el elegido para construir o el que haya). Son **reforzadas**
(el doble de vida) y no cuentan en tu límite de piezas.

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

- **Pavos:** 25 por noche sobrevivida y 15 por plano completado.
- **Logros nuevos:** Primera noche, Tres noches, Una semana (récord de noches seguidas),
  Cazazombis (100 zombis), Rescatador (5 supervivientes), Fortaleza (4 planos), Artesano
  (10 objetos fabricados).
- En `perfil.txt`: `zombis_record_noches`, `zombis_abatidos`, `supervivientes`, `planos` y
  `fabricados`. Las bajas de zombis no cuentan como las del battle royale.

## Código

- `game/zombies.h/.cpp`: tipos de zombi, recetas, planos y `ZombieConfig`.
- `game/match_zombies.cpp`: día y noche, aparición, IA de zombis y supervivientes,
  crafteo, planos y el estado del mapa (`Match::applyMapState`).
- `world/zombie_sites.h`: la base, campamentos, centros de crafteo y planos; los objetos,
  en `world_layout.cpp` (`zombieSites`).
- `app/app_zombies.cpp`: HUD, avisos, menú de crafteo, noche, minimapa en ruinas, cielo y
  resultados.
- `MatchConfig::zombies()`: las reglas del modo.
