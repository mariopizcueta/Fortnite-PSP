#!/usr/bin/env python3
"""Genera las texturas originales del juego en Recursos/Texturas/ (32x32, estilo PS1).

Uso (desde la raiz del proyecto):  python3 tools/generate_textures.py

Cada textura se dibuja por procedimiento (ruido determinista, nada copiado de ningun
juego) y despues se reduce a una paleta retro de pocos colores. El numero inicial del
nombre fija su posicion en el atlas que usa el juego (orden de world::TextureId); las
de metal, contenedor y persiana son grises porque el juego las tine con el color de
cada edificio. Se pueden retocar a mano: prepare_assets.py usa estos ficheros tal cual.
"""

import math
from pathlib import Path

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "Recursos" / "Texturas"
N = 32


def hash2(x, y, seed):
    h = (x * 374761393 + y * 668265263 + seed * 2246822519) & 0xFFFFFFFF
    h = ((h ^ (h >> 13)) * 1274126177) & 0xFFFFFFFF
    return ((h ^ (h >> 16)) & 0xFFFF) / 65536.0


def white(seed):
    return np.array([[hash2(x, y, seed) for x in range(N)] for y in range(N)])


def blotch(cells, seed):
    """Ruido de valor suave y repetible (tileable) con `cells` celdas por lado."""
    g = np.array([[hash2(i, j, seed) for i in range(cells)] for j in range(cells)])
    out = np.zeros((N, N))
    for y in range(N):
        for x in range(N):
            fx, fy = x * cells / N, y * cells / N
            x0, y0 = int(fx), int(fy)
            tx, ty = fx - x0, fy - y0
            tx, ty = tx * tx * (3 - 2 * tx), ty * ty * (3 - 2 * ty)
            a, b = g[y0 % cells, x0 % cells], g[y0 % cells, (x0 + 1) % cells]
            c, d = g[(y0 + 1) % cells, x0 % cells], g[(y0 + 1) % cells, (x0 + 1) % cells]
            out[y, x] = (a + (b - a) * tx) * (1 - ty) + (c + (d - c) * tx) * ty
    return out


def shade(base, s):
    """Imagen RGB a partir de un color base y un mapa de intensidad."""
    rgb = np.stack([np.clip(base[i] * s, 0, 255) for i in range(3)], axis=-1)
    return rgb


def retro(rgb, colors):
    """Reduce a una paleta de `colors` colores (sin tramado: pixeles limpios)."""
    img = Image.fromarray(rgb.astype(np.uint8))
    return img.quantize(colors=colors, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE).convert("RGB")


def grass():
    s = 0.80 + 0.34 * blotch(4, 11) + 0.10 * (white(12) - 0.5)
    w = white(13)
    s = np.where(w < 0.08, s * 0.70, np.where(w > 0.95, s * 1.18, s))
    rgb = shade((74, 124, 50), s)
    rgb[..., 0] += 10 * blotch(3, 14)  # toque amarillento en manchas
    return retro(rgb, 8)


def dirt():
    s = 0.84 + 0.26 * blotch(4, 21) + 0.10 * (white(22) - 0.5)
    w = white(23)
    rgb = shade((140, 106, 72), np.where(w > 0.95, s * 0.68, s))
    rgb[w < 0.05] += 32
    return retro(rgb, 8)


def concrete():
    s = 0.88 + 0.14 * blotch(4, 31) + 0.07 * (white(32) - 0.5)
    yy, xx = np.mgrid[0:N, 0:N]
    s = np.where((xx == 0) | (yy == 0), s * 0.66, s)       # juntas entre losas de 4 m
    s = np.where(white(33) < 0.03, s * 0.80, s)            # desconchones
    return retro(shade((152, 150, 144), s), 6)


def metal_siding():
    profile = np.array([0.70, 0.86, 1.0, 1.0, 0.90, 0.78, 0.64, 0.62])
    yy, xx = np.mgrid[0:N, 0:N]
    s = profile[xx % 8] + 0.06 * (white(41) - 0.5)
    drip = np.clip((yy - 23) / 9.0, 0, 1) * blotch(8, 42)[0][xx]  # suciedad que escurre
    s = s * (1 - 0.3 * drip)
    return retro(shade((236, 236, 236), s), 8)


def roof_panel():
    yy, xx = np.mgrid[0:N, 0:N]
    s = np.where(xx % 16 < 3, 1.0, 0.80) + 0.08 * (white(51) - 0.5)
    rgb = shade((222, 222, 216), s)
    rust = blotch(4, 52) > 0.70
    rgb[rust] *= np.array([0.95, 0.68, 0.52])
    return retro(rgb, 8)


def container():
    yy, xx = np.mgrid[0:N, 0:N]
    s = np.where(xx % 4 < 2, 1.0, 0.78) + 0.06 * (white(61) - 0.5)
    rgb = shade((230, 230, 230), s)
    frame = (xx < 2) | (xx > 29) | (yy < 2) | (yy > 29)
    rgb[frame] = (122, 122, 122)
    return retro(rgb, 6)


