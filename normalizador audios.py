# -*- coding: utf-8 -*-
"""
Created on Thu Nov 20 19:51:41 2025

@author: fcignoli
"""

import soundfile as sf
from pathlib import Path
import numpy as np

# Carpeta con tus audios
INPUT_DIR = Path(r"C:\Users\felic\OneDrive\Documentos\Proyectos doctorado\Proyectos doctorado\horneros\protocolos 2025\audios\20251119")
OUTPUT_DIR = INPUT_DIR / "15"
OUTPUT_DIR.mkdir(exist_ok=True)

TARGET_PEAK = 0.98  # pico máximo deseado (escala lineal)

# Listar y ordenar archivos
files = sorted(INPUT_DIR.glob("*.wav"))
n_files = len(files)
digits = max(3, len(str(n_files)))  # para 001, 002,... o más si hay >999

mapping_path = OUTPUT_DIR / "mapping.txt"

with mapping_path.open("w", encoding="utf-8") as f_map:
    for i, wav_path in enumerate(files, start=1):
        print("Procesando:", wav_path.name)

        audio, sr = sf.read(wav_path)
        max_amp = np.max(np.abs(audio))

        if max_amp == 0:
            print("  Archivo silencioso, se copia sin cambios.")
            audio_norm = audio
        else:
            gain = TARGET_PEAK / max_amp
            audio_norm = audio * gain
            print(f"  Ganancia aplicada: {gain:.3f}")

        new_name = f"{i:0{digits}d}.wav"
        out_path = OUTPUT_DIR / new_name
        sf.write(out_path, audio_norm, sr)

        # Guardar mapeo en el txt
        f_map.write(f"{new_name} <- {wav_path.name}\n")

print("Listo. Mapping guardado en:", mapping_path)
