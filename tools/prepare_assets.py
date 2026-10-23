#!/usr/bin/env python3
"""Convierte Recursos/ en los ficheros listos para la PSP de assets/ (y src/assets/ui_atlas.h).

Uso (desde la raiz del proyecto):
    python3 tools/generate_textures.py   # (opcional) regenera Recursos/Texturas
    python3 tools/generate_sounds.py     # (opcional) regenera los efectos de Recursos/Sonidos
    python3 tools/generate_hud.py        # (opcional) regenera los iconos propios de Recursos/HUD
    python3 tools/prepare_assets.py      # o: make assets

Requiere Pillow y numpy; para la musica usa `afconvert` (incluido en macOS). Los
ficheros generados se versionan, asi que compilar NO necesita Python.

Recursos/    -> recursos directos del juego (se convierten y se usan tal cual).
Referencias/ -> solo material de referencia visual, nunca se empaqueta.
"""

import struct
import subprocess
import tempfile
import wave
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parent.parent
RES = ROOT / "Recursos"
ASSETS = ROOT / "assets"
FONT = "/System/Library/Fonts/Supplemental/Impact.ttf"  # la mas parecida a la de Fortnite

SCREEN_SIZE = (480, 272)  # resolucion nativa de la PSP
ICON0_SIZE = (144, 80)    # icono del XMB
LOGO_WIDTH = 256


def cover(img, size):
    """Escala para cubrir `size` manteniendo proporcion y recorta centrado."""
    tw, th = size
    scale = max(tw / img.width, th / img.height)
    w, h = round(img.width * scale), round(img.height * scale)
    img = img.resize((w, h), Image.LANCZOS)
    left, top = (w - tw) // 2, (h - th) // 2
    return img.crop((left, top, left + tw, top + th))


def save(img, path):
    path.parent.mkdir(parents=True, exist_ok=True)
    img.save(path, "PNG", optimize=True)
    print(f"  {path.relative_to(ROOT)}  {img.width}x{img.height}  {path.stat().st_size} bytes")


def alpha_crop(img, threshold=24):
    bbox = img.getchannel("A").point(lambda v: 255 if v > threshold else 0).getbbox()
    return img.crop(bbox)


# --- Imagenes de pantalla completa -------------------------------------------------------

def screens():
    login = Image.open(RES / "loginscreen.png").convert("RGB")
    save(cover(login, SCREEN_SIZE), ASSETS / "images" / "login.png")

    match = Image.open(RES / "Loadscreen - match.png").convert("RGB")
    loadscreen = cover(match, SCREEN_SIZE)
    save(loadscreen, ASSETS / "images" / "loadscreen.png")
    save(loadscreen, ASSETS / "pbp" / "PIC1.PNG")
    w, h = match.size
    crop = match.crop((int(w * 0.22), int(h * 0.02), int(w * 0.78), int(h * 0.98)))
    save(cover(crop, ICON0_SIZE), ASSETS / "pbp" / "ICON0.PNG")

    logo = alpha_crop(Image.open(RES / "fortnite_PNG25.png").convert("RGBA"))
    height = round(logo.height * LOGO_WIDTH / logo.width)
    save(logo.resize((LOGO_WIDTH, height), Image.LANCZOS), ASSETS / "images" / "logo.png")


# --- Atlas de interfaz ------------------------------------------------------------------------

def label(text, size, fill=(255, 255, 255, 255), stroke=2):
    font = ImageFont.truetype(FONT, size)
    box = font.getbbox(text, stroke_width=stroke)
    img = Image.new("RGBA", (box[2] - box[0] + 2, box[3] - box[1] + 2), (0, 0, 0, 0))
    ImageDraw.Draw(img).text((1 - box[0], 1 - box[1]), text, font=font, fill=fill,
                             stroke_width=stroke, stroke_fill=(16, 16, 28, 255))
    return img


def icon(name, size):
    img = alpha_crop(Image.open(RES / "HUD" / name).convert("RGBA"), 8)
    img.thumbnail((size, size), Image.LANCZOS)
    return img


def victory_banner():
    banner = alpha_crop(Image.open(RES / "HUD" / "Victory_Royale_Old_-_Fortnite.webp").convert("RGBA"))
    return banner.resize((round(banner.width * 1.5), round(banner.height * 1.5)), Image.NEAREST)


