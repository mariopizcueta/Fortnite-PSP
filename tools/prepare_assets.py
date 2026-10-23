#!/usr/bin/env python3
"""Genera los assets listos para PSP a partir de la carpeta Recursos/.

Uso (desde la raiz del proyecto):
    python3 tools/prepare_assets.py      # o: make assets

Requiere Pillow. Solo hace falta ejecutarlo cuando cambian los recursos
originales; los ficheros generados en assets/ se versionan junto al codigo,
asi que la compilacion normal NO necesita Python.

Recursos/   -> recursos directos del juego (se convierten y se usan tal cual).
Referencias/ -> solo material visual de referencia, nunca se empaqueta.
"""

from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parent.parent
SRC_LOADSCREEN = ROOT / "Recursos" / "Loadscreen.png"
SRC_LOGO = ROOT / "Recursos" / "fortnite_PNG25.png"
LOGO_WIDTH = 256  # ancho maximo de textura comodo para la GE (potencia de 2)

SCREEN_SIZE = (480, 272)  # resolucion nativa de la PSP
ICON0_SIZE = (144, 80)    # icono del XMB


def cover(img: Image.Image, size: tuple[int, int]) -> Image.Image:
    """Escala para cubrir `size` manteniendo proporcion y recorta centrado."""
    tw, th = size
    scale = max(tw / img.width, th / img.height)
    w, h = round(img.width * scale), round(img.height * scale)
    img = img.resize((w, h), Image.LANCZOS)
    left, top = (w - tw) // 2, (h - th) // 2
    return img.crop((left, top, left + tw, top + th))


def save(img: Image.Image, path: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    img.save(path, "PNG", optimize=True)
    print(f"  {path.relative_to(ROOT)}  {img.width}x{img.height}  {path.stat().st_size} bytes")


def main() -> None:
    src = Image.open(SRC_LOADSCREEN).convert("RGB")
    print(f"Fuente: {SRC_LOADSCREEN.relative_to(ROOT)} {src.width}x{src.height}")

    loadscreen = cover(src, SCREEN_SIZE)
    # Imagen que el juego incrusta en el ejecutable (pantalla inicial).
    save(loadscreen, ROOT / "assets" / "images" / "loadscreen.png")
    # Fondo que muestra el XMB al seleccionar el juego.
    save(loadscreen, ROOT / "assets" / "pbp" / "PIC1.PNG")
    # Icono del XMB: zona central de la ilustracion (personaje y globo).
    w, h = src.size
    crop = src.crop((int(w * 0.22), int(h * 0.02), int(w * 0.78), int(h * 0.98)))
    save(cover(crop, ICON0_SIZE), ROOT / "assets" / "pbp" / "ICON0.PNG")

    # Logotipo con transparencia para la portada: se recorta al texto + su halo y se
    # escala a 256 px de ancho (RGBA, se dibuja con mezcla alfa).
    logo = Image.open(SRC_LOGO).convert("RGBA")
    print(f"Fuente: {SRC_LOGO.relative_to(ROOT)} {logo.width}x{logo.height}")
    bbox = logo.getchannel("A").point(lambda v: 255 if v > 24 else 0).getbbox()
    logo = logo.crop(bbox)
    height = round(logo.height * LOGO_WIDTH / logo.width)
    save(logo.resize((LOGO_WIDTH, height), Image.LANCZOS), ROOT / "assets" / "images" / "logo.png")


if __name__ == "__main__":
    main()
