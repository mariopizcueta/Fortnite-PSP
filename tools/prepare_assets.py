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
from PIL import Image, ImageDraw, ImageFilter, ImageFont

ROOT = Path(__file__).resolve().parent.parent
RES = ROOT / "Recursos"
ASSETS = ROOT / "assets"
FONT = "/System/Library/Fonts/Supplemental/Impact.ttf"  # la mas parecida a la de Fortnite

SCREEN_SIZE = (480, 272)  # resolucion nativa de la PSP
ICON0_SIZE = (144, 80)    # icono del XMB


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
    scope()


def scope():
    """Mirilla del francotirador (Recursos/mirilla.png) a media resolucion (240x136; se
    dibuja a 2x). Fuera del aro, negro opaco (en el original es transparente); dentro,
    transparente. Las lineas finas de la reticula desaparecen al reducir: el juego dibuja
    la suya."""
    img = Image.open(RES / "mirilla.png").convert("RGBA")
    cx, cy, ring = 836, 465, 575  # centro del aro y radio entre el borde interior y el exterior
    px = img.load()
    for y in range(img.height):
        for x in range(img.width):
            if px[x, y][3] < 128 and (x - cx) ** 2 + (y - cy) ** 2 > ring * ring:
                px[x, y] = (0, 0, 0, 255)
    small = img.resize((240, 136), Image.LANCZOS)
    small.putalpha(small.getchannel("A").point(lambda v: 255 if v > 140 else 0))
    save(small, ASSETS / "images" / "mirilla.png")



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


def slot_art(path, size=32, margin=0.06):
    """Captura de una ranura del inventario con su fondo de rareza: sin el marco, a `size`."""
    img = Image.open(path).convert("RGB")
    w, h = img.size
    img = img.crop((round(w * margin), round(h * margin), round(w * (1 - margin)), round(h * (1 - margin))))
    return img.resize((size, size), Image.LANCZOS).convert("RGBA")


def button(name, height):
    """Boton de PSP de Recursos/Texturas PSP, recortado y a `height` px de alto."""
    img = alpha_crop(Image.open(RES / "Texturas PSP" / name).convert("RGBA"), 8)
    width = max(1, round(img.width * height / img.height))
    img = img.resize((width, height), Image.LANCZOS)
    # Contorno claro de 1 px: los botones son oscuros y deben leerse sobre los paneles
    # azul marino de los menus y sobre el escenario.
    out = Image.new("RGBA", (width + 2, height + 2), (0, 0, 0, 0))
    halo = Image.new("RGBA", out.size, (235, 240, 250, 0))
    alpha = Image.new("L", out.size, 0)
    alpha.paste(img.getchannel("A"), (1, 1))
    halo.putalpha(alpha.filter(ImageFilter.MaxFilter(3)).point(lambda v: 210 if v > 60 else 0))
    out.alpha_composite(halo)
    out.alpha_composite(img, (1, 1))
    return out


BUTTONS = {
    "BtnCross": ("PlayStation_button_X.svg.webp", 13),
    "BtnCircle": ("PlayStation_button_C.svg.webp", 13),
    "BtnSquare": ("PlayStation_button_S.svg.webp", 13),
    "BtnTriangle": ("PlayStation_button_T.svg.webp", 13),
    "BtnL": ("PlayStation_Portable_button_L.svg.webp", 10),
    "BtnR": ("PlayStation_Portable_button_R.svg.webp", 10),
    "BtnStart": ("PlayStation_Portable_button_Start.svg.webp", 12),
    "BtnSelect": ("PlayStation_Portable_button_Select.svg.webp", 12),
    "BtnUp": ("PlayStation_Portable_button_Up.svg.webp", 13),
    "BtnDown": ("PlayStation_Portable_button_Down.svg.webp", 13),
    "BtnLeft": ("PlayStation_Portable_button_Left.svg.webp", 10),
    "BtnRight": ("PlayStation_Portable_button_Right.svg.webp", 10),
    "BtnDpad": ("PlayStation_Directional_button.svg.webp", 13),
    "BtnStick": ("PlayStation_Portable_button_Analog_Stick.svg.webp", 13),
}