def defeat_banner(size):
    """Banner original de derrota con la misma forma que el de victoria, en rojo."""
    w, h = size
    img = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    slant = h // 3
    d.polygon([(slant, 0), (w - 1, 0), (w - 1 - slant, h - 1), (0, h - 1)], fill=(168, 36, 40, 255))
    d.polygon([(slant, 0), (w - 1, 0), (w - 1, 4), (slant - 1, 4)], fill=(222, 86, 80, 255))
    text = label("ELIMINADO", int(h * 0.62), stroke=2)
    img.alpha_composite(text, ((w - text.width) // 2, (h - text.height) // 2))
    return img


def ui_atlas():
    victory = victory_banner()
    logo = alpha_crop(Image.open(RES / "fortnite_PNG25.png").convert("RGBA"))
    logo_small = logo.resize((112, round(logo.height * 112 / logo.width)), Image.LANCZOS)
    items = {
        "Victory": victory,
        "Defeat": defeat_banner(victory.size),
        "LogoSmall": logo_small,
        "Jugar": label("JUGAR", 30),
        "Controles": label("CONTROLES", 20),
        "Salir": label("SALIR", 20),
        "Mapa": label("POLVORÍN POLVORIENTO", 17),
        "IniciarSesion": label("INICIAR SESIÓN", 22),
        "Cargando": label("CARGANDO", 18),
        "Construccion": label("MODO CONSTRUCCIÓN", 13, stroke=1),
        "Pico": icon("Pico.png", 26),
        "Pared": icon("Pared.png", 26),
        "Rampa": icon("Rampa.png", 26),
        "Suelo": icon("Suelo.png", 26),
        "Scar": Image.open(RES / "HUD" / "scar.png").convert("RGBA"),
        "MatMadera": Image.open(RES / "HUD" / "material_madera.png").convert("RGBA"),
        "MatLadrillo": Image.open(RES / "HUD" / "material_ladrillo.png").convert("RGBA"),
        "MatMetal": Image.open(RES / "HUD" / "material_metal.png").convert("RGBA"),
        "IconVivos": Image.open(RES / "HUD" / "icono_vivos.png").convert("RGBA"),
        "IconBajas": Image.open(RES / "HUD" / "icono_bajas.png").convert("RGBA"),
        "IconZona": Image.open(RES / "HUD" / "icono_zona.png").convert("RGBA"),
        "IconEscudo": Image.open(RES / "HUD" / "icono_escudo.png").convert("RGBA"),
        "IconVida": Image.open(RES / "HUD" / "icono_vida.png").convert("RGBA"),
    }
    # Empaquetado por estantes en un atlas de 512 px de ancho (altura potencia de 2).
    width = 512
    order = sorted(items, key=lambda k: -items[k].height)
    rects, x, y, shelf = {}, 0, 0, 0
    for name in order:
        img = items[name]
        if x + img.width > width:
            x, y, shelf = 0, y + shelf + 1, 0
        rects[name] = (x, y, img.width, img.height)
        x += img.width + 1
        shelf = max(shelf, img.height)
    height = 1
    while height < y + shelf:
        height *= 2
    atlas = Image.new("RGBA", (width, height), (0, 0, 0, 0))
    for name, (rx, ry, _, _) in rects.items():
        atlas.alpha_composite(items[name], (rx, ry))
    save(atlas, ASSETS / "images" / "ui_atlas.png")

    lines = [
        "// Generado por tools/prepare_assets.py: no editar a mano.",
        "// Rectangulos (en pixeles) de cada elemento en assets/images/ui_atlas.png.",
        "#pragma once",
        "",
        "namespace assets {",
        "namespace ui {",
        "",
        "struct Rect {",
        "    short x, y, w, h;",
        "};",
        "",
        f"constexpr int kAtlasWidth = {width};",
        f"constexpr int kAtlasHeight = {height};",
    ]
    for name in items:
        rx, ry, rw, rh = rects[name]
        lines.append(f"constexpr Rect k{name}{{{rx}, {ry}, {rw}, {rh}}};")
    lines += ["", "}  // namespace ui", "}  // namespace assets", ""]
    header = ROOT / "src" / "assets" / "ui_atlas.h"
    header.write_text("\n".join(lines))
    print(f"  {header.relative_to(ROOT)}  {len(items)} elementos")


# --- Texturas del mundo -------------------------------------------------------------------------

def texture_atlas():
    files = sorted((RES / "Texturas").glob("*.png"))
    assert len(files) == 16, "se esperan 16 texturas (00..15) en Recursos/Texturas"
    atlas = Image.new("RGB", (128, 128))
    for i, f in enumerate(files):
        tile = Image.open(f).convert("RGB")
        assert tile.size == (32, 32), f"{f.name} debe medir 32x32"
        atlas.paste(tile, ((i % 4) * 32, (i // 4) * 32))
    save(atlas, ASSETS / "images" / "textures.png")


# --- Sonido -------------------------------------------------------------------------------------

IMA_STEPS = [
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 50, 55, 60, 66,
    73, 80, 88, 97, 107, 118, 130, 143, 157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449,
    494, 544, 598, 658, 724, 796, 876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272,
    2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493,
    10442, 11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767]
IMA_INDEX = [-1, -1, -1, -1, 2, 4, 6, 8]
BLOCK_BYTES = 1024
SAMPLES_PER_BLOCK = 1 + (BLOCK_BYTES - 4) * 2  # muestra de cabecera + 2 por byte


def ima_encode(samples, rate):
    """IMA ADPCM por bloques (4:1). Formato propio sencillo, decodificado en src/audio."""
    out = bytearray(b"IMA1" + struct.pack("<IIII", rate, len(samples), BLOCK_BYTES, SAMPLES_PER_BLOCK))
    for start in range(0, len(samples), SAMPLES_PER_BLOCK):
        block = samples[start:start + SAMPLES_PER_BLOCK]
        predictor = int(block[0])
        index = 0
        if start > 0:
            index = state_index
        data = bytearray(struct.pack("<hBB", predictor, index, 0))
        nibbles = []
        for s in block[1:]:
            step = IMA_STEPS[index]
            diff = int(s) - predictor
            code = 0
            if diff < 0:
                code = 8
                diff = -diff
            delta = step >> 3
            if diff >= step:
                code |= 4
                diff -= step
                delta += step
            step >>= 1
            if diff >= step:
                code |= 2
                diff -= step
                delta += step
            step >>= 1
            if diff >= step:
                code |= 1
                delta += step
            predictor += -delta if code & 8 else delta
            predictor = max(-32768, min(32767, predictor))
            index = max(0, min(88, index + IMA_INDEX[code & 7]))
            nibbles.append(code)
        state_index = index
        if len(nibbles) % 2:
            nibbles.append(0)
        for i in range(0, len(nibbles), 2):
            data.append(nibbles[i] | (nibbles[i + 1] << 4))
        data += bytes(BLOCK_BYTES - len(data))
        out += data
    return bytes(out)


def read_wav(path):
    with wave.open(str(path), "rb") as w:
        assert w.getsampwidth() == 2
        data = np.frombuffer(w.readframes(w.getnframes()), dtype="<i2")
        return data.reshape(-1, w.getnchannels()), w.getframerate()


def sounds():
    out = ASSETS / "sounds"
    out.mkdir(parents=True, exist_ok=True)
    for wav in sorted((RES / "Sonidos").glob("*.wav")):
        if wav.name == "main title song.wav":
            continue
        data, rate = read_wav(wav)
        assert data.shape[1] == 1 and rate == 22050, f"{wav.name}: se espera mono 22050 Hz"
        target = out / wav.name
        target.write_bytes(wav.read_bytes())
        print(f"  {target.relative_to(ROOT)}  {len(data) / rate:.2f} s")

    # Musica del menu: 48 kHz estereo (22 MB) -> 22050 Hz mono -> IMA ADPCM (~1.3 MB).
    song = RES / "Sonidos" / "main title song.wav"
    with tempfile.TemporaryDirectory() as tmp:
        mono = Path(tmp) / "mono.wav"
        subprocess.run(["afconvert", "-f", "WAVE", "-d", "LEI16@22050", "-c", "1", str(song), str(mono)],
                       check=True)
        data, rate = read_wav(mono)
    encoded = ima_encode(data[:, 0], rate)
    target = out / "musica_menu.ima"
    target.write_bytes(encoded)
    print(f"  {target.relative_to(ROOT)}  {len(data) / rate:.1f} s  {len(encoded)} bytes "
          f"(original {song.stat().st_size} bytes)")


def main():
    screens()
    ui_atlas()
    texture_atlas()
    sounds()


if __name__ == "__main__":
    main()
