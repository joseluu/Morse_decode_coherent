// Seuil d'Otsu sur histogramme log-espace pour la decision tonalite/silence (copie de k4_otsu.h du decodeur V3.0,
// lui-meme issu de Morse_decode_V_1_4_FFT_1024_Tone/log_histogram.h + otsu_threshold.cpp).
// Variable decisionnelle : puissance normalisee du demodulateur coherent (ou sa racine).
#ifndef CW_OTSU_H
#define CW_OTSU_H

#include <Arduino.h>
#include <math.h>
#include <string.h>

#define CW_HIST_BINS 512

struct CWOtsu {
  uint32_t counts[CW_HIST_BINS];
  float lmin = -3.0f, lmax = 4.0f;   // log10 des bornes de l'histogramme
  uint32_t startMs = 0;
  uint32_t windowMs = 10000;         // fenetre de collecte (apres la premiere)
  uint32_t firstWindowMs = 3000;     // la premiere fenetre est courte pour obtenir vite un seuil
  bool valid = false;                // un seuil est disponible
  bool first = true;
  float threshold = 0, mean0 = 0, mean1 = 0;
  uint32_t count0 = 0, count1 = 0;
  uint32_t windows = 0;

  void setRange(float minV, float maxV) { lmin = log10f(minV); lmax = log10f(maxV); }
  void reset() {            // etat de mise sous tension : pas de seuil, histogramme vide
    memset(counts, 0, sizeof(counts));
    valid = false; first = true; threshold = mean0 = mean1 = 0; count0 = count1 = 0; windows = 0;
    startMs = millis();
  }
  float step() const { return (lmax - lmin) / (float)CW_HIST_BINS; }
  float center(int i) const { return powf(10.0f, lmin + ((float)i + 0.5f) * step()); }
  float edge(int i) const { return powf(10.0f, lmin + (float)i * step()); }
  int index(float v) const {
    if (!(v > 1e-30f)) return 0;
    int i = (int)((log10f(v) - lmin) / step());
    if (i < 0) i = 0;
    if (i >= CW_HIST_BINS) i = CW_HIST_BINS - 1;
    return i;
  }
  void add(float v) {
    counts[index(v)]++;
    if (millis() - startMs >= (first ? firstWindowMs : windowMs)) compute();
  }
  void compute() {
    uint32_t total = 0;
    float sumAll = 0;
    for (int i = 0; i < CW_HIST_BINS; i++) { total += counts[i]; sumAll += (float)counts[i] * center(i); }
    uint32_t w0 = 0;
    float sum0 = 0, bestVar = -1;
    int bestT = -1;
    for (int t = 0; t < CW_HIST_BINS - 1 && total > 0; t++) {
      w0 += counts[t];
      sum0 += (float)counts[t] * center(t);
      uint32_t w1 = total - w0;
      if (w0 == 0 || w1 == 0) continue;
      float m0 = sum0 / (float)w0, m1 = (sumAll - sum0) / (float)w1;
      float bv = (float)w0 * (float)w1 * (m1 - m0) * (m1 - m0);
      if (bv > bestVar) { bestVar = bv; bestT = t; }
    }
    if (bestT >= 0) {
      w0 = 0; sum0 = 0;
      for (int t = 0; t <= bestT; t++) { w0 += counts[t]; sum0 += (float)counts[t] * center(t); }
      count0 = w0; count1 = total - w0;
      mean0 = count0 ? sum0 / (float)count0 : 0;
      mean1 = count1 ? (sumAll - sum0) / (float)count1 : 0;
      threshold = edge(bestT + 1);
      valid = true;
      windows++;
    }
    memset(counts, 0, sizeof(counts));
    first = false;
    startMs = millis();
  }
};

#endif
