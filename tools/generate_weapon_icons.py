#!/usr/bin/env python3
"""Recorta las siluetas de las armas para los iconos del HUD (Recursos/HUD/Armas/*.png).

Uso (desde la raiz del proyecto):  python3 tools/generate_weapon_icons.py

Fuentes (fotos de las armas que dio el usuario):
  fusil          Referencias/Armas/images.jpeg (SCAR)           (fondo dorado: se quita)
  escopeta       Referencias/Armas/pump-shotgun-...webp         (fondo azul: se quita)
  subfusil       Referencias/Armas/fortnite-suppressed-smg...   (fondo azul: se quita)
  francotirador  Referencias/Armas/Icon_Weapons_Sniper_01_L.webp (ya transparente)

Se guardan a 64 px con transparencia; tools/prepare_assets.py las pone sobre el fondo
de cada rareza (gris, verde, azul, morado, dorado) en ranuras de 32x32.
"""

import colorsys
from pathlib import Path

from PIL import Image, ImageFilter

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "Recursos" / "HUD" / "Armas"
SIZE = 64


def key_background(img, is_background):
    """Pone transparentes los pixeles del fondo (funcion de (h, s, v) en 0..1)."""
    img = img.convert("RGBA")
    px = img.load()
    for y in range(img.height):
        for x in range(img.width):
            r, g, b, a = px[x, y]
            h, s, v = colorsys.rgb_to_hsv(r / 255, g / 255, b / 255)
            if a < 40 or is_background(h * 360, s, v):
                px[x, y] = (0, 0, 0, 0)
    return img


def finish(img, margin):
    w, h = img.size
    img = img.crop((round(w * margin), round(h * margin), round(w * (1 - margin)), round(h * (1 - margin))))
    alpha = img.getchannel("A").point(lambda v: 255 if v > 90 else 0)
    # Apertura (erosion + dilatacion): quita destellos y motas del fondo que no son el arma.
    alpha = alpha.filter(ImageFilter.MinFilter(3)).filter(ImageFilter.MaxFilter(3))
    img.putalpha(alpha)
    bbox = alpha.getbbox()
    if bbox:
        img = img.crop(bbox)
    img.thumbnail((SIZE, SIZE), Image.LANCZOS)
    out = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    out.alpha_composite(img, ((SIZE - img.width) // 2, (SIZE - img.height) // 2))
    out.putalpha(out.getchannel("A").point(lambda v: 255 if v > 110 else 0))
    return out


def gold(h, s, v):
    return 25 <= h <= 60 and s > 0.55 and v > 0.55


def blue(h, s, v):
    return 185 <= h <= 255 and ((s > 0.35 and v > 0.5) or v > 0.85)  # no el acero pavonado (oscuro)


ICONS = {
    "fusil": (ROOT / "Referencias" / "Armas" / "images.jpeg", gold, 0.0),
    "escopeta": (ROOT / "Referencias" / "Armas" / "pump-shotgun-fortnite-ftr_1uyjreuyqvygl1fz5rvaorr20k.png.webp", blue, 0.0),
    "subfusil": (ROOT / "Referencias" / "Armas" / "fortnite-suppressed-smg.jpg.webp", blue, 0.0),
    "francotirador": (ROOT / "Referencias" / "Armas" / "Icon_Weapons_Sniper_01_L.webp", lambda h, s, v: False, 0.0),
}


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    for name, (source, background, margin) in ICONS.items():
        img = Image.open(source)
        img.thumbnail((400, 400), Image.LANCZOS)  # rapido y suficiente para 64 px
        icon = finish(key_background(img, background), margin)
        icon.save(OUT / f"{name}.png")
        print(f"  Recursos/HUD/Armas/{name}.png  {icon.width}x{icon.height}")


if __name__ == "__main__":
    main()
