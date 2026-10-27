# Ajustes e idioma (parche, fase 8)

## Menú de ajustes

**START en el lobby** lo abre y lo cierra (también CÍRCULO). Es modal: mientras está
abierto el lobby no recibe ningún botón, así que **L / R cambian de sección del menú** y no
de sección del lobby. ↑ ↓ eligen la opción, ← → cambian su valor y X activa (reasignar,
restaurar, idioma, borrar). Todo se aplica al momento y se guarda al cerrar.

| Sección | Opción | Efecto real |
| --- | --- | --- |
| Gráficos | Distancia de dibujado (cerca / media / lejos) | La niebla (y con ella lo que se dibuja) acaba a 70 / 95 / 125 m |
| | Detalle de personajes (bajo / medio / alto) | Los bots usan el modelo sencillo a más de 6 / 10 / 18 m |
| | Números de daño, trazadoras, contador de FPS | Se ven o no |
| | Sincronía vertical | VSync del cambio de buffer |
| Sensibilidad | Cámara (1–10), mirilla (1–10) | Velocidad de giro con el stick al apuntar y con la mirilla |
| | Zona muerta del stick (10–35 %) | Cuánto hay que mover el stick para que cuente |
| | Invertir eje vertical | Arriba/abajo de la mira |
| Controles | Saltar, construir, recargar/recoger/material, pico, disparar/usar/colocar, apuntar, agacharse, bailar | X espera el nuevo botón (START cancela). Solo X, O, □, △, L, R, ↑, ↓; si otro lo tenía, se **intercambian** (nunca dos acciones en el mismo). "Restaurar predeterminados". Fijos: cruceta ←/→, SELECT, START, L+SELECT y L+R+START |
| Idioma | Español / English | Toda la interfaz al momento (textos y rótulos) |
| Datos | Borrar datos del jugador | Ver abajo |
| Admin (v0.13) | Contraseña / Abrir MOD MENU / Bloquear | Con la contraseña se desbloquea el menú de trucos; ver [ACTUALIZACION_0_13.md](ACTUALIZACION_0_13.md#10-menú-de-administrador-mod-menu). Se guarda `admin=1` y `truco_<clave>=1` |

Ninguna opción necesita reiniciar la partida (el menú solo se abre en el lobby). Todas las
ayudas de botones de la partida, la página de controles y los consejos de carga muestran el
botón asignado a cada acción.

## Persistencia

`ajustes.txt` junto al EBOOT (`ms0:/PSP/GAME/FortnitePSP/`), en "clave=valor". Guardado
seguro (se escribe `ajustes.tmp` y se cambia por el bueno). Al cargar, cada valor fuera de su
rango se descarta (queda el de por defecto); botones repetidos o desconocidos restauran los
controles por defecto; si no hay fichero o está roto, todo por defecto.

## Borrar datos del jugador

No hay cuentas ni servidores: los datos son locales. Se borran **solo** `perfil.txt` (y su
temporal `perfil.tmp`) de la carpeta del juego: nombre, estadísticas, logros y lo equipado en
la taquilla. No se tocan los ajustes, el ejecutable, los recursos ni ningún otro fichero.

Pide confirmación con lo que se va a borrar (X borra, CÍRCULO cancela). Si falla (Memory
Stick protegida o retirada) se avisa y el perfil sigue intacto. Al terminar, el juego vuelve a
pedir el nombre como la primera vez.

## Idioma (`core/i18n.*`)

Todos los textos visibles están en `core/i18n_texts.h`, cada uno con su versión en español
e inglés lado a lado; el código usa identificadores (`i18n::T`) y los traduce al dibujar. Los
rótulos grandes del atlas (JUGAR/PLAY, CONTROLES/CONTROLS, SALIR/EXIT, CARGANDO/LOADING,
MODO CONSTRUCCIÓN/BUILD MODE, ELIMINADO/ELIMINATED, el nombre del mapa, las pestañas y
AJUSTES/SETTINGS) se generan en los dos idiomas en `tools/prepare_assets.py`. La fuente de
texto es ASCII: sin tildes en ningún idioma. `make app-sim` comprueba que ningún texto se sale
de la pantalla, que los textos estrechos caben en su hueco y que los dos idiomas tienen los
mismos formatos (`%d`, `%s`), y guarda capturas en inglés (`35*`).
