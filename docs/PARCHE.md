# Parche de mecánicas y contenido (fase 8)

Rama `fase-8`, punto de recuperación: etiqueta `pre-parche-8` (= v0.11.0, la última versión
probada en la PSP). Este documento recoge la auditoría previa y, bloque a bloque, lo que se
ha hecho, cómo se ha comprobado y lo que falta.

## 1. Auditoría

### Sistemas existentes

| Sistema | Dónde | Estado antes del parche |
| --- | --- | --- |
| Armas | `game/weapon.*`, `game/inventory.cpp` (`weaponConfig`) | 4 armas × rareza (fusil/SCAR, escopeta de corredera, subfusil con silenciador, francotirador de cerrojo), disparo instantáneo por rayo, dispersión de cadera/apuntando, caída del daño. Sin retroceso de juego (solo el gesto de la animación). |
| Inventario | `game/inventory.*` | 5 ranuras + pico, munición común por tipo, materiales. Al estar lleno se cambia por la ranura seleccionada; **con un consumible en la mano CUADRADO no hacía nada** (no se podía recoger ni cambiar). Un arma soltada volvía con el cargador lleno (duplicaba munición). |
| Curaciones | `Match::updatePlayerItems` | R empieza a usar; se termina tras `ItemDef::useSeconds`. Se podía andar y saltar curándose. Recibir daño no interrumpe (como Fortnite). |
| Construcción | `game/structures.*`, `app/build_visuals.*` | Pared, suelo y rampa; madera/piedra/metal; cajas dinámicas compartidas con física, IA y disparos. Sin trampas. |
| IA | `game/match_bots.cpp` | 30 bots máx., estados Botín/Explorar/Desplazarse/Buscar/Atacar, inventario de 5 ranuras, curas, paredes de cobertura. |
| Botín | `game/match_loot.cpp` | Cofres, cajas de munición, objetos del suelo, airdrops. Los bots eliminados sueltan su inventario y munición, **no materiales**. Sin llama. |
| Animación | `game/character_model.*` | 9 piezas articuladas (FK), paso con fase por distancia, apuntar con dos manos solo con L/disparando; **al correr el arma va en una mano y el brazo se balancea**; el arma está pegada al antebrazo (una malla de antebrazo por arma × paleta × LOD). |
| Cámara / mirilla | `camera/third_person_camera.*`, `App::sniperScoped` | **Al pulsar L sin objetivo la cámara se recoloca detrás del jugador**: con el francotirador la mirilla aparecía mirando hacia donde miraba el personaje, no hacia donde estaba la mira. Además el ojo pasa de 5 m detrás a la cabeza sin corregir el paralaje. |
| Agacharse | `Match` | Más lento y caja de impacto baja; los bots fallan más. **No mejoraba la precisión del jugador.** |
| Sonido | `audio/*`, `App::processEvents` | 27 efectos + música. **Las cajas de munición y los airdrops usaban el sonido del cofre.** |
| Lobby | `app/app_menus.cpp`, `app/lobby_scene.*` | JUGAR (con nº de bots) / CONTROLES / SALIR, estadísticas del perfil. Sin secciones, sin ajustes. |
| Persistencia | `app/profile.*` | `perfil.txt` (clave=valor) junto al EBOOT: nombre y estadísticas. Escritura directa (un apagón a mitad lo podía dejar vacío). |
| Textos | Repartidos en `app_menus.cpp`, `app_hud.cpp`, `inventory.cpp`, `structures.cpp`… | Solo español, sin sistema centralizado. Rótulos grandes (JUGAR, CONTROLES…) dibujados en el atlas con la fuente Impact. |
| Botones de PSP | `tools/prepare_assets.py::button` | Iconos de `Recursos/Texturas PSP` con un **halo de 1 px añadido por el script** (el "contorno"). |
| Pruebas de host | `tools/match_sim`, `tools/app_sim`, `tools/physics_test`, `tools/scene_preview` | Todas en verde al empezar (0 fallos). |

### Recursos encontrados

Nuevos desde la v0.11.0 (sin versionar todavía):

