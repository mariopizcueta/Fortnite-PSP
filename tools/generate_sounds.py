#!/usr/bin/env python3
"""Sintetiza los efectos de sonido originales en Recursos/Sonidos/ (WAV PCM 16 bits, mono, 22050 Hz).

Uso (desde la raiz del proyecto):  python3 tools/generate_sounds.py

Todos los sonidos se generan aqui con osciladores y ruido (nada grabado ni copiado),
cortos y pequenos: el juego los mezcla en la PSP a 44.1 kHz. Formato elegido porque la
PSP lo reproduce sin decodificar (PCM de 16 bits) y ocupa poco a 22 kHz mono.
"""

import math
import wave
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "Recursos" / "Sonidos"
RATE = 22050
rng = np.random.default_rng(1234)  # determinista


def t(seconds):
    return np.arange(int(seconds * RATE)) / RATE


def env(n, attack, decay):
    """Envolvente ataque lineal + caida exponencial (constante `decay` en segundos)."""
    x = np.arange(n) / RATE
    a = np.clip(x / max(attack, 1e-4), 0, 1)
    return a * np.exp(-x / decay)


def lowpass(x, alpha):
    y = np.zeros_like(x)
    acc = 0.0
    for i, v in enumerate(x):
        acc += alpha * (v - acc)
        y[i] = acc
    return y


def square(freq, tt):
    return np.sign(np.sin(2 * math.pi * freq * tt))


def normalize(x, peak=0.7):
    m = np.max(np.abs(x)) or 1.0
    return x / m * peak


