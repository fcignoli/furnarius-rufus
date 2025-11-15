# -*- coding: utf-8 -*-
"""
Created on Tue Nov 11 19:19:21 2025

@author: felic
"""

# === PROTOCOLO: CON -> GAP -> 15 estímulos (5×stim1, 5×stim2, 5×stim3) en orden aleatorio ===
# Pegar y ejecutar en UNA SOLA CELDA (Spyder/Jupyter).
# Requiere: numpy, soundfile, scipy

# ---------------- CONFIG ----------------
from pathlib import Path

BASE = Path(__file__).parent if '__file__' in globals() else Path.cwd()


CON_PATH   = "canto_con.wav"
STIM1_PATH = "11_40rep_f.wav"
STIM2_PATH = "12_40rep_f.wav"
STIM3_PATH = "13_40rep_f.wav"


OUT_WAV = BASE / "00000003.wav"
OUT_CSV = BASE / "protocolo3_timeline.csv"


TARGET_SR   = 44100   # Hz (se re-muestrea si hace falta)
STIM_DUR_S  = 5.0     # s (cada estímulo se recorta/padea a este valor)
GAP_S       = 15.0    # s (silencio fijo entre ítems, incluido después del CON)
CON_DUR_S   = None    # s (None = usar duración original del CON; poné un número para forzar trim/pad)
PEAK_LEVEL  = 0.95    # normalización peak
EXPORT_STEREO = False # True duplica a 2 canales
RANDOM_SEED = 42      # para orden reproducible. Cambiá o poné None para variar cada corrida.
# ---------------------------------------

import csv, random
from pathlib import Path
import numpy as np
import soundfile as sf
from scipy.signal import resample_poly

# ---------- utilidades de audio ----------
def load_audio(path, target_sr=None):
    x, sr = sf.read(str(path), always_2d=False)
    if x.dtype != np.float32:
        x = x.astype(np.float32, copy=False)
    if x.ndim == 2:
        x = x.mean(axis=1).astype(np.float32, copy=False)
    if (target_sr is not None) and (sr != target_sr):
        g = np.gcd(sr, target_sr)
        up, down = target_sr // g, sr // g
        x = resample_poly(x, up, down).astype(np.float32, copy=False)
        sr = target_sr
    return x, sr

def fade_edges(x, sr, fadems=5.0):
    n = len(x)
    if n == 0: 
        return x
    w = max(1, min(int(sr * fadems / 1000.0), n//2))
    ramp = 0.5 * (1 - np.cos(np.linspace(0, np.pi, w, dtype=np.float32)))
    y = x.copy()
    y[:w]  *= ramp
    y[-w:] *= ramp[::-1]
    return y

def fit_to_duration(x, sr, target_sec, fade_ms=5.0):
    target_n = int(round(target_sec * sr))
    if len(x) > target_n:
        y = x[:target_n]
    elif len(x) < target_n:
        y = np.pad(x, (0, target_n - len(x)), mode="constant")
    else:
        y = x
    return fade_edges(y, sr, fade_ms)

def normalize_peak(x, peak=0.95):
    m = float(np.max(np.abs(x))) if x.size else 1.0
    return (x / m * peak).astype(np.float32) if m > 0 else x.astype(np.float32)

def silence(sr, sec): 
    return np.zeros(int(round(sr*sec)), dtype=np.float32)

def stereoize(x, stereo): 
    return np.stack([x, x], axis=1) if stereo else x

# ---------- construcción del protocolo ----------
if RANDOM_SEED is not None:
    random.seed(RANDOM_SEED)

sr = int(TARGET_SR)

# Cargar CON
con, _ = load_audio(CON_PATH, target_sr=sr)
if CON_DUR_S is not None:
    con = fit_to_duration(con, sr, CON_DUR_S, fade_ms=5.0)
con = normalize_peak(con, peak=PEAK_LEVEL)

# Cargar y preparar los 3 estímulos base
base = []
for lab, pth in [("S1", STIM1_PATH), ("S2", STIM2_PATH), ("S3", STIM3_PATH)]:
    x, _ = load_audio(pth, target_sr=sr)
    x = fit_to_duration(x, sr, STIM_DUR_S, fade_ms=5.0)
    x = normalize_peak(x, peak=PEAK_LEVEL)
    base.append((lab, x))

# Repetir 5 veces cada uno y barajar
items = []
for lab, x in base:
    for i in range(1, 6):
        items.append((f"{lab}_{i:02d}", lab, x.copy()))
random.shuffle(items)

# Ensamblar: CON -> gap -> (stim, gap, stim, ... sin gap final)
events = []
timeline = []
t = 0.0

events.append(stereoize(con, EXPORT_STEREO))
timeline.append({"start_s": t, "label": "CON", "class": "CON", "duration_s": len(con)/sr})
t += len(con)/sr

if items:
    g = silence(sr, GAP_S)
    events.append(stereoize(g, EXPORT_STEREO))
    timeline.append({"start_s": t, "label": "SILENCE", "class": "gap", "duration_s": GAP_S})
    t += GAP_S

for k, (label, cls, x) in enumerate(items):
    events.append(stereoize(x, EXPORT_STEREO))
    timeline.append({"start_s": t, "label": label, "class": cls, "duration_s": len(x)/sr})
    t += len(x)/sr
    if k < len(items) - 1:
        g = silence(sr, GAP_S)
        events.append(stereoize(g, EXPORT_STEREO))
        timeline.append({"start_s": t, "label": "SILENCE", "class": "gap", "duration_s": GAP_S})
        t += GAP_S

audio = np.concatenate(events, axis=0)
audio = np.clip(audio, -0.999, 0.999).astype(np.float32)
total_s = audio.shape[0] / sr

# Guardar WAV y CSV
sf.write(str(Path(OUT_WAV).with_suffix(".wav")), audio, sr)

with open(Path(OUT_CSV).with_suffix(".csv"), "w", newline="") as f:
    cols = ["start_s","label","class","duration_s","sr","stereo","gap_s","stim_dur_s"]
    w = csv.DictWriter(f, fieldnames=cols); w.writeheader()
    for row in timeline:
        w.writerow({**row, "sr": sr, "stereo": EXPORT_STEREO, "gap_s": GAP_S, "stim_dur_s": STIM_DUR_S})

# Reporte
mm = int(total_s // 60); ss = int(round(total_s % 60))
print(f"[OK] Guardado WAV: {Path(OUT_WAV).with_suffix('.wav')}")
print(f"[OK] Guardado CSV: {Path(OUT_CSV).with_suffix('.csv')}")
print(f"Duración total ≈ {mm} min {ss} s  |  SR={sr} Hz  |  GAP={GAP_S}s  |  STIM={STIM_DUR_S}s")
print("Primeros 6 eventos:")
for r in timeline[:6]:
    print(r)
