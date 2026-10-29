#!/usr/bin/env python3
"""Genera las texturas originales del juego en Recursos/Texturas/ (64x64, estilo PS1).

Uso (desde la raiz del proyecto):  python3 tools/generate_textures.py

Cada textura se dibuja por procedimiento (ruido determinista, nada copiado de ningun
juego) y despues se reduce a una paleta retro de pocos colores. El numero inicial del
nombre fija su posicion en el atlas que usa el juego (orden de world::TextureId); las
de metal, contenedor y persiana son grises porque el juego las tine con el color de
cada edificio. Se pueden retocar a mano: prepare_assets.py usa estos ficheros tal cual.

v0.14: 64x64 (antes 32x32) con el mismo tamano en el mundo, asi que cada texel mide la
mitad: mas detalle (vetas, tornillos, grietas, briznas) y mas colores por textura. Las
texturas 16..23 son las del modo zombi (Polvorin en ruinas): el juego las usa en lugar de
las normales con el tema "en ruinas" (world::TextureTheme::Ruined).
"""

import math
from pathlib import Path

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "Recursos" / "Texturas"
N = 64
YY, XX = np.mgrid[0:N, 0:N]


def hash2(x, y, seed):
    h = (np.asarray(x, dtype=np.int64) * 374761393 + np.asarray(y, dtype=np.int64) * 668265263 + seed * 2246822519) & 0xFFFFFFFF
    h = ((h ^ (h >> 13)) * 1274126177) & 0xFFFFFFFF
    return ((h ^ (h >> 16)) & 0xFFFF) / 65536.0


def white(seed):
    return hash2(XX, YY, seed)


def blotch(cells, seed):
    """Ruido de valor suave y repetible (tileable) con `cells` celdas por lado."""
    fx, fy = XX * cells / N, YY * cells / N
    x0, y0 = fx.astype(int), fy.astype(int)
    tx, ty = fx - x0, fy - y0
    tx, ty = tx * tx * (3 - 2 * tx), ty * ty * (3 - 2 * ty)
    g = lambda i, j: hash2(i % cells, j % cells, seed)
    a, b = g(x0, y0), g(x0 + 1, y0)
    c, d = g(x0, y0 + 1), g(x0 + 1, y0 + 1)
    return (a + (b - a) * tx) * (1 - ty) + (c + (d - c) * tx) * ty


def fbm(seed, octaves=(4, 8, 16), weights=(0.55, 0.30, 0.15)):
    return sum(w * blotch(c, seed + i * 7) for i, (c, w) in enumerate(zip(octaves, weights)))


def shade(base, s):
    """Imagen RGB a partir de un color base y un mapa de intensidad."""
    return np.stack([np.clip(base[i] * s, 0, 255) for i in range(3)], axis=-1).astype(float)


def mix(rgb, mask, color, amount=1.0):
    """Mezcla `color` sobre los texeles de `mask` (amount 0..1 o un mapa)."""
    c = np.array(color, dtype=float)
    m = (mask.astype(float) * amount)[..., None]
    return rgb * (1 - m) + c * m


def retro(rgb, colors):
    """Reduce a una paleta de `colors` colores (sin tramado: pixeles limpios)."""
    img = Image.fromarray(np.clip(rgb, 0, 255).astype(np.uint8))
    return img.quantize(colors=colors, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE).convert("RGB")


def cracks(seed, density=0.02, length=10):
    """Grietas finas: caminos aleatorios que serpentean (tileable)."""
    mask = np.zeros((N, N), dtype=bool)
    starts = int(N * N * density / length)
    for k in range(starts):
        x = int(hash2(k, 1, seed) * N)
        y = int(hash2(k, 2, seed) * N)
        dx, dy = (1, 0) if hash2(k, 3, seed) < 0.5 else (0, 1)
        for step in range(length):
            mask[y % N, x % N] = True
            r = hash2(k, step + 10, seed)
            if r < 0.3:
                x += dy
                y += dx
            elif r < 0.6:
                x -= dy
                y -= dx
            x += dx
            y += dy
    return mask


# --- Texturas normales ---------------------------------------------------------------------------