# Fondos de rareza de Fortnite (arriba, abajo): comun, poco comun, rara, epica, legendaria.
RARITY_COLORS = [((150, 156, 162), (92, 98, 104)), ((104, 196, 62), (40, 120, 22)),
                 ((70, 170, 240), (26, 92, 186)), ((186, 96, 236), (104, 40, 166)),
                 ((246, 172, 54), (192, 104, 20))]
# Rarezas en las que existe cada arma (mismo criterio que game::weaponHasRarity) y su icono:
# las siluetas de Recursos/HUD/Armas o las imagenes de Recursos/Weapons (parche, fase 8).
WEAPON_SLOTS = {
    # Fusil de asalto: gris, verde y azul con el fusil generico (Assault_rifle_icon.png); el
    # morado con la silueta de la SCAR; el dorado es Recursos/Weapons/AssaultRiffleLegendary.png.
    "AR": ("Weapons/Assault_rifle_icon.png", [0, 1, 2]),
    "Scar": ("HUD/Armas/fusil.png", [3]),
    "Shotgun": ("HUD/Armas/escopeta.png", [1, 2, 3, 4]),
    "Smg": ("HUD/Armas/subfusil.png", [0, 1, 2, 3, 4]),
    "Sniper": ("HUD/Armas/francotirador.png", [2, 3, 4]),
    "Burst": ("Weapons/Burst_Assault_Rifle_-_Weapon_-_Fortnite.webp", [0, 1, 2, 3, 4]),
    "Scoped": ("Weapons/Scoped_Assault_Rifle_-_Weapon_-_Fortnite.webp", [2, 3]),
    "Smg2": ("Weapons/Submachine_Gun_%28v1.6%29_-_Weapon_-_Fortnite.webp", [0, 1, 2]),
    "Tactical": ("Weapons/Tactical_Shotgun.webp", [0, 1, 2, 3, 4]),
    "Pistol": ("Weapons/pistola.png", [0, 1, 2]),
    "Revolver": ("Weapons/revolver.jpeg", [0, 1, 2]),
    "SemiSniper": ("Weapons/Icon_Weapons_SK_Sniper_02_L.webp", [1, 2]),
    "Rocket": ("Weapons/Bazooka_-_Schematic_-_Save_the_World.webp", [2, 3, 4]),
    "Launcher": ("Weapons/Icon_Weapons_SK_Grenade_Launcher_L.webp", [2, 3, 4]),
    "Grenade": ("Weapons/Granada.png", [0]),
    "Trap": ("HUD/spiketrap.webp", [1]),
}


def remove_white_background(img, tolerance=24):
    """Quita el fondo blanco de una imagen sin transparencia (Recursos/Weapons/revolver.jpeg):
    relleno desde los bordes de los pixeles casi blancos. El fichero original no se toca."""
    img = img.convert("RGBA")
    px = img.load()
    w, h = img.size
    near_white = lambda c: c[0] > 255 - tolerance and c[1] > 255 - tolerance and c[2] > 255 - tolerance
    seen = set()
    stack = [(x, y) for x in range(w) for y in (0, h - 1)] + [(x, y) for y in range(h) for x in (0, w - 1)]
    while stack:
        x, y = stack.pop()
        if (x, y) in seen or not (0 <= x < w and 0 <= y < h):
            continue
        seen.add((x, y))
        if not near_white(px[x, y]):
            continue
        px[x, y] = (255, 255, 255, 0)
        stack.extend(((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)))
    return img


def weapon_icon(path):
    img = Image.open(RES / path)
    if img.mode != "RGBA":
        img = remove_white_background(img)
    return alpha_crop(img.convert("RGBA"), 8)


