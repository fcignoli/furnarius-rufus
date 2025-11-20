# -*- coding: utf-8 -*-
"""
Created on Wed Nov 19 16:14:17 2025

@author: felic
"""

import soundfile as sf
import os

DURACION_SEG = 5.0  # segundos

def recortar_a_5s(input_path, output_path=None):
    # Cargar audio
    data, sr = sf.read(input_path)

    # Calcular cantidad de muestras para 5 segundos
    n_muestras_5s = int(DURACION_SEG * sr)

    # Si el audio es más corto, se usa lo que haya
    n_muestras_5s = min(len(data), n_muestras_5s)

    recorte = data[:n_muestras_5s]

    # Nombre de salida por defecto
    if output_path is None:
        base, ext = os.path.splitext(input_path)
        output_path = base + "_5s.wav"

    # Guardar archivo recortado
    sf.write(output_path, recorte, sr)
    print(f"Archivo recortado guardado en: {output_path}")

if __name__ == "__main__":
    # Cambiá esta ruta por la de tu archivo
    input_file = "C:\Users\felic\OneDrive\Documentos\Proyectos doctorado\Proyectos doctorado\horneros\protocolos 2025\audios\13_60rep_f.wav"
    recortar_a_5s(input_file)