def grass():
    s = 0.78 + 0.36 * fbm(11) + 0.08 * (white(12) - 0.5)
    rgb = shade((72, 124, 48), s)
    rgb[..., 0] += 14 * blotch(3, 14)                     # toque amarillento en manchas
    blade = (white(13) < 0.10) & (hash2(XX, YY + 1, 13) < 0.5)
    rgb = mix(rgb, blade, (104, 160, 62), 0.8)            # briznas claras
    rgb = mix(rgb, np.roll(blade, 1, axis=0), (44, 82, 32), 0.6)  # su sombra
    clump = blotch(8, 15) > 0.72
    rgb = mix(rgb, clump, (52, 96, 36), 0.5)
    flower = white(16) < 0.006
    rgb = mix(rgb, flower, (236, 220, 120))
    rgb = mix(rgb, white(17) < 0.004, (240, 240, 230))
    return retro(rgb, 14)


def dirt():
    s = 0.80 + 0.30 * fbm(21) + 0.10 * (white(22) - 0.5)
    rgb = shade((138, 104, 70), s)
    pebble = (blotch(16, 23) > 0.80) & (white(24) < 0.7)
    rgb = mix(rgb, pebble, (176, 156, 128), 0.85)
    rgb = mix(rgb, np.roll(pebble, 1, axis=0) & ~pebble, (84, 62, 44), 0.7)
    rgb = mix(rgb, white(25) < 0.03, (70, 52, 36), 0.6)
    return retro(rgb, 12)


def concrete():
    s = 0.86 + 0.16 * fbm(31) + 0.06 * (white(32) - 0.5)
    rgb = shade((154, 152, 146), s)
    joint = (XX < 2) | (YY < 2)                            # juntas entre losas de 4 m
    rgb = mix(rgb, joint, (92, 90, 86), 0.8)
    rgb = mix(rgb, white(33) < 0.04, (120, 118, 112), 0.6)    # arido
    rgb = mix(rgb, cracks(34, 0.012, 9), (98, 96, 92), 0.7)
    stain = blotch(5, 35) > 0.74
    rgb = mix(rgb, stain, (118, 112, 100), 0.35)
    return retro(rgb, 12)


def metal_siding(rusty=False, seed=41):
    profile = np.array([0.66, 0.74, 0.86, 0.96, 1.0, 1.0, 0.98, 0.92, 0.86, 0.80, 0.74, 0.68, 0.62, 0.58, 0.58, 0.62])
    s = profile[XX % 16] + 0.05 * (white(seed) - 0.5)
    drip = np.clip((YY - 44) / 20.0, 0, 1) * blotch(16, seed + 1)[0][XX]   # suciedad que escurre
    s = s * (1 - 0.32 * drip)
    rgb = shade((236, 236, 236), s)
    rivet = ((XX % 16) == 8) & ((YY % 32) == 3)
    rgb = mix(rgb, rivet, (120, 120, 120))
    rgb = mix(rgb, np.roll(rivet, -1, axis=0), (250, 250, 250), 0.6)
    if rusty:
        r = fbm(seed + 3)
        rust = r > 0.52
        rgb = mix(rgb, rust, (150, 80, 44), np.clip((r - 0.52) * 5, 0.45, 0.95))
        streak = (blotch(16, seed + 4)[0][XX] > 0.6) & (YY > 8)
        rgb = mix(rgb, streak, (130, 70, 40), 0.35)
        hole = (fbm(seed + 5) > 0.74)
        rgb = mix(rgb, hole, (24, 20, 18))
        return retro(rgb, 16)
    return retro(rgb, 12)


def roof_panel(rusty=False, seed=51):
    s = np.where(XX % 32 < 5, 1.0, 0.80) + 0.06 * (white(seed) - 0.5)
    s = np.where(XX % 32 == 5, s * 0.7, s)
    s = np.where(YY % 32 == 0, s * 0.75, s)                # solape de chapas
    rgb = shade((222, 222, 216), s)
    rust = fbm(seed + 1) > (0.50 if rusty else 0.64)
    rgb = mix(rgb, rust, (176, 104, 70) if not rusty else (146, 76, 42), 0.55 if not rusty else 0.9)
    bolt = (XX % 32 == 2) & (YY % 16 == 8)
    rgb = mix(rgb, bolt, (90, 90, 90))
    if rusty:
        rgb = mix(rgb, fbm(seed + 2) > 0.80, (40, 28, 22))      # agujeros
        return retro(rgb, 16)
    return retro(rgb, 12)