- `Recursos/Weapons/`: `Assault_rifle_icon.png` (fusil de asalto genérico tipo M16),
  `Burst_Assault_Rifle…webp` (fusil de ráfagas), `Scoped_Assault_Rifle…webp` (fusil con
  mira), `Submachine_Gun_(v1.6)…webp` (subfusil), `Tactical_Shotgun.webp` (escopeta táctica),
  `pistola.png`, `revolver.jpeg` (fondo blanco, sin transparencia), `Icon_Weapons_SK_Sniper_02_L.webp`
  (francotirador semiautomático), `Bazooka…webp` (lanzacohetes), `Icon_Weapons_SK_Grenade_Launcher_L.webp`
  (lanzagranadas), `Granada.png` (granada arrojadiza).
- `Recursos/HUD/spiketrap.webp`: icono de la trampa de pinchos.
- `Referencias/FortniteFloorTrap.jpg.avif`: trampa de pinchos de suelo (marco con franjas
  amarillas, placas y 16 pinchos).
- `Referencias/Supply_Llama_-_Container_-_Fortnite.webp`: llama de suministros.
- `Referencias/locker.jpg`: taquilla de Fortnite (pestañas arriba, rejilla de casillas por
  categoría, vista previa a la derecha con "rareza | tipo" y nombre).

### Recursos que faltan

- **Modelos 3D:** no hay ninguno en el proyecto (todo es geometría procedural). Las armas
  nuevas, la llama y la trampa se modelan en código a partir de los iconos y referencias.
- **Sonidos:** no hay sonido de caja de munición, de llama, de trampa, de explosión ni de
  las armas nuevas. Siguiendo el criterio de la fase 6 (sonidos originales sintetizados en
  `tools/generate_sounds.py`), se generan sonidos propios; nunca se reutiliza el del cofre.
- **Skins:** no hay recursos de skins; la taquilla queda preparada y vacía salvo el traje
  por defecto.
- **Fuente:** la fuente de texto es la 8×8 de `pspdebug` (ASCII): los textos se escriben
  sin tildes ni eñes en los dos idiomas.

### Archivos que hay que modificar

`game/inventory.*`, `game/weapon.*`, `game/match*.{h,cpp}`, `game/match_config.h`,
`game/character_model.*`, `game/structures.*`, `camera/third_person_camera.*`,
`app/app*.{h,cpp}`, `app/prop_visuals.*`, `app/build_visuals.*`, `app/profile.*`,
`app/ui.*`, `input/input.*`, `audio/*`, `assets/*`, `tools/prepare_assets.py`,
`tools/generate_sounds.py`, `tools/match_sim`, `tools/app_sim`, `Makefile`, documentación.
Nuevos: modelos de objetos (`game/item_models.*`), proyectiles/explosiones, trampas,
traducciones (`core/i18n.*`), ajustes, logros y taquilla.

### Dependencias entre funcionalidades

- Los modelos de armas separados del antebrazo (D) los necesitan las armas nuevas (B): se
  crean en B y la animación de D los reutiliza.
- Las armas explosivas (B) necesitan proyectiles y explosiones, que también dañan paredes,
  objetos del mapa, trampas y la llama.
- Las trampas (C) dependen de la construcción y de los pickups de B (se encuentran como
  botín).
- Los logros (E) se alimentan de los sucesos de la partida de A–C y se guardan con la
  persistencia, que F amplía (ajustes, borrado de datos).
- El idioma (F) toca todos los textos de A–E: se hace al final, como pide el orden.

### Orden recomendado

El del encargo: A → B → C → D → E → F, compilando y pasando las pruebas de host al final de
cada bloque.

## 2. Bloque A: inventario, curaciones y combate (v0.12.0-a)

### Sustituir objetos (también curativos)

- **Causa del fallo anterior:** con un consumible en la mano, CUADRADO no se enviaba a la
  partida (solo con el pico o un arma), así que no se podía recoger ni cambiar nada.
  Ahora CUADRADO abre/recoge lleve lo que lleve en la mano (`App::updateGame`).