def wood_crate():
    yy, xx = np.mgrid[0:N, 0:N]
    s = 0.92 + 0.12 * (white(71)[:, :] - 0.5) + 0.06 * (white(72) - 0.5)
    frame = (xx < 3) | (xx > 28) | (yy < 3) | (yy > 28)
    brace = np.abs(xx - yy) <= 2
    s = np.where(frame | brace, s * 1.12, np.where(yy % 8 == 0, s * 0.55, s))
    edge = (frame | brace) & ((xx == 3) | (xx == 28) | (yy == 3) | (yy == 28) | (np.abs(xx - yy) == 2))
    s = np.where(edge, s * 0.7, s)
    return retro(shade((158, 112, 64), s), 8)


def checker():
    yy, xx = np.mgrid[0:N, 0:N]
    on = ((xx // 8) + (yy // 8)) % 2 == 0
    rgb = np.where(on[..., None], np.array([228, 176, 55]), np.array([44, 44, 56])).astype(float)
    rgb[(xx % 16 == 0) | (yy % 16 == 0)] = 255
    return retro(rgb, 3)


def shutter():
    yy, xx = np.mgrid[0:N, 0:N]
    s = np.choose(yy % 4, [0.62, 1.0, 0.88, 0.88]) + 0.05 * (white(81) - 0.5)
    s = np.where(yy > 29, s * 0.5, s)
    return retro(shade((212, 212, 206), s), 5)


def office_window():
    yy, xx = np.mgrid[0:N, 0:N]
    s = 0.92 + 0.10 * (white(91) - 0.5)
    rgb = shade((210, 200, 182), np.where(yy >= 30, s * 0.75, s))
    inside = (xx >= 5) & (xx <= 26) & (yy >= 7) & (yy <= 22)
    rgb[inside] = (48, 78, 112)
    glint = inside & ((xx - yy) > 2) & ((xx - yy) < 6)
    rgb[glint] = (150, 185, 215)
    bars = inside & ((xx == 5) | (xx == 26) | (yy == 7) | (yy == 22) | (xx == 15) | (xx == 16))
    rgb[bars] = (70, 72, 78)
    return retro(rgb, 8)


def camo():
    a, b = blotch(4, 101), blotch(6, 102)
    rgb = np.zeros((N, N, 3)) + np.array([196, 150, 98])
    rgb[b > 0.66] = (214, 176, 122)
    rgb[a > 0.62] = (150, 104, 62)
    rgb *= (0.95 + 0.08 * (white(103) - 0.5))[..., None]
    return retro(rgb, 4)


def water():
    yy, xx = np.mgrid[0:N, 0:N]
    wave = np.sin(xx * 0.39 + yy * 1.18 + 3.0 * blotch(4, 111))
    s = (0.90 + 0.10 * blotch(4, 112)) * np.where(wave > 0.82, 1.28, 1.0)
    return retro(shade((52, 104, 150), s), 5)


def rock():
    yy, xx = np.mgrid[0:N, 0:N]
    band = 0.88 + 0.14 * blotch(8, 121)[:, 0][yy]
    s = band * (0.90 + 0.18 * blotch(4, 122)) + 0.06 * (white(123) - 0.5)
    crack = np.array([[hash2(x // 2, y, 124) for x in range(N)] for y in range(N)]) < 0.04
    return retro(shade((150, 128, 108), np.where(crack, s * 0.65, s)), 7)


def sand():
    s = 0.92 + 0.12 * blotch(4, 131) + 0.08 * (white(132) - 0.5)
    return retro(shade((214, 196, 150), s), 5)


def block_wall():
    yy, xx = np.mgrid[0:N, 0:N]
    offset = ((yy // 16) % 2) * 8
    joint = (yy % 16 == 0) | ((xx + offset) % 16 == 0)
    s = 0.92 + 0.10 * blotch(4, 141) + 0.05 * (white(142) - 0.5)
    s = np.where(joint, s * 0.72, s)
    s = np.where(yy >= 30, s * 0.85, s)
    return retro(shade((190, 184, 168), s), 6)


def wood_wall():
    yy, xx = np.mgrid[0:N, 0:N]
    plank = np.array([[hash2(y // 8, 0, 151) for _ in range(N)] for y in range(N)])
    grain = np.array([[hash2(x // 3, y, 152) for x in range(N)] for y in range(N)])
    s = 0.88 + 0.10 * plank + 0.10 * (grain - 0.5)
    s = np.where(yy % 8 == 0, s * 0.55, s)
    side = (xx < 3) | (xx > 28)
    s = np.where(side, 0.78 + 0.06 * white(153), s)
    rgb = shade((150, 98, 52), s)
    nails = ((xx == 1) | (xx == 30)) & (yy % 8 == 4)
    rgb[nails] = (60, 56, 50)
    return retro(rgb, 8)


TEXTURES = [
    ("00_hierba", grass), ("01_tierra", dirt), ("02_hormigon", concrete),
    ("03_metal_ondulado", metal_siding), ("04_tejado_metal", roof_panel), ("05_contenedor", container),
    ("06_madera_caja", wood_crate), ("07_damero", checker), ("08_persiana", shutter),
    ("09_fachada_oficina", office_window), ("10_camuflaje", camo), ("11_agua", water),
    ("12_roca", rock), ("13_arena", sand), ("14_muro_bloques", block_wall), ("15_madera_pared", wood_wall),
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