def container(rusty=False, seed=61):
    s = np.where(XX % 8 < 4, 1.0, 0.78) + 0.05 * (white(seed) - 0.5)
    s = np.where(XX % 8 == 4, s * 0.85, s)
    rgb = shade((230, 230, 230), s)
    frame = (XX < 3) | (XX > 60) | (YY < 3) | (YY > 60)
    rgb = mix(rgb, frame, (122, 122, 122))
    corner = ((XX < 6) | (XX > 57)) & ((YY < 6) | (YY > 57))
    rgb = mix(rgb, corner, (90, 90, 90))
    plate = (XX > 40) & (XX < 54) & (YY > 10) & (YY < 18)  # placa de datos
    rgb = mix(rgb, plate, (250, 250, 250), 0.8)
    rgb = mix(rgb, plate & (YY % 3 == 0) & (XX % 2 == 0), (110, 110, 110))
    if rusty:
        r = fbm(seed + 2)
        rgb = mix(rgb, r > 0.5, (140, 74, 40), np.clip((r - 0.5) * 5, 0.4, 0.9))
        rgb = mix(rgb, (np.roll(frame, 2, axis=0) & (white(seed + 3) < 0.5)), (110, 60, 34), 0.6)
        return retro(rgb, 16)
    return retro(rgb, 10)


