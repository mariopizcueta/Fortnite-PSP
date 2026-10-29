# Actualización v0.14.0

Cinco cambios pedidos, con lo que se ha hecho, cómo se ha comprobado (pruebas de host, no en
la PSP) y lo que queda por mirar en la consola.

## 1. 30 bots por defecto y partidas de práctica

- Las partidas empiezan con **30 bots** (`game::kDefaultBots`). En el panel del lobby
  (CUADRADO) se pueden bajar de 5 en 5 hasta 5.
- Con **menos de 30** (`game::kRewardBots`) la partida es **de práctica**: no da pavos ni
  cuenta para nada del perfil (partidas, victorias, bajas ni ningún logro). El lobby lo
  avisa en rojo ("PRÁCTICA (menos de 30 bots)"), el panel también y los resultados dicen
  "Partida de práctica: sin pavos ni logros".
- El modo zombi no tiene bots: siempre cuenta (con sus propias recompensas).

## 2. Bots más inteligentes

- **Acampan los airdrops:** armados y con 60 o más de vida + escudo, a veces se apostan a
  10-14 m de un airdrop sin abrir (cayendo o ya en el suelo), en un sitio desde el que lo
  ven y mejor junto a algo que les cubra; lo vigilan (el arma lista, mirando a un lado y a
  otro) y disparan a quien vaya a por él. Como mucho 3 por airdrop. Si en 30 s nadie viene,
  lo abren ellos. Estado nuevo `BotState::Camp`.
- **Estructuras pequeñas:**
  - **Caja de cuatro paredes:** al recibir disparos desde lejos (o huyendo), si tienen 40
    o más de material, se encierran: primero la pared hacia quien dispara, luego los lados
    y detrás (una cada 0,2 s). A los 5-9 s quitan sus paredes para salir (si no se están
    curando ni peleando). Nunca disparan a sus propias paredes.
  - **Rampa para ganar altura:** si el rival está más alto (tejado, contenedores, una
    rampa) a 5-28 m, ponen una rampa hacia él y suben por ella; arriba se quedan 7 s (no
    bajan solos como al caer en un tejado).
  - Límites: 6 piezas por bot y 40 entre todos (antes 3 y 24).
- **Final de partida** (8 vivos o menos en una partida que era grande, o desde el quinto
  cierre de la tormenta):
  - rotan antes (45 s antes del cierre en lugar de 25);
  - en vez de cazar por el mapa (20 % de las veces en lugar del 45 %), buscan un sitio
    dentro de la zona siguiente junto a algo que cubra, lo protegen con una caja y esperan
    8-14 s vigilando;
  - más rampas en los tiroteos a media distancia;
  - huyen antes con poca vida (60 en lugar de 45).
- En las partidas simuladas de 30 bots: 2-8 acampadas, 3-6 cajas, 3-6 rampas y 2-5
  posiciones de final de partida por partida.

## 3. Animaciones y gráficos

- **Texturas del mapa a 64×64** (antes 32×32), con el mismo tamaño en el mundo: el doble de
  detalle y más colores (vetas, tornillos, nudos, grietas, briznas, flores, líquenes,
  juntas, placas de los contenedores…). 24 texturas: las 16 de siempre redibujadas y 8 del
  modo zombi (`tools/generate_textures.py`).
- **Cielo:** cúpula con degradado (azul arriba, del color de la niebla en el horizonte), nubes
  y el sol en la dirección de la luz. En el modo zombi, encapotado.
- **Personajes:**
  - anillos de 10 lados en el modelo de cerca (antes 8): cabeza, brazos y piernas más
    redondos;
  - **mochila** con tapa, bolsillo y saco de dormir enrollado, y sus correas (es lo que se
    ve desde la cámara);
  - cuello de la camiseta, bolsas del cinturón, pulgares y nudillos;
  - 1750 triángulos el de cerca (antes 1220) y 548 el de lejos (antes 492). Solo los que
    están a menos de la distancia de detalle de los ajustes (10 m por defecto) usan el de
    cerca.