def save(name, x):
    data = (np.clip(x, -1, 1) * 32767).astype("<i2")
    OUT.mkdir(parents=True, exist_ok=True)
    with wave.open(str(OUT / f"{name}.wav"), "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(data.tobytes())
    print(f"  Recursos/Sonidos/{name}.wav  {len(data) / RATE:.2f} s  {len(data) * 2 + 44} bytes")


def disparo():
    tt = t(0.30)
    noise = lowpass(rng.uniform(-1, 1, len(tt)), 0.35) * env(len(tt), 0.001, 0.045)
    thump = np.sin(2 * math.pi * (95 - 40 * tt / 0.3) * tt) * env(len(tt), 0.001, 0.07)
    crack = rng.uniform(-1, 1, len(tt)) * env(len(tt), 0.0005, 0.008)
    return normalize(noise * 0.9 + thump * 0.8 + crack * 0.6, 0.85)


def impacto():
    tt = t(0.09)
    tick = np.sin(2 * math.pi * 2300 * tt) * env(len(tt), 0.0005, 0.018)
    tock = square(1150, tt) * env(len(tt), 0.0005, 0.010) * 0.3
    return normalize(tick + tock, 0.6)


def herido():
    tt = t(0.20)
    thud = np.sin(2 * math.pi * (140 - 80 * tt / 0.2) * tt) * env(len(tt), 0.002, 0.06)
    noise = lowpass(rng.uniform(-1, 1, len(tt)), 0.1) * env(len(tt), 0.001, 0.03)
    return normalize(thud + noise * 0.6, 0.7)


def eliminacion():
    out = np.zeros(int(0.5 * RATE))
    for start, freq in ((0.0, 1318.5), (0.11, 1975.5)):
        tt = t(0.38)
        tone = (np.sin(2 * math.pi * freq * tt) + 0.3 * square(freq, tt)) * env(len(tt), 0.002, 0.12)
        i = int(start * RATE)
        out[i:i + len(tone)] += tone[: len(out) - i]
    return normalize(out, 0.6)


def pared():
    tt = t(0.26)
    knock = (np.sin(2 * math.pi * 180 * tt) + 0.6 * np.sin(2 * math.pi * 265 * tt)) * env(len(tt), 0.001, 0.05)
    noise = lowpass(rng.uniform(-1, 1, len(tt)), 0.25) * env(len(tt), 0.001, 0.02)
    return normalize(knock + noise * 0.5, 0.7)


def pared_rota():
    tt = t(0.45)
    crunch = lowpass(rng.uniform(-1, 1, len(tt)), 0.45) * env(len(tt), 0.002, 0.12)
    crackle = (rng.uniform(0, 1, len(tt)) > 0.97) * rng.uniform(-1, 1, len(tt)) * env(len(tt), 0.001, 0.2)
    fall = np.sin(2 * math.pi * (220 - 150 * tt / 0.45) * tt) * env(len(tt), 0.005, 0.15)
    return normalize(crunch + crackle * 0.8 + fall * 0.5, 0.75)


def recarga():
    out = np.zeros(int(0.65 * RATE))
    for start, freq, dur in ((0.0, 900, 0.03), (0.08, 600, 0.04), (0.48, 1200, 0.03), (0.53, 700, 0.05)):
        tt = t(dur * 3)
        click = (square(freq, tt) * 0.5 + rng.uniform(-1, 1, len(tt))) * env(len(tt), 0.0005, dur * 0.4)
        i = int(start * RATE)
        out[i:i + len(click)] += click[: len(out) - i]
    return normalize(out, 0.55)


def menu_mover():
    tt = t(0.05)
    return normalize(square(880, tt) * env(len(tt), 0.001, 0.015), 0.35)


def menu_aceptar():
    tt = t(0.14)
    freq = np.where(tt < 0.06, 660, 990)
    phase = np.cumsum(2 * math.pi * freq / RATE)
    return normalize(np.sign(np.sin(phase)) * env(len(tt), 0.001, 0.05), 0.4)


def arpeggio(notes, step, decay, length):
    out = np.zeros(int(length * RATE))
    for k, freq in enumerate(notes):
        tt = t(length - k * step)
        tone = (square(freq, tt) * 0.4 + np.sin(2 * math.pi * freq * tt)) * env(len(tt), 0.003, decay)
        i = int(k * step * RATE)
        out[i:i + len(tone)] += tone[: len(out) - i]
    return out


def victoria():
    # Do-Mi-Sol-Do' y acorde final: fanfarria corta y original.
    out = arpeggio([523.25, 659.25, 783.99, 1046.5], 0.12, 0.25, 1.6)
    chord = sum(np.sin(2 * math.pi * f * t(1.0)) for f in (523.25, 659.25, 783.99, 1046.5))
    i = int(0.6 * RATE)
    out[i:i + len(chord)] += chord * env(len(chord), 0.01, 0.45) * 0.5
    return normalize(out, 0.7)


def derrota():
    return normalize(arpeggio([523.25, 466.16, 392.0, 311.13], 0.18, 0.35, 1.4), 0.6)


def pico():
    # Golpe seco del pico: un "chac" de madera con un poco de ruido.
    tt = t(0.22)
    chop = (np.sin(2 * math.pi * 320 * tt) + 0.5 * np.sin(2 * math.pi * 760 * tt)) * env(len(tt), 0.0008, 0.035)
    noise = lowpass(rng.uniform(-1, 1, len(tt)), 0.5) * env(len(tt), 0.0005, 0.012)
    return normalize(chop + noise * 0.7, 0.7)


def curacion():
    # Acorde ascendente suave al terminar de usar un consumible.
    return normalize(arpeggio([523.3, 659.3, 784.0, 1046.5], 0.07, 0.18, 0.55), 0.45)


def escopeta():
    # Escopeta de corredera: golpe grave y largo con ruido, y el "clac" de la corredera.
    out = np.zeros(int(0.85 * RATE))
    tt = t(0.45)
    boom = lowpass(rng.uniform(-1, 1, len(tt)), 0.22) * env(len(tt), 0.001, 0.09)
    thump = np.sin(2 * math.pi * (70 - 30 * tt / 0.45) * tt) * env(len(tt), 0.001, 0.12)
    out[:len(tt)] += boom * 1.0 + thump * 0.9
    for start, freq in ((0.55, 700), (0.66, 480)):
        ct = t(0.06)
        click = (square(freq, ct) * 0.5 + rng.uniform(-1, 1, len(ct))) * env(len(ct), 0.0005, 0.012)
        i = int(start * RATE)
        out[i:i + len(click)] += click * 0.5
    return normalize(out, 0.85)


def subfusil():
    # Subfusil con silenciador: disparo corto y apagado ("pff").
    tt = t(0.12)
    puff = lowpass(rng.uniform(-1, 1, len(tt)), 0.18) * env(len(tt), 0.0008, 0.025)
    tick = np.sin(2 * math.pi * 1400 * tt) * env(len(tt), 0.0005, 0.006)
    return normalize(puff + tick * 0.35, 0.6)


def recoger():
    # Recoger un objeto: dos notas cortas ascendentes.
    return normalize(arpeggio([880.0, 1318.5], 0.05, 0.05, 0.16), 0.4)


# --- Parche (fase 8): sonidos que no hay en Recursos/Sonidos -----------------------------------

def pistola():
    # Pistola / revolver: chasquido seco y agudo con cola corta.
    tt = t(0.22)
    crack = lowpass(rng.uniform(-1, 1, len(tt)), 0.55) * env(len(tt), 0.0005, 0.02)
    body = np.sin(2 * math.pi * (180 - 60 * tt / 0.22) * tt) * env(len(tt), 0.001, 0.04)
    return normalize(crack + body * 0.6, 0.8)


def cohete():
    # Lanzacohetes: golpe grave y el silbido del motor que se aleja.
    tt = t(0.7)
    thump = np.sin(2 * math.pi * (60 - 20 * tt / 0.7) * tt) * env(len(tt), 0.002, 0.10)
    hiss = lowpass(rng.uniform(-1, 1, len(tt)), 0.6) * env(len(tt), 0.02, 0.30)
    return normalize(thump + hiss * 0.6, 0.8)


def lanzagranadas():
    # Lanzagranadas: "plop" hueco.
    tt = t(0.25)
    plop = np.sin(2 * math.pi * (240 - 160 * tt / 0.25) * tt) * env(len(tt), 0.001, 0.06)
    air = lowpass(rng.uniform(-1, 1, len(tt)), 0.3) * env(len(tt), 0.001, 0.03)
    return normalize(plop + air * 0.5, 0.75)


def granada_lanzar():
    # Lanzar una granada: el seguro (clic) y el silbido del brazo.
    out = np.zeros(int(0.35 * RATE))
    ct = t(0.04)
    out[:len(ct)] += (square(1500, ct) * 0.5 + rng.uniform(-1, 1, len(ct))) * env(len(ct), 0.0005, 0.008)
    tt = t(0.25)
    whoosh = lowpass(rng.uniform(-1, 1, len(tt)), 0.15) * np.sin(math.pi * np.arange(len(tt)) / len(tt))
    i = int(0.08 * RATE)
    out[i:i + len(whoosh)] += whoosh * 0.8
    return normalize(out, 0.55)


def explosion():
    # Explosion: estallido grave con ruido largo y crepitar.
    tt = t(1.2)
    boom = lowpass(rng.uniform(-1, 1, len(tt)), 0.12) * env(len(tt), 0.002, 0.30)
    sub = np.sin(2 * math.pi * (55 - 25 * tt / 1.2) * tt) * env(len(tt), 0.002, 0.35)
    crackle = (rng.uniform(0, 1, len(tt)) > 0.985) * rng.uniform(-1, 1, len(tt)) * env(len(tt), 0.05, 0.4)
    return normalize(boom * 1.2 + sub + crackle * 0.6, 0.95)


def caja_municion():
    # Caja de municion: el cierre metalico que salta y la tapa con las balas que suenan.
    out = np.zeros(int(0.5 * RATE))
    for start, freq, dur, gain in ((0.0, 1900, 0.025, 0.7), (0.05, 1300, 0.03, 0.6), (0.16, 420, 0.08, 0.9)):
        tt = t(dur * 4)
        hit = (np.sin(2 * math.pi * freq * tt) + 0.4 * rng.uniform(-1, 1, len(tt))) * env(len(tt), 0.0005, dur)
        i = int(start * RATE)
        out[i:i + len(hit)] += hit[: len(out) - i] * gain
    for k in range(5):  # tintineo de los cartuchos
        tt = t(0.05)
        ting = np.sin(2 * math.pi * (2600 + 300 * k) * tt) * env(len(tt), 0.0005, 0.012)
        i = int((0.24 + 0.035 * k) * RATE)
        out[i:i + len(ting)] += ting * 0.25
    return normalize(out, 0.6)


def llama():
    # Llama de suministros: carton que se rompe y una campanilla brillante.
    out = np.zeros(int(1.0 * RATE))
    tt = t(0.3)
    out[:len(tt)] += lowpass(rng.uniform(-1, 1, len(tt)), 0.5) * env(len(tt), 0.001, 0.07)
    chime = arpeggio([1046.5, 1318.5, 1568.0, 2093.0], 0.06, 0.2, 0.75)
    i = int(0.2 * RATE)
    out[i:i + len(chime)] += chime[: len(out) - i] * 0.6
    return normalize(out, 0.6)


def trampa():
    # Trampa de pinchos: muelle metalico que salta ("shhank").
    tt = t(0.4)
    ring = (np.sin(2 * math.pi * 1250 * tt) + 0.6 * np.sin(2 * math.pi * 1870 * tt)) * env(len(tt), 0.001, 0.08)
    scrape = lowpass(rng.uniform(-1, 1, len(tt)), 0.7) * env(len(tt), 0.0005, 0.03)
    clunk = np.sin(2 * math.pi * 140 * tt) * env(len(tt), 0.001, 0.05)
    return normalize(ring * 0.6 + scrape + clunk * 0.7, 0.75)


# --- Recargas por familia de arma (v0.13) -----------------------------------------------------
# Piezas mecanicas: chasquidos metalicos, golpes sordos (cargadores, cartuchos) y deslizamientos.
# Cada una dura algo menos que la recarga mas rapida de su arma (inventory.cpp).


def place(out, start, snd, gain=1.0):
    i = int(start * RATE)
    n = min(len(snd), len(out) - i)
    if n > 0:
        out[i:i + n] += snd[:n] * gain


def clack(freq, dur=0.03, ring=0.4):
    """Chasquido metalico: ruido + un tono que resuena un poco."""
    tt = t(dur * 4)
    tone = np.sin(2 * math.pi * freq * tt) * env(len(tt), 0.0005, dur * ring)
    noise = rng.uniform(-1, 1, len(tt)) * env(len(tt), 0.0003, dur * 0.25)
    return tone * 0.7 + noise


def thunk(freq=180, dur=0.06):
    """Golpe sordo (cargador que entra, cartucho que asienta)."""
    tt = t(dur * 3)
    body = np.sin(2 * math.pi * (freq * (1.0 - 0.4 * tt / (dur * 3))) * tt) * env(len(tt), 0.001, dur * 0.5)
    return body + lowpass(rng.uniform(-1, 1, len(tt)), 0.25) * env(len(tt), 0.0005, dur * 0.2) * 0.8


def slide(dur=0.12, bright=0.35):
    """Metal que se desliza (corredera, cerrojo, cargador que sale)."""
    tt = t(dur)
    shape = np.sin(math.pi * tt / dur) ** 0.6
    return lowpass(rng.uniform(-1, 1, len(tt)), bright) * shape * 0.6


def recarga_fusil():
    out = np.zeros(int(1.75 * RATE))
    place(out, 0.05, clack(900, 0.03))          # boton del cargador
    place(out, 0.08, slide(0.16, 0.25))         # el cargador sale
    place(out, 0.88, slide(0.10, 0.3), 0.7)     # el nuevo entra...
    place(out, 0.98, thunk(220, 0.06))          # ...y asienta
    place(out, 1.00, clack(1300, 0.025), 0.8)
    place(out, 1.35, slide(0.08, 0.45))         # palanca de carga atras
    place(out, 1.43, clack(1600, 0.03))
    place(out, 1.50, clack(1100, 0.03), 0.8)    # y adelante
    return normalize(out, 0.6)


def shell_insert():
    snd = np.zeros(int(0.12 * RATE))
    place(snd, 0.0, slide(0.05, 0.5), 0.5)
    place(snd, 0.045, clack(1900, 0.02, 0.6), 0.8)
    place(snd, 0.05, thunk(320, 0.03), 0.6)
    return snd


def pump(start, out):
    place(out, start, slide(0.09, 0.4))
    place(out, start + 0.09, clack(700, 0.04, 0.6))
    place(out, start + 0.17, slide(0.08, 0.45))
    place(out, start + 0.25, clack(1000, 0.04, 0.6))


def recarga_escopeta():
    # Corredera: un cartucho tras otro y el golpe de la corredera al final.
    out = np.zeros(int(3.7 * RATE))
    for k in range(5):
        place(out, 0.25 + 0.55 * k, shell_insert())
    pump(3.1, out)
    return normalize(out, 0.6)


def recarga_tactica():
    out = np.zeros(int(3.3 * RATE))
    for k in range(6):
        place(out, 0.2 + 0.42 * k, shell_insert())
    pump(2.75, out)
    return normalize(out, 0.6)


def recarga_subfusil():
    out = np.zeros(int(1.5 * RATE))
    place(out, 0.04, clack(1200, 0.025))
    place(out, 0.06, slide(0.10, 0.35))
    place(out, 0.62, slide(0.07, 0.4), 0.6)
    place(out, 0.69, thunk(260, 0.05))
    place(out, 0.71, clack(1500, 0.02), 0.8)
    place(out, 1.05, slide(0.06, 0.5))
    place(out, 1.11, clack(1800, 0.025))
    return normalize(out, 0.55)


def recarga_pistola():
    out = np.zeros(int(1.15 * RATE))
    place(out, 0.03, clack(1400, 0.02))
    place(out, 0.05, slide(0.07, 0.4), 0.7)
    place(out, 0.50, thunk(300, 0.04))
    place(out, 0.52, clack(1700, 0.02), 0.8)
    place(out, 0.85, clack(2100, 0.03, 0.6))   # corredera adelante
    return normalize(out, 0.5)


def recarga_revolver():
    out = np.zeros(int(1.8 * RATE))
    place(out, 0.03, clack(1100, 0.04, 0.6))   # se abre el tambor
    for k in range(6):                          # vainas fuera y balas dentro
        place(out, 0.25 + 0.06 * k, clack(2600 + 120 * k, 0.012, 0.8), 0.35)
    for k in range(3):
        place(out, 0.75 + 0.18 * k, shell_insert(), 0.7)
    place(out, 1.40, clack(900, 0.05, 0.5))    # se cierra
    for k in range(4):                          # y gira
        place(out, 1.50 + 0.04 * k, clack(2000, 0.01, 0.5), 0.4)
    return normalize(out, 0.55)


def recarga_francotirador():
    out = np.zeros(int(2.2 * RATE))
    place(out, 0.05, clack(800, 0.04, 0.6))    # cerrojo arriba
    place(out, 0.12, slide(0.12, 0.35))        # atras
    place(out, 0.26, clack(1200, 0.03))
    place(out, 0.9, shell_insert())            # bala
    place(out, 1.55, slide(0.12, 0.35))        # adelante
    place(out, 1.68, clack(1000, 0.03))
    place(out, 1.78, clack(700, 0.05, 0.6))    # y abajo
    return normalize(out, 0.6)


def recarga_lanzagranadas():
    out = np.zeros(int(2.3 * RATE))
    place(out, 0.05, clack(500, 0.07, 0.6))    # se abre (pesado)
    place(out, 0.08, thunk(120, 0.1))
    for k in range(3):
        place(out, 0.55 + 0.38 * k, thunk(160, 0.08))
        place(out, 0.58 + 0.38 * k, clack(900, 0.03), 0.5)
    place(out, 1.85, clack(600, 0.08, 0.6))    # se cierra
    place(out, 1.88, thunk(140, 0.1))
    return normalize(out, 0.65)


# --- v0.14: modo zombi y agua (al final: los anteriores siguen saliendo identicos) -----------

def zombie_gemido():
    """Gemido grave: diente de sierra con vibrato lento, filtrado y con aspereza."""
    tt = t(1.3)
    f = 95 + 18 * np.sin(2 * math.pi * 1.7 * tt) - 25 * tt
    phase = 2 * math.pi * np.cumsum(f) / RATE
    saw = 2 * ((phase / (2 * math.pi)) % 1.0) - 1
    rough = lowpass(rng.standard_normal(len(tt)), 0.08) * 0.6
    x = lowpass(saw + rough, 0.18) * (0.6 + 0.4 * np.sin(2 * math.pi * 3.1 * tt) ** 2)
    e = np.minimum(1, tt / 0.15) * np.clip((1.3 - tt) / 0.5, 0, 1)
    return normalize(x * e, 0.6)


def zombie_golpe():
    """Zarpazo: golpe sordo y un arañazo."""
    out = np.zeros(int(0.4 * RATE))
    place(out, 0.0, thunk(110, 0.12))
    scratch = lowpass(rng.standard_normal(int(0.18 * RATE)), 0.5) * env(int(0.18 * RATE), 0.01, 0.06)
    place(out, 0.04, scratch, 0.6)
    return normalize(out, 0.75)


def horda():
    """Cuerno grave que anuncia la noche y la horda (3 s)."""
    tt = t(3.0)
    tone = np.sin(2 * math.pi * 73 * tt) + 0.5 * np.sin(2 * math.pi * 110 * tt) + 0.3 * np.sin(2 * math.pi * 146 * tt)
    e = np.minimum(1, tt / 0.6) * np.clip((3.0 - tt) / 1.2, 0, 1)
    wobble = 1 + 0.05 * np.sin(2 * math.pi * 4 * tt)
    return normalize(lowpass(tone * wobble * e, 0.3), 0.7)


def chapoteo():
    """Paso en el agua: ruido filtrado con dos salpicaduras."""
    out = np.zeros(int(0.3 * RATE))
    for k, at in enumerate((0.0, 0.07)):
        n = int(0.2 * RATE)
        splash = lowpass(rng.standard_normal(n), 0.35 - 0.1 * k) * env(n, 0.005, 0.05)
        place(out, at, splash, 1.0 - 0.4 * k)
    return normalize(out, 0.55)


def fabricar():
    """Martillazos y una carraca: algo fabricado en el banco."""
    out = np.zeros(int(0.8 * RATE))
    for k in range(3):
        place(out, 0.02 + 0.16 * k, clack(700 + 120 * k, 0.05, 0.6))
        place(out, 0.02 + 0.16 * k, thunk(220, 0.05), 0.6)
    for k in range(5):
        place(out, 0.52 + 0.045 * k, clack(1800, 0.015, 0.3), 0.5)
    return normalize(out, 0.65)


def amanecer():
    """Campanita suave: se ha sobrevivido a la noche."""
    return normalize(arpeggio([523, 659, 784, 1046], 0.16, 0.5, 1.6), 0.5)


SOUNDS = {
    "disparo": disparo, "impacto": impacto, "herido": herido, "eliminacion": eliminacion,
    "pared": pared, "pared_rota": pared_rota, "recarga": recarga, "menu_mover": menu_mover,
    "menu_aceptar": menu_aceptar, "victoria": victoria, "derrota": derrota,
    "pico": pico, "curacion": curacion,  # al final: los anteriores salen identicos
    "escopeta": escopeta, "subfusil": subfusil, "recoger": recoger,
    # Parche (fase 8): al final, para que los anteriores sigan saliendo identicos.
    "pistola": pistola, "cohete": cohete, "lanzagranadas": lanzagranadas, "granada_lanzar": granada_lanzar,
    "explosion": explosion, "caja_municion": caja_municion, "llama": llama, "trampa": trampa,
    # v0.13: recargas por familia (al final: los anteriores siguen saliendo identicos).
    "recarga_fusil": recarga_fusil, "recarga_escopeta": recarga_escopeta, "recarga_tactica": recarga_tactica,
    "recarga_subfusil": recarga_subfusil, "recarga_pistola": recarga_pistola, "recarga_revolver": recarga_revolver,
    "recarga_francotirador": recarga_francotirador, "recarga_lanzagranadas": recarga_lanzagranadas,
    # v0.14: modo zombi y agua (al final, por lo mismo).
    "zombie_gemido": zombie_gemido, "zombie_golpe": zombie_golpe, "horda": horda, "chapoteo": chapoteo,
    "fabricar": fabricar, "amanecer": amanecer,
}


# Sustituidos por sonidos dados por el usuario (Recursos/Sonidos/Sonidos Limpios, ver
# tools/prepare_assets.py). Se siguen calculando para no alterar el generador aleatorio:
# asi los demas efectos salen identicos. v0.13: la pistola y el cohete ya tienen grabacion.
REPLACED = {"disparo", "victoria", "pistola", "cohete"}


def main():
    for name, fn in SOUNDS.items():
        x = fn()
        if name not in REPLACED:
            save(name, x)


if __name__ == "__main__":
    main()