def wood_crate(rotten=False, seed=71):
    grain = hash2(XX // 3, YY, seed) * 0.6 + 0.4 * hash2(XX // 5, YY // 2, seed + 1)
    s = 0.86 + 0.16 * (grain - 0.5) + 0.06 * (white(seed + 2) - 0.5)
    frame = (XX < 6) | (XX > 57) | (YY < 6) | (YY > 57)
    brace = np.abs(XX - YY) <= 4
    s = np.where(frame | brace, s * 1.12, np.where(YY % 16 == 0, s * 0.55, s))
    edge = (frame | brace) & ((XX == 6) | (XX == 57) | (YY == 6) | (YY == 57) | (np.abs(XX - YY) == 4))
    s = np.where(edge, s * 0.68, s)
    base = (104, 82, 58) if rotten else (158, 112, 64)
    rgb = shade(base, s)
    nail = (((XX == 3) | (XX == 60)) & ((YY % 16) == 8)) | (((YY == 3) | (YY == 60)) & ((XX % 16) == 8))
    rgb = mix(rgb, nail, (64, 60, 56))
    if rotten:
        rot = fbm(seed + 3) > 0.6
        rgb = mix(rgb, rot, (56, 50, 40), 0.7)
        rgb = mix(rgb, fbm(seed + 4) > 0.75, (66, 84, 48), 0.6)   # moho
        return retro(rgb, 14)
    return retro(rgb, 12)


def checker():
    on = ((XX // 16) + (YY // 16)) % 2 == 0
    rgb = np.where(on[..., None], np.array([228, 176, 55]), np.array([44, 44, 56])).astype(float)
    rgb[(XX % 32 == 0) | (YY % 32 == 0)] = 255
    return retro(rgb, 3)


def shutter():
    s = np.choose(YY % 8, [0.58, 0.8, 1.0, 0.96, 0.92, 0.88, 0.84, 0.72]) + 0.04 * (white(81) - 0.5)
    s = np.where(YY > 59, s * 0.5, s)
    rgb = shade((212, 212, 206), s)
    rgb = mix(rgb, (XX > 28) & (XX < 36) & (YY > 54) & (YY < 58), (80, 80, 80))  # tirador
    return retro(rgb, 8)


def office_window():
    s = 0.92 + 0.08 * (white(91) - 0.5)
    rgb = shade((210, 200, 182), np.where(YY >= 60, s * 0.75, s))
    inside = (XX >= 10) & (XX <= 53) & (YY >= 14) & (YY <= 45)
    sky = 0.7 + 0.3 * (1 - (YY - 14) / 32.0)
    rgb[inside] = np.stack([48 * sky, 78 * sky, 112 * sky], axis=-1)[inside]
    blinds = inside & (YY < 24) & (YY % 2 == 0)
    rgb = mix(rgb, blinds, (180, 176, 160), 0.8)
    glint = inside & ((XX - YY) > 4) & ((XX - YY) < 12)
    rgb = mix(rgb, glint, (150, 185, 215), 0.8)
    bars = inside & ((XX == 10) | (XX == 53) | (YY == 14) | (YY == 45) | (XX == 31) | (XX == 32))
    rgb = mix(rgb, bars, (70, 72, 78))
    sill = (XX >= 8) & (XX <= 55) & (YY >= 46) & (YY <= 48)
    rgb = mix(rgb, sill, (230, 226, 214))
    return retro(rgb, 14)


def camo():
    a, b, c = blotch(4, 101), blotch(6, 102), blotch(8, 104)
    rgb = np.zeros((N, N, 3)) + np.array([196, 150, 98])
    rgb[b > 0.66] = (214, 176, 122)
    rgb[a > 0.62] = (150, 104, 62)
    rgb[(c > 0.74) & (a < 0.5)] = (110, 82, 52)
    rgb *= (0.95 + 0.06 * (white(103) - 0.5))[..., None]
    return retro(rgb, 6)


def water(murky=False):
    wave = np.sin(XX * 0.196 + YY * 0.59 + 3.0 * fbm(111)) + 0.5 * np.sin(XX * 0.49 - YY * 0.2 + 2.0 * blotch(4, 113))
    s = (0.86 + 0.14 * fbm(112))
    base = (52, 60, 44) if murky else (50, 102, 150)
    rgb = shade(base, s)
    crest = wave > 1.05
    rgb = mix(rgb, crest, (82, 88, 66) if murky else (128, 176, 212), 0.5 if murky else 0.75)
    rgb = mix(rgb, (wave > 0.85) & ~crest, (70, 78, 58) if murky else (86, 140, 186), 0.3 if murky else 0.5)
    deep = fbm(114) > 0.62
    rgb = mix(rgb, deep, (40, 50, 36) if murky else (38, 82, 128), 0.5)
    if murky:
        scum = (fbm(115) > 0.7) & (white(116) < 0.8)
        rgb = mix(rgb, scum, (86, 92, 52), 0.6)
        return retro(rgb, 10)
    return retro(rgb, 10)


def rock():
    band = 0.86 + 0.16 * blotch(16, 121)[:, 0][YY]
    s = band * (0.86 + 0.22 * fbm(122)) + 0.06 * (white(123) - 0.5)
    rgb = shade((150, 128, 108), s)
    rgb = mix(rgb, cracks(124, 0.02, 12), (86, 72, 60), 0.85)
    lichen = (fbm(125) > 0.66) & (white(126) < 0.6)
    rgb = mix(rgb, lichen, (120, 132, 82), 0.55)
    rgb = mix(rgb, white(127) < 0.02, (190, 176, 160), 0.6)
    return retro(rgb, 14)


def sand():
    ripple = np.sin(YY * 0.4 + 2.5 * blotch(4, 133)) > 0.7
    s = 0.90 + 0.12 * fbm(131) + 0.08 * (white(132) - 0.5)
    rgb = shade((214, 196, 150), np.where(ripple, s * 0.92, s))
    rgb = mix(rgb, white(134) < 0.03, (240, 228, 196), 0.7)
    rgb = mix(rgb, white(135) < 0.02, (160, 140, 104), 0.6)
    return retro(rgb, 10)


def block_wall():
    offset = ((YY // 32) % 2) * 16
    joint = (YY % 32 < 2) | ((XX + offset) % 32 < 2)
    s = 0.90 + 0.12 * fbm(141) + 0.05 * (white(142) - 0.5)
    s = s + 0.05 * (hash2((XX + offset) // 32, YY // 32, 143) - 0.5)   # cada bloque con su tono
    rgb = shade((190, 184, 168), s)
    rgb = mix(rgb, joint, (120, 116, 106), 0.85)
    rgb = mix(rgb, (white(144) < 0.03), (150, 146, 134), 0.7)       # poros
    chip = (fbm(145) > 0.74) & ~joint
    rgb = mix(rgb, chip, (160, 154, 140), 0.6)
    rgb = mix(rgb, YY >= 60, (150, 146, 136), 0.6)                  # albardilla
    return retro(rgb, 12)


def wood_wall():
    plank = hash2(np.zeros_like(XX), YY // 16, 151)
    grain = hash2(XX // 4, YY, 152) * 0.7 + 0.3 * hash2(XX // 9, YY // 3, 154)
    s = 0.86 + 0.12 * plank + 0.12 * (grain - 0.5)
    s = np.where(YY % 16 == 0, s * 0.5, np.where(YY % 16 == 1, s * 0.8, s))
    side = (XX < 6) | (XX > 57)
    s = np.where(side, 0.76 + 0.06 * white(153), s)
    rgb = shade((150, 98, 52), s)
    knot = (blotch(8, 155) > 0.82) & ~side
    rgb = mix(rgb, knot, (96, 62, 34), 0.8)
    nails = ((XX == 2) | (XX == 61)) & (YY % 16 == 8)
    rgb = mix(rgb, nails, (60, 56, 50))
    return retro(rgb, 12)


# --- Modo zombi (v0.14) -------------------------------------------------------------------------

def dead_grass():
    s = 0.78 + 0.34 * fbm(211) + 0.10 * (white(212) - 0.5)
    rgb = shade((118, 106, 62), s)
    dirtp = fbm(213) > 0.62
    rgb = mix(rgb, dirtp, (92, 72, 50), 0.8)
    blade = (white(214) < 0.08)
    rgb = mix(rgb, blade, (150, 136, 84), 0.7)
    rgb = mix(rgb, (white(215) < 0.03) & ~dirtp, (70, 84, 46), 0.7)   # algo verde
    return retro(rgb, 12)


def cracked_concrete():
    s = 0.80 + 0.20 * fbm(231) + 0.06 * (white(232) - 0.5)
    rgb = shade((130, 128, 120), s)
    joint = (XX < 2) | (YY < 2)
    rgb = mix(rgb, joint, (70, 68, 64), 0.8)
    rgb = mix(rgb, cracks(233, 0.05, 14), (54, 52, 48), 0.9)
    moss = (fbm(234) > 0.6) & (white(235) < 0.7)
    rgb = mix(rgb, moss, (70, 86, 50), 0.6)
    stain = fbm(236) > 0.7
    rgb = mix(rgb, stain, (90, 70, 56), 0.4)   # manchas de oxido y sangre seca
    return retro(rgb, 14)


def canvas():
    weave = ((XX + YY) % 4 == 0) | ((XX - YY) % 4 == 0)
    s = 0.88 + 0.08 * white(261) + np.where(weave, -0.07, 0.0) + 0.1 * (fbm(262) - 0.5)
    rgb = shade((200, 196, 180), s)
    patch = (XX > 36) & (XX < 54) & (YY > 8) & (YY < 24)
    rgb = mix(rgb, patch, (150, 140, 116), 0.7)
    stitch = patch & ((XX == 37) | (XX == 53) | (YY == 9) | (YY == 23)) & ((XX + YY) % 3 == 0)
    rgb = mix(rgb, stitch, (70, 64, 56))
    rgb = mix(rgb, fbm(263) > 0.7, (120, 104, 80), 0.45)
    return retro(rgb, 12)


TEXTURES = [
    ("00_hierba", grass), ("01_tierra", dirt), ("02_hormigon", concrete),
    ("03_metal_ondulado", metal_siding), ("04_tejado_metal", roof_panel), ("05_contenedor", container),
    ("06_madera_caja", wood_crate), ("07_damero", checker), ("08_persiana", shutter),
    ("09_fachada_oficina", office_window), ("10_camuflaje", camo), ("11_agua", water),
    ("12_roca", rock), ("13_arena", sand), ("14_muro_bloques", block_wall), ("15_madera_pared", wood_wall),
    ("16_hierba_seca", dead_grass), ("17_metal_oxidado", lambda: metal_siding(True, 221)),
    ("18_tejado_oxidado", lambda: roof_panel(True, 225)), ("19_contenedor_oxidado", lambda: container(True, 229)),
    ("20_agua_turbia", lambda: water(True)), ("21_hormigon_agrietado", cracked_concrete),
    ("22_madera_podrida", lambda: wood_crate(True, 251)), ("23_lona", canvas),
]


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    for name, fn in TEXTURES:
        img = fn()
        assert img.size == (N, N)
        img.save(OUT / f"{name}.png", optimize=True)
        print(f"  Recursos/Texturas/{name}.png  {len(img.getcolors())} colores")


if __name__ == "__main__":
    main()
