# Pavos y tienda de objetos (v0.13)

## Pavos

Moneda del juego (los "V-Bucks" de Fortnite; en inglés se muestran como *V-Bucks*). Se
guardan en `perfil.txt` (`pavos=N`) y se ven siempre en el lobby, arriba a la derecha.

| Cómo se consiguen | Pavos | Cuándo |
| --- | --- | --- |
| Victoria magistral | 50 | Al terminar la partida ganada (sale en la pantalla de victoria) |
| Logro completado | 50 | Se **recogen** a mano: LOGROS → elegir el logro → X |

- **Recompensas por recoger:** en LOGROS, los completados sin recoger tienen la moneda
  resaltada y la cabecera dice cuántos quedan; en la barra de secciones, un punto amarillo
  junto a LOGROS lo recuerda. Cada logro se recoge una sola vez (`cobrado_<logro>=1`).
- **Retrocompatible:** un `perfil.txt` de antes de la v0.13 no tiene la línea `pavos`. Al
  cargarlo se pagan las victorias ya ganadas (50 por cada una) y todos los logros que ya
  estaban completados quedan **por recoger**. Nada se pierde ni se cobra dos veces: desde
  el primer guardado el fichero ya lleva `pavos=` y los `cobrado_`.

## Tienda (sección TIENDA del lobby)

Venden los trajes de los personajes de la CPU (los mismos que llevan los bots), **poco
comunes** y a **800 pavos** cada uno:

| Traje | Clave en `perfil.txt` | Paleta |
| --- | --- | --- |
| Comando Sombra | `traje_comando` | `game::palettes::kCommando` (negro y camuflaje gris) |
| Mono Naranja | `traje_mono` | `game::palettes::kOrange` |
| Guardabosques | `traje_guardabosques` | `game::palettes::kRanger` (rojo y azul con casco) |

- **Arriba/abajo** elige; el personaje del lobby muestra el traje elegido.
- **X** compra: pide confirmación (precio y lo que quedaría; X compra, CÍRCULO cancela).
  Sin pavos suficientes avisa y no abre la confirmación.
- Si el traje ya es tuyo, **X lo equipa** (también se puede equipar en la TAQUILLA, donde
  solo aparece lo que se tiene).
- Lo comprado se guarda al momento (`comprado_<clave>=1`). Un traje de pago equipado en un
  perfil que no lo ha comprado (fichero editado a mano) vuelve al de por defecto.

## Añadir un objeto a la tienda

Una fila en `kCatalog` (`app/locker.cpp`) con precio mayor que 0; con precio 0 se tiene
desde el principio. La tienda (`shopItems`) y la taquilla (`App::lockerItems`, que filtra
con `Profile::owns`) lo recogen solos. En memoria lo comprado es un bit por índice del
catálogo (hasta 32 objetos); en `perfil.txt` va la clave, así que las filas se pueden
reordenar, pero **las claves no se cambian nunca**.
