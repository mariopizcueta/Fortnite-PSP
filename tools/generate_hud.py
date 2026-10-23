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


def scar():
    # Fusil de asalto de perfil (canon a la derecha), dorado como la SCAR legendaria.
    img = canvas(48, 24)
    d = ImageDraw.Draw(img)
    d.rectangle((10, 8, 36, 13), fill=GOLD, outline=OUTLINE)       # cuerpo
    d.rectangle((36, 9, 46, 11), fill=DARK, outline=OUTLINE)       # canon
    d.rectangle((2, 9, 10, 15), fill=GOLD_DARK, outline=OUTLINE)   # culata
    d.rectangle((16, 13, 20, 20), fill=DARK, outline=OUTLINE)      # cargador
    d.rectangle((24, 13, 27, 18), fill=GOLD_DARK, outline=OUTLINE) # empunadura
    d.rectangle((18, 5, 30, 7), fill=DARK, outline=OUTLINE)        # mira
    return img


def wood():
    img = canvas(16, 16)
    d = ImageDraw.Draw(img)
    for i, y in enumerate((3, 7, 11)):
        d.rectangle((1 + i, y, 14 - i, y + 3), fill=(176, 120, 64, 255), outline=(90, 56, 26, 255))
    return img


def brick():
    img = canvas(16, 16)
    d = ImageDraw.Draw(img)
    d.rectangle((1, 5, 14, 12), fill=(196, 104, 72, 255), outline=(96, 44, 30, 255))
    d.line((1, 8, 14, 8), fill=(96, 44, 30, 255))
    d.line((7, 5, 7, 8), fill=(96, 44, 30, 255))
    d.line((10, 8, 10, 12), fill=(96, 44, 30, 255))
    return img


def metal():
    img = canvas(16, 16)
    d = ImageDraw.Draw(img)
    d.polygon([(2, 11), (10, 11), (14, 5), (6, 5)], fill=(170, 176, 190, 255), outline=(70, 74, 86, 255))
    d.line((6, 8, 12, 8), fill=(220, 224, 236, 255))
    return img


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


SHIELD_BLUE = (70, 150, 235, 255)
SHIELD_LIGHT = (170, 220, 255, 255)
GLASS = (200, 220, 230, 255)


def bandage():
    # Rollo de venda: cilindro blanco con la tira colgando.
    img = canvas(24, 24)
    d = ImageDraw.Draw(img)
    d.rectangle((6, 6, 17, 17), fill=(236, 232, 220, 255), outline=OUTLINE)
    d.line((6, 10, 17, 10), fill=(200, 190, 170, 255))
    d.line((6, 14, 17, 14), fill=(200, 190, 170, 255))
    d.rectangle((15, 16, 19, 22), fill=(236, 232, 220, 255), outline=OUTLINE)
    return img


def medkit():
    # Botiquin: maletin blanco con cruz roja.
    img = canvas(24, 24)
    d = ImageDraw.Draw(img)
    d.rectangle((9, 3, 14, 6), outline=OUTLINE)
    d.rectangle((2, 6, 21, 20), fill=(240, 240, 240, 255), outline=OUTLINE)
    d.rectangle((10, 8, 13, 18), fill=(210, 40, 40, 255))
    d.rectangle((7, 11, 16, 15), fill=(210, 40, 40, 255))
    return img


def small_shield():
    # Minipocion: frasco pequeno y redondo de liquido azul.
    img = canvas(24, 24)
    d = ImageDraw.Draw(img)
    d.rectangle((10, 4, 13, 8), fill=GLASS, outline=OUTLINE)
    d.ellipse((5, 8, 18, 21), fill=SHIELD_BLUE, outline=OUTLINE)
    d.ellipse((8, 11, 11, 14), fill=SHIELD_LIGHT)
    return img


def shield_potion():
    # Pocion de escudo: botella alta de liquido azul.
    img = canvas(24, 24)
    d = ImageDraw.Draw(img)
    d.rectangle((9, 1, 14, 6), fill=GLASS, outline=OUTLINE)
    d.rectangle((5, 6, 18, 22), fill=SHIELD_BLUE, outline=OUTLINE)
    d.rectangle((7, 9, 9, 18), fill=SHIELD_LIGHT)
    return img


ICONS = {
    "scar": scar, "material_madera": wood, "material_ladrillo": brick, "material_metal": metal,
    "icono_vivos": person, "icono_bajas": kills, "icono_zona": zone, "icono_escudo": shield,
    "icono_vida": health,
    "vendas": bandage, "botiquin": medkit, "minipocion": small_shield, "pocion_escudo": shield_potion,
}


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    for name, fn in ICONS.items():
        img = fn()
        img.save(OUT / f"{name}.png")
        print(f"  Recursos/HUD/{name}.png  {img.width}x{img.height}")


if __name__ == "__main__":
    main()
