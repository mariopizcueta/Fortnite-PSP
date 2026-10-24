# Logros (parche, fase 8)

Sección **LOGROS** del lobby (L / R cambian de sección; arriba / abajo recorren la lista).
Cada logro muestra nombre, descripción, progreso (`3 / 10`) y, si está conseguido, la fecha.

## Cómo funcionan (`app/achievements.*`, `app/profile.*`)

- Cada logro es un **contador del perfil** (`app::Stat`) y una **meta**. Los contadores
  suben por sucesos reales de la partida (`App::processEvents`): eliminaciones (y con qué
  arma: trampa o explosivo, que la partida anota en el suceso `Eliminated`), disparos a la
  cabeza, cofres, suministros y llamas abiertos, piezas construidas, materiales conseguidos
  con el pico y curativos usados; al terminar la partida, victorias, partidas, top 5 y la
  racha de bajas.
- Al llegar a la meta se **desbloquea una sola vez** (`Profile::unlockReached`): sale un
  aviso arriba ("LOGRO DESBLOQUEADO"), suena y **se guarda en el momento**. Un logro
  desbloqueado no se vuelve a registrar.
- Se guarda en `perfil.txt` con el resto del perfil (el sistema que ya existía):
  `logro_<clave>=AAAAMMDD` y los contadores (`cofres=`, `piezas=`...). El guardado es
  seguro (se escribe `perfil.tmp` y se cambia por `perfil.txt`; si la consola se apaga a
  mitad, al cargar se usa el temporal) y al cargar se valida todo.

## Lista inicial

| Logro | Condición |
| --- | --- |
| Victoria Magistral / Imparable | Ganar 1 / 5 partidas |
| Primera sangre / Cazador / Exterminador | 1 / 25 / 100 eliminaciones |
| Puntería | 10 disparos a la cabeza |
| En racha | 5 eliminaciones en una partida |
| Saqueador / Buscatesoros | Abrir 10 / 50 cofres |
| Llama a la vista | Abrir o romper una llama |
| Correo aéreo | Abrir un suministro aéreo |
| Arquitecto | Construir 100 piezas |
| Leñador | 500 materiales con el pico |
| Trampero | Eliminar con una trampa |
| Demoledor | Eliminar con un explosivo |
| Entre los mejores | Terminar entre los 5 primeros |
| Veterano | Jugar 20 partidas |
| Botiquín andante | Usar 25 curativos |

## Añadir un logro

1. Un valor en `AchievementId` y su fila en `kAchievements` (`achievements.cpp`): clave,
   nombre, descripción, contador y meta.
2. Si necesita un contador nuevo: un valor en `Stat`, su campo en `Profile` (con su clave
   en `kFields` de `profile.cpp`) y un `addStat()` donde ocurra el suceso.