- **Animaciones:**
  - el tronco gira con cada zancada (hombros contra caderas) y se balancea de lado al andar;
  - en reposo, cambia el peso de pierna;
  - se inclina hacia las curvas al girar corriendo;
  - brazada más natural (el codo se dobla más al ir el brazo hacia delante);
  - al caer eliminado, a veces de bruces (y con un giro), y se le doblan las rodillas;
  - en el agua levanta las rodillas;
  - los zombis: encorvados, cabeceando, cojeando, con los brazos al frente y el zarpazo.
- **Modelos 3D:** zombis (dos aspectos y el bruto, más grande), plano enrollado, y la base,
  los campamentos y los centros de crafteo del modo zombi.

## 4. Andar por el agua

- El lago ya no bloquea el paso: se cruza andando a **la mitad de velocidad** (y se salta
  menos). La superficie se dibuja 35 cm por encima del fondo, así que se ve a los personajes
  con los pies en el agua, y las orillas tienen su canto.
- Los pasos suenan a **chapoteo** (los propios y los de los bots cercanos).
- Los bots también lo cruzan. El botín nunca aparece en el agua; lo que cae al lago flota.

## 5. Modo Supervivencia zombi

Todo en [ZOMBIS.md](ZOMBIS.md): mapa en ruinas y más tenebroso, base abandonada donde se
despierta del coma, día y noche con hordas, tres tipos de zombi, supervivientes a los que dar
un arma y una cura para que se unan, centros de crafteo (en el botín solo hay escopetas,
pistolas y curas), planos para fortificar la base, avisos, minimapa, logros y pavos.

## Sonidos nuevos (sintetizados)

`zombie_gemido`, `zombie_golpe`, `horda` (cuerno del anochecer), `chapoteo`, `fabricar` y
`amanecer` (`tools/generate_sounds.py`). Si se deja en `Recursos/Sonidos/Sonidos Limpios/`
un fichero con el mismo nombre, sustituye al sintetizado (`make assets && make`).

## Comprobado (host, no PSP)

- `make match-sim`: sección 20 nueva (caja de paredes y salir de ella, rampa contra un rival
  en lo alto, acampar un airdrop, partidas reales de 30 bots con las tácticas, bots que
  cruzan el lago, el mapa en ruinas, supervivientes, centros de crafteo, botín del modo,
  aparición de zombis, anochecer y horda, zombis que golpean y rompen paredes,
  supervivientes que se unen y siguen, sin fuego amigo, crafteo, planos, dos días y dos
  noches seguidos, y vuelta al mapa normal). **0 fallos en 231 comprobaciones.**
- `make app-sim`: sección 11 nueva (partida de práctica sin pavos ni logros, panel de modo,
  despertar, pistola junto al catre, rescate con CUADRADO, menú de crafteo, pieza de un plano,
  agua, anochecer, amanecer, resultados con pavos y logro, vuelta al battle royale) y las de
  siempre con 30 bots. **0 fallos en 178 comprobaciones.** Capturas `50*`–`61*` en
  `build/app_sim/`.
- `make physics-test`: 0 fallos. `make preview`: vista `16_zombis` nueva.

## Pendiente de comprobar en la PSP

- **Rendimiento con 30 bots** ahora que es lo normal, con los personajes más detallados
  (1750 triángulos de cerca), las texturas de 64×64 (8 KB cada una: caben justo en la caché
  de texturas de la GE) y el cielo.
- Modo zombi: 22 zombis + 5 supervivientes con la noche (la viñeta es una textura a pantalla
  completa con mezcla).
- Que la noche no quede demasiado oscura en la pantalla de la PSP-2000 (se ajusta en
  `App::renderNightOverlay` y `App::updateZombieFog`).
- Volumen de los sonidos sintetizados nuevos.
