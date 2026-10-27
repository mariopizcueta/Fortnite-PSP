# Taquilla (parche, fase 8)

Sección **TAQUILLA** del lobby, según `Referencias/locker.jpg`: rejilla de categorías
(traje, mochila, pico, ala delta, baile, envoltorio), vista previa del personaje y ficha
del objeto ("rareza | categoría" y nombre). Cruceta para elegir la categoría, X para
entrar, arriba/abajo para elegir objeto, X para equiparlo, CÍRCULO para volver.

Están los objetos **por defecto** (traje "Recluta", pico, ala delta y baile) y, desde la
v0.13, los trajes de los personajes de la CPU que se hayan comprado en la **TIENDA** con
pavos ([TIENDA.md](TIENDA.md)); solo sale lo que se tiene. Mochila y envoltorio están
vacías y lo indican ("aún no hay objetos; llegarán en una actualización").

Lo equipado se guarda en `perfil.txt` (`equipado_<categoría>=<clave>`); si una clave no
existe en el catálogo (o no es de esa categoría) se usa el objeto por defecto.

## Añadir una skin (traje)

1. Su paleta en `game/character_model.cpp` (`namespace palettes`), declarada en
   `character_model.h`: colores de piel, pelo, camiseta, chaleco, pantalón, botas,
   guantes... y si lleva pañuelo, casco o camuflaje. El modelo es el mismo; la paleta lo
   tiñe.
2. Una fila en `kCatalog` (`app/locker.cpp`):
   `{"traje_clave", "Nombre", CosmeticSlot::Outfit, game::Rarity::Epic, &game::palettes::kMiSkin}`.
3. Nada más: aparece en la categoría Traje, se puede equipar y el jugador se dibuja con
   ella en el lobby y en la partida (`App::init` construye el modelo del jugador con el
   traje equipado y la taquilla lo reconstruye al cambiarlo).

Otras categorías: una fila en `kCatalog` con su `CosmeticSlot`. Hoy el juego no aplica
picos, alas o bailes distintos (solo existe el de por defecto); al añadir uno hay que leer
`Profile::equippedIn(...)` donde se dibuja ese objeto (`game::ItemModels` para el pico,
`PropVisuals::drawGlider` para el ala delta, `BodyPose::Dance` para el baile).
