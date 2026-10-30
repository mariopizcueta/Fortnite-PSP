# Equipos: dúo y escuadrón (v0.16)

Submodos del **Battle Royale**. En el lobby: **CUADRADO** → fila **EQUIPOS** → ←/→
(**SOLO / DÚO / ESCUADRÓN**). Monodisparo y Supervivencia zombi no tienen equipos.

| Submodo | Personajes | Equipos | Tus compañeros |
|---|---|---|---|
| Solo | 1 + hasta 30 bots | cada uno por su cuenta | ninguno |
| Dúo | 30 (tú + 29 bots) | 15 parejas | 1 bot |
| Escuadrón | 40 (tú + 39 bots) | 10 equipos de 4 | 3 bots |

Los bots del escuadrón son **39 y no 40** para que todos los equipos queden completos: con 40
sobraría uno solo en un equipo (`kSquadBots`, en `game/match_config.h`). Dúo y escuadrón
cuentan siempre para pavos y logros (en la fila de los bots ya no se elige el número).

## Reglas

- **Sin fuego amigo:** las balas atraviesan a los compañeros; explosiones y trampas tampoco
  les hacen daño.
- **Se gana en equipo:** la partida acaba cuando queda un equipo. Si gana un compañero tuyo,
  ganas tú aunque hayas caído. El puesto es el del equipo.
- **Autobús:** tus compañeros saltan cuando saltas tú y caen a tu lado. Los equipos de bots
  saltan juntos y aterrizan en el mismo sitio.

## Derribado

Quien se queda sin vida con un compañero en pie **no muere**: cae a cuatro patas.

- Se arrastra (26 % de la velocidad). No dispara, no construye, no usa objetos ni recoge nada.
- Pierde vida sola: **30 s** la primera vez, 20 la segunda, 15 la tercera…
  (`MatchConfig::downSeconds`). Los rivales también pueden rematarle.
- Si se acaba esa vida, **muere del todo** y suelta sus cosas (como en Fortnite).
- Si el último en pie de un equipo cae, los derribados caen con él: el equipo queda eliminado.
- Caído del todo, mientras quede un compañero se le sigue mirando (y se puede ganar). Cuando
  no queda ninguno llega la pantalla de derrota.

## Reanimar

- **CUADRADO mantenido** junto a un compañero derribado, **10 s**. Soltar corta la reanimación
  y hay que empezar de cero. Mientras se atiende, ni se mueve quien reanima ni el derribado
  pierde vida.
- Se levanta con **30 de vida** y sin escudo.
- Los bots acuden a un compañero derribado a menos de 45 m si no tienen un rival a menos de
  7 m. A ti también te levantan.

## HUD

- **Arriba a la izquierda**, en pequeño: nombre, escudo y vida de cada compañero (en rojo si
  está derribado; la barra amarilla es la reanimación).
- Una **cruz azul con su nombre** sobre su cabeza (roja si está derribado) y un punto en el
  minimapa y en el mapa.
- Si te derriban: **DERRIBADO**, los segundos que te quedan y una barra.

## IA de equipo

- Cada bot sigue a su líder: el primero en pie del equipo y, en el tuyo, tú. Si se aleja más
  de 22 m (`teamLeash`) vuelve con él. Sin arma va a buscarla adonde haga falta.
- Prefieren disparar al que sigue en pie antes que a un derribado.

## Código

- `game/match_teams.cpp`: reparto, derribar, desangrarse, reanimar y la IA de equipo.
- `Match::eliminate` (`game/match.cpp`): caer con el equipo, puesto y victoria.
- `game/match_bots.cpp`: estados `BotState::Downed` y `BotState::Revive`, seguir al líder.
- `BodyPose::Downed` (`game/character_model.cpp`): la postura a cuatro patas.
- `app/app_hud.cpp`: barras, marcas y lo de estar derribado (compartido con los aliados).
