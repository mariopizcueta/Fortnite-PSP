#!/usr/bin/env python3
"""Dibuja los iconos originales del HUD que no estan en Recursos/HUD (pixel art).

Uso (desde la raiz del proyecto):  python3 tools/generate_hud.py

Completan los de Recursos/HUD (Pico, Pared, Rampa, Suelo, Victory Royale) para el HUD
de Referencias/HUD: ranura del arma (SCAR), materiales, vivos, bajas, zona, escudo y
vida. Se dibujan aqui a mano, pixel a pixel, con formas simples.
"""

from pathlib import Path

from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "Recursos" / "HUD"

GOLD = (232, 176, 56, 255)
GOLD_DARK = (150, 104, 30, 255)
DARK = (40, 38, 36, 255)
WHITE = (250, 250, 250, 255)
OUTLINE = (20, 20, 24, 255)


def canvas(w, h):
    return Image.new("RGBA", (w, h), (0, 0, 0, 0))


def person():
    img = canvas(14, 14)
    d = ImageDraw.Draw(img)
    d.ellipse((0, 0, 13, 13), fill=(20, 20, 24, 230))
    d.ellipse((5, 2, 8, 5), fill=WHITE)
    d.rectangle((3, 7, 10, 11), fill=WHITE)
    return img


def kills():
    img = canvas(14, 14)
    d = ImageDraw.Draw(img)
    d.ellipse((1, 1, 12, 12), fill=(20, 20, 24, 230), outline=WHITE)
    d.line((6, 0, 6, 13), fill=WHITE)
    d.line((0, 6, 13, 6), fill=WHITE)
    d.rectangle((5, 5, 7, 7), fill=(230, 70, 60, 255))
    return img


def zone():
    img = canvas(14, 14)
    d = ImageDraw.Draw(img)
    d.ellipse((0, 0, 13, 13), fill=(70, 160, 230, 255), outline=(200, 236, 255, 255))
    d.line((6, 3, 6, 7), fill=WHITE)
    d.line((6, 7, 9, 9), fill=WHITE)
    return img


def shield():
    img = canvas(12, 12)
    d = ImageDraw.Draw(img)
    d.polygon([(1, 1), (10, 1), (10, 6), (5, 11), (1, 6)], fill=WHITE, outline=(120, 120, 120, 255))
    return img


def health():
    img = canvas(12, 12)
    d = ImageDraw.Draw(img)
    d.rectangle((4, 0, 7, 11), fill=WHITE)
    d.rectangle((0, 4, 11, 7), fill=WHITE)
    return img


# Los materiales, la SCAR y los consumibles ya no se generan: estan en Recursos (Wood_icon,
# stone_orig, Metal_icon, Weapons/AssaultRiffleLegendary, minipocion, Vendas.webp,
# botiquin.jpg, "escudo grande.jpg"), dados por el usuario.
ICONS = {
    "icono_vivos": person, "icono_bajas": kills, "icono_zona": zone, "icono_escudo": shield,
    "icono_vida": health,
}


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    for name, fn in ICONS.items():
        img = fn()
        img.save(OUT / f"{name}.png")
        print(f"  Recursos/HUD/{name}.png  {img.width}x{img.height}")


if __name__ == "__main__":
    main()
