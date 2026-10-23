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


SOUNDS = {
    "disparo": disparo, "impacto": impacto, "herido": herido, "eliminacion": eliminacion,
    "pared": pared, "pared_rota": pared_rota, "recarga": recarga, "menu_mover": menu_mover,
    "menu_aceptar": menu_aceptar, "victoria": victoria, "derrota": derrota,
    "pico": pico, "curacion": curacion,  # al final: los anteriores salen identicos
}


# Sustituidos por sonidos dados por el usuario (Recursos/Sonidos/Sonidos Limpios, ver
# tools/prepare_assets.py). Se siguen calculando para no alterar el generador aleatorio:
# asi los demas efectos salen identicos.
REPLACED = {"disparo", "victoria"}


def main():
    for name, fn in SOUNDS.items():
        x = fn()
        if name not in REPLACED:
            save(name, x)


if __name__ == "__main__":
    main()
