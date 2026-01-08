from scipy import signal
import numpy as np

# === Paramètres principaux ===
f_carrier = 900.0
lowpass_cutoff = 3.0      # Coupure finale sur enveloppe (Hz) – ajuste selon vitesse Morse

# === Choix du facteur de décimation (doit être entier) ===
desired_decimation = 12    # Ex: 15 → fs_resampled = 44100 / 15 = 2940 Hz → f_carrier = 2940 / 4 = 735 Hz
# Autres valeurs courantes : 12, 10, 20, etc.

# === Calcul de la fréquence d'échantillonnage resultant des choix ci-dessus ===
f_sampling = f_carrier * 4.0 * desired_decimation         # Fréquence d'échantillonnage Teensy


print("// Parametres du traitement")
print(f"float32_t AudioCoherentDemod4x_F32::f_carrier = {f_carrier:.3f}f;")
print(f"float32_t AudioCoherentDemod4x_F32::lowpass_cutoff = {lowpass_cutoff:.3f}f;")
print(f"float32_t AudioCoherentDemod4x_F32::f_sampling = {f_sampling:.3f}f;")
print(f"int AudioCoherentDemod4x_F32::decimation_factor = {desired_decimation};")
print("\n")

# === 1. Pré-filtre anti-aliasing : Butterworth 4ème ordre à 2*f_carrier ===
pre_cutoff = 2.0 * f_carrier
sos_pre = signal.butter(4, pre_cutoff / (f_sampling / 2.0), btype='low', output='sos')

print("// Coefficients pré-filtre anti-aliasing (Butterworth 4ème ordre à 2*f_carrier)")
print("float32_t AudioCoherentDemod4x_F32::pre_sos[12] = {")
for i in range(2):
    sec = sos_pre[i]
    print(f"    {sec[0]:.9f}f, {sec[1]:.9f}f, {sec[2]:.9f}f, {-sec[4]:.9f}f, {-sec[5]:.9f}f,")
print("};\n")

# === 2. Filtre Bessel passe-bas à f_carrier (avant décimation) ===
# Ordre 4 pour bon compromis atténuation / retard de groupe
bessel_cutoff = f_carrier
sos_bessel = signal.bessel(4, bessel_cutoff / (f_sampling / 2.0), btype='low', output='sos', norm='delay')

print("// Coefficients filtre Bessel 4ème ordre à f_carrier avant decimation")
print("float32_t AudioCoherentDemod4x_F32::bessel_sos[12] = {")
for i in range(2):
    sec = sos_bessel[i]
    print(f"    {sec[0]:.9f}f, {sec[1]:.9f}f, {sec[2]:.9f}f, {-sec[4]:.9f}f, {-sec[5]:.9f}f,")
print("};\n")

# === 3. Filtre final passe-bas sur enveloppe (Butterworth 2ème ordre) ===
sos_lp = signal.butter(2, lowpass_cutoff / (f_carrier / 2.0), btype='low', output='sos')

print("// Coefficients filtre final enveloppe (Butterworth 2ème ordre)")
print("float32_t AudioCoherentDemod4x_F32::lp_sos[6] = {")
sec = sos_lp[0]
print(f"    {sec[0]:.9f}f, {sec[1]:.9f}f, {sec[2]:.9f}f, {-sec[4]:.9f}f, {-sec[5]:.9f}f")
print("};")