- **Selector de qué soltar:** si el objeto no cabe (`Match::pickupRoom() == 0`), CUADRADO
  abre un selector sobre la fila de objetos: la cruceta izquierda/derecha elige la ranura
  (empieza en la de la mano), CUADRADO confirma y CÍRCULO cancela (sin entrar en
  construcción). Mientras está abierto no se dispara ni se usa nada. Se cierra solo si el
  objeto deja de estar al alcance.
- `Match::swapPickup(pickup, slot)`: coloca el objeto en esa ranura (`Inventory::place`) y
  deja lo que había en el suelo (`Match::dropSlot`), donde estaba lo recogido. Se puede
  volver a recoger.
- **Sin duplicar munición:** `Pickup::loaded` guarda las balas del cargador de un arma
  soltada (el jugador y los bots); al recogerla vuelve con esas balas, no con el cargador
  lleno.
- `interact()` sin selector (bots, pruebas) mantiene el cambio por la ranura de la mano.

### Curarse sin moverse

- Mientras se usa un consumible (`Match::useProgress() >= 0`): sin desplazamiento, sin
  salto, sin agacharse/levantarse, sin recoger y sin bailar. La cámara gira con normalidad.
- Cambiar de ranura, sacar el pico o entrar en construcción cancelan el uso **sin gastar**
  el objeto (como antes).
- Recibir daño no lo interrumpe (como en Fortnite y como ya funcionaba); se puede activar
  con `MatchConfig::interruptUseOnDamage`.
- Duración configurable por objeto: `MatchConfig::useSeconds[ItemType]` (0 = el valor de
  `ItemDef`); la usan el jugador y los bots.

### Francotirador

- **Causa:** al pulsar L sin objetivo fijado la cámara se recolocaba detrás del jugador
  (`recenter`), así que la mirilla aparecía mirando hacia donde miraba el cuerpo. Además el
  ojo pasa de estar 5 m detrás y 1.2 m por encima a la cabeza (paralaje).
- **Arreglo:** con la mirilla no hay recolocación. Al ponerla (`App::updateScopeTransition`)
  se calcula el punto bajo la mira de tercera persona (`Match::aimPoint`, el mismo cálculo
  del disparo: mundo, paredes y personajes) y la cámara se orienta desde la cabeza hacia
  ese punto; al quitarla, `ThirdPersonCamera::aimAt` orienta la cámara de tercera persona
  hacia el punto de la mirilla. El disparo sigue saliendo del arma (comprobación de
  cobertura desde el pecho), así que no atraviesa muros que la cámara ve por encima.
- La ayuda de puntería con la mirilla baja de 4 a 2 grados: con el zoom (15 grados de
  campo) 4 grados desviaban el impacto un cuarto de pantalla del centro de la retícula.
- A menos de ~1 m de distancia horizontal la mirilla (que sale de la cabeza) no puede
  apuntar exactamente al mismo punto que la cámara; se deja la orientación como estaba.

### Agacharse y retroceso

- `WeaponConfig::crouchSpreadScale`: fusil 0.6, subfusil 0.7, escopeta 0.85, francotirador
  0.5 (el resto de armas se configura en el bloque B). `MatchConfig::crouchAccuracyBonus`
  (0–1) escala la bonificación.
- Retroceso nuevo (`WeaponConfig::recoil`, `maxRecoil`, `recoilRecovery`): cada disparo
  abre la dispersión, que se recupera 0.25 s después del último disparo. Agacharse reduce
  la dispersión del arma pero **no** el retroceso. La cámara sube un poco con cada disparo
  y vuelve sola.
- La mira del HUD se abre con la dispersión real del siguiente disparo
  (`Match::playerSpreadNow`, la misma función que usa el disparo).

### Comprobado (host, no PSP)

`make match-sim` (prueba 16), `make app-sim` (pruebas 5c y 5d con el mando simulado:
mirilla horizontal con la cámara girada 50 grados, hacia arriba, hacia abajo y frente a una
pared a 4 m; selector con curativo en la mano; curarse con el stick y X pulsados),
`make physics-test`, `make preview`: 0 fallos. Capturas: `build/app_sim/30b_selector_cambio.png`,
`26e_mirilla_pared_cerca.png`, `30c_curandose_quieto.png`. Falta probarlo en la PSP.