def weapon_slot(icon_path, rarity, size=32):
    """Ranura de arma: degradado de la rareza y el icono del arma encima (sin deformarlo,
    centrado en 30x30)."""
    top, bottom = RARITY_COLORS[rarity]
    slot = Image.new("RGBA", (size, size))
    draw = ImageDraw.Draw(slot)
    for y in range(size):
        t = y / (size - 1)
        draw.line((0, y, size, y), fill=tuple(round(a + (b - a) * t) for a, b in zip(top, bottom)) + (255,))
    icon = weapon_icon(icon_path)
    icon.thumbnail((size - 2, size - 2), Image.LANCZOS)
    icon.putalpha(icon.getchannel("A").point(lambda v: 255 if v > 100 else 0))
    slot.alpha_composite(icon, ((size - icon.width) // 2, (size - icon.height) // 2))
    return slot


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


def minimap_label():
    """Nombre del deposito para el minimapa, en dos lineas (el minimapa mide 76 px)."""
    lines = [label("Polvorín", 10, stroke=1), label("Polvoriento", 10, stroke=1)]
    w = max(l.width for l in lines)
    img = Image.new("RGBA", (w, lines[0].height + lines[1].height - 3), (0, 0, 0, 0))
    img.alpha_composite(lines[0], ((w - lines[0].width) // 2, 0))
    img.alpha_composite(lines[1], ((w - lines[1].width) // 2, lines[0].height - 3))
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
        "MapaMinimapa": minimap_label(),
        "IniciarSesion": label("INICIAR SESIÓN", 22),
        "Cargando": label("CARGANDO", 18),
        "Construccion": label("MODO CONSTRUCCIÓN", 13, stroke=1),
        "Pico": icon("Pico.png", 26),
        "Pared": icon("Pared.png", 26),
        "Rampa": icon("Rampa.png", 26),
        "Suelo": icon("Suelo.png", 26),
        "Trampa": icon("spiketrap.webp", 26),  # cuarta pieza de construccion (parche, fase 8)
        # Ranuras completas (con su fondo de rareza) dadas en Recursos.
        "SlotScar": slot_art(RES / "Weapons" / "AssaultRiffleLegendary.png"),
        "SlotMinipocion": slot_art(RES / "HUD" / "minipocion.png"),
        "SlotVendas": slot_art(RES / "HUD" / "Vendas.webp", margin=0.0),
        "SlotBotiquin": slot_art(RES / "HUD" / "botiquin.jpg", margin=0.0),
        "SlotPocionEscudo": slot_art(RES / "HUD" / "escudo grande.jpg", margin=0.0),
        # Materiales (Recursos/HUD/Wood_icon.png, stone_orig.png, Metal_icon.png).
        "MatMadera": icon("Wood_icon.png", 18),
        "MatLadrillo": icon("stone_orig.png", 18),
        "MatMetal": icon("Metal_icon.png", 18),
        "IconVivos": Image.open(RES / "HUD" / "icono_vivos.png").convert("RGBA"),
        "IconBajas": Image.open(RES / "HUD" / "icono_bajas.png").convert("RGBA"),
        "IconZona": Image.open(RES / "HUD" / "icono_zona.png").convert("RGBA"),
        "IconEscudo": Image.open(RES / "HUD" / "icono_escudo.png").convert("RGBA"),
        "IconVida": Image.open(RES / "HUD" / "icono_vida.png").convert("RGBA"),
    }
    # Ranuras de armas por rareza.
    for key, (icon_name, rarities) in WEAPON_SLOTS.items():
        for r in rarities:
            items[f"Slot{key}{r}"] = weapon_slot(icon_name, r)
    # Botones de la PSP (Recursos/Texturas PSP) para las ayudas de controles.
    for name, (source, height) in BUTTONS.items():
        items[name] = button(source, height)
    # Empaquetado por estantes en un atlas de 512 px de ancho. La altura es solo la usada
    # (multiplo de 8): la GE declara la potencia de 2 pero en RAM solo van esas filas.
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
    height = (y + shelf + 7) // 8 * 8
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


# Sonidos de Recursos/Sonidos/Sonidos Limpios que ya tienen uso en el juego -> nombre en
# assets/sounds.
CLEAN_SOUNDS = {
    # nombre en assets/sounds: (fichero, segundos maximos o None)
    "disparo": ("scardisparo.wav", None),                       # fusil de asalto / SCAR
    "victoria": ("Fortnite Victory Royale.mp3", None),
    "muerte_jugador": ("muertejugadorfortnite.wav", None),
    "miniescudo": ("miniescudo.wav", None),
    "pocion_escudo": ("pociondeescudogrande.wav", None),
    "aviso_tormenta": ("avisotormenta.wav", None),              # al entrar en la tormenta del borde
    "cofre_abrir": ("Chestopening.wav", None),                  # abrir cofres, cajas y airdrops
    "cofre_cerca": ("Fortnite Chest(sin abrir).mp3", 5.0),      # zumbido de un cofre cercano
    "salto_autobus": ("Saltar del autobus.mp3", 4.5),
    "ala_delta": ("Opertura ala delta.wav", None),
    "francotirador": ("Fortnite fusil de tirador de cerrojomp3.mp3", None),
    "baile": ("Fortnite bailemp3.mp3", None),                   # cruceta arriba (7.7 s, se repite)
}
STEP_SOURCE = "Fortnite Running Sound Effect.wav"


def to_mono_22050(path):
    """Cualquier formato que entienda afconvert (wav, mp3) -> muestras int16 mono 22050 Hz."""
    with tempfile.TemporaryDirectory() as tmp:
        mono = Path(tmp) / "mono.wav"
        subprocess.run(["afconvert", "-f", "WAVE", "-d", "LEI16@22050", "-c", "1", str(path), str(mono)],
                       check=True)
        data, rate = read_wav(mono)
    return data[:, 0].astype(np.int32), rate


def trim_start(x, threshold=0.03, keep=0.004):
    """Quita el silencio inicial (el sonido debe empezar con el suceso)."""
    peak = np.max(np.abs(x)) or 1
    start = int(np.argmax(np.abs(x) > threshold * peak))
    return x[max(0, start - int(keep * 22050)):]


def write_wav(path, x, rate=22050):
    data = np.clip(x, -32768, 32767).astype("<i2")
    with wave.open(str(path), "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(rate)
        w.writeframes(data.tobytes())


def footstep(x):
    """Un unico paso de la grabacion de carrera: el golpe de energia mediana (ni el mas
    flojo ni el mas fuerte), 0.22 s con caida suave. Se dispara en cada pisada."""
    win = int(0.01 * 22050)
    env = np.array([np.sqrt(np.mean(x[i:i + win] ** 2.0)) for i in range(0, len(x) - win, win)])
    threshold = 0.35 * env.max()
    onsets = [i for i in range(1, len(env)) if env[i] > threshold and env[i - 1] <= threshold]
    strengths = [env[i:i + 8].max() for i in onsets]
    chosen = onsets[int(np.argsort(strengths)[len(strengths) // 2])]
    start = max(0, chosen * win - int(0.008 * 22050))
    step = x[start:start + int(0.22 * 22050)].astype(np.float64)
    fade = int(0.06 * 22050)
    step[-fade:] *= np.linspace(1.0, 0.0, fade)
    step *= 0.8 * 32767 / (np.max(np.abs(step)) or 1)
    return step.astype(np.int32), len(onsets)


def sounds():
    out = ASSETS / "sounds"
    out.mkdir(parents=True, exist_ok=True)
    clean = RES / "Sonidos" / "Sonidos Limpios"
    for name, (source, max_seconds) in CLEAN_SOUNDS.items():
        x, rate = to_mono_22050(clean / source)
        x = trim_start(x)
        if max_seconds and len(x) > max_seconds * rate:
            # Recortado con una caida suave al final (los sonidos largos ocupan mucho).
            x = x[:int(max_seconds * rate)].astype(np.float64)
            fade = int(0.4 * rate)
            x[-fade:] *= np.linspace(1.0, 0.0, fade)
            x = x.astype(np.int32)
        write_wav(out / f"{name}.wav", x, rate)
        print(f"  assets/sounds/{name}.wav  {len(x) / rate:.2f} s  (de Sonidos Limpios/{source})")
    x, rate = to_mono_22050(clean / STEP_SOURCE)
    step, found = footstep(x)
    write_wav(out / "paso.wav", step, rate)
    print(f"  assets/sounds/paso.wav  {len(step) / rate:.2f} s  (1 de {found} pasos de {STEP_SOURCE})")

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
