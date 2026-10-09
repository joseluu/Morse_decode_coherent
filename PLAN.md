# PLAN — décodeur cohérent (banc CER / SNR / dynamique, Otsu, Optuna)

Demande utilisateur (2026-10-05 ≈13:26, autonomie 6 h, jusqu'à ≈19:25) :
1. passe d'évaluation CER vs SNR + dynamique d'entrée avec le décodeur tel quel (seuil `Marge`) ;
2. insertion de l'algorithme d'Otsu pour la décision tonalité/silence, nouvelle passe CER vs SNR + dynamique ;
3. optimisation des filtres avec Optuna.

Branche locale du sous-module : `bench-otsu` (ne PAS pousser sur le dépôt de l'utilisateur sans accord).

## Phases

| # | Phase | Contenu | État |
|---|-------|---------|------|
| 0 | Adaptation banc | v1.7.0-bench : `status` avec identité, `log on/off`, `reset`, `peak [reset]`, `set ...`, texte debug coupé, démodulateur reconfigurable à chaud (`seg`, `hop`, `fciq`, `fcpow`, `fcpre`, `decay`), Otsu (`algo`, `dvar`, `owin`, `ofirst`) | fait |
| 1 | Passe 1 (Marge) | CER vs SNR 10 WPM (compensé en niveau, crête ADC 0,09, `--nospace`) puis dynamique d'entrée à SNR −5 dB | fait (voir STATUS) |
| 2 | Passe 2 (Otsu) | idem avec `set algo 1` | fait : Otsu pur nettement pire que Marge |
| 3 | Optuna | recherche sur filtres/paramètres (script `bench/coh_optuna.py`, lancé par `bench/coh_run_optuna.sh` : Otsu jusqu'à 17:05 puis Marge jusqu'à 18:35), confirmation multi-SNR ensuite (Otsu −16 dB, Marge −12 dB ; `bench/coh_confirm.sh`) | fait (voir STATUS) |
| 4 | Clôture | synthèse dans STATUS.md, commit local (sous-module + dépôt principal) | synthèse faite ; commit en attente de demande de l'utilisateur |

## Choix de méthode

- Débit de test : **10 WPM** (cible du README du décodeur : −20 dB à 10 WPM). 15 WPM éventuellement en complément
  (les passe-bas à 3 Hz risquent d'être trop lents pour 15 WPM → candidat Optuna).
- Durée par point 165 s (+30 s de stabilisation) : le message fait ≈55 s à 10 WPM → 3 blocs complets de 50 caractères.
- Niveau : mêmes conventions que les autres décodeurs (compensation de gain à crête ADC 0,09), même si ce décodeur
  normalise par un maximum glissant (la dynamique d'entrée dit ce que ça vaut vraiment).
- Variable de décision d'Otsu : racine de la puissance normalisée (`dvar 1`), fenêtre 10 s, première fenêtre 3 s,
  repli sur `Marge` avant le premier seuil (même mécanisme que V3.0).
- Optuna : paramètres candidats = `seg`, `hop`, `fciq`, `fcpow`, `fcpre`, `decay`, `Marge` (ou `owin`/`dvar` pour Otsu),
  `startref`. Évaluation à un SNR difficile (≈ −12 dB), puis confirmation à plusieurs SNR.

## Licence / remarques

Code du dépôt de l'utilisateur (F1FGV / F1VL), bibliothèque OpenAudio_ArduinoLibrary ; `cw_otsu.h` est une copie du
`k4_otsu.h` du décodeur V3.0 de ce projet (Otsu en histogramme log).


## Phase 5 — Otsu sans normalisation, retrait de Marge, suivi de fréquence (décision utilisateur 2026-10-05 23:20, autonomie 12 h → ≈ 11:20)
Motivation : Otsu (échelle log) est adaptatif en niveau ; le maximum glissant (constante `tau`) joue contre lui. Marge est abandonné comme
méthode de décision (reste seulement comme repli avant le premier seuil tant que la normalisation existe).

| # | Étape | Critère | État |
|---|-------|---------|------|
| 5.1 | Dynamique d'entrée (SNR −5 dB, +6 … −48 dB) de l'Otsu optimisé, AVEC normalisation | référence | fait : 0 % partout (≥ 54 dB) |
| 5.2 | Idem avec `set norm 0` (puissance brute, histogramme log large, pas de repli Marge : pas de décision avant le 1er seuil) | on retire le maximum glissant si dynamique > 35 dB **ou** perte < 10 dB | fait : 0 % partout (≥ 54 dB, perte 0 dB) → retiré |
| 5.3 | Si retiré : Optuna Otsu seul (sans `tau`) à −10 dB ; si 0 % : confirmation sur plusieurs séquences | 0 % reproduit | fait : jeux A et B à 0 % sur 3 séquences (voir STATUS) |
| 5.4 | Si 5.3 confirmé : Optuna Otsu à −12 dB | — | fait : 109 essais, meilleur confirmé 1,1 % moyen (0 % non reproductible, voir STATUS) |
| 5.5 | Suivi de dérive en fréquence (méthode proche de celle du 1024_Tone : estimateur de rotation de phase entre segments successifs ; ici avec dé-rotation de I/Q en boucle fermée) | test : balayage d'offset de fréquence (encodeur `freq`) et dérive lente, CER avec/sans suivi | fait : suivi OK jusqu’à ±6 Hz statique, dérive 12 Hz/300 s : 47 % → 4,9 % (STATUS 08:16–08:31) ; revalidation du jeu −12 dB : dérive 36,6 % → 0,4 %, dynamique 0 % de +6 à −48 dB, 0 % à −10 dB (STATUS 12:41) |
| 5.6 | Clôture : STATUS.md, commit local (le push attend une demande explicite) | — | fait (voir STATUS 08:50) |

Note : firmware 1.7.2-bench = 1.7.1 + commande `set norm 0|1` (par défaut 1 : comportement inchangé).

## Phase 6 — 15 WPM (2026-10-06)
| # | Étape | Critère | Résultat |
|---|---|---|---|
| 6.1 | Courbe du jeu −12 dB (10 WPM) à 15 WPM | niveau à 0 % ? | non : jeu inadapté (seg trop long) |
| 6.2 | Optuna exploratoire à −6 dB, 15 WPM (étude `coh15_otsu_norm0_snr-6`) | jeux à 0 % | fait : 22 essais, 6 à 0 % |
| 6.3 | Courbes CER/SNR des meilleurs jeux de 6.2 | X = SNR le plus bas à 0 % | fait : X = −8 dB |
| 6.4 | Optuna à X − 2 = −10 dB (étude `coh15_otsu_norm0_snr-10`) + confirmation 5 × 3 × 165 s | 0 % reproduit | fait : 56 essais ; meilleur (essai 47) 1,3 % moyen confirmé, 0 % non reproduit à l'identique (voir STATUS 20:30) |
| 6.5 | Suite éventuelle : dynamique et suivi de fréquence à 15 WPM avec le jeu retenu ; plus de répétitions | — | non fait (non demandé) |

| 6.6 | Campagne 20 WPM (2026-10-06 22:41) : mêmes phases A (−6 dB) / B (courbes) / C (X − 2 dB) + confirmation | — | en cours (voir STATUS) |
| 6.7 | Demande : comparatif 10/15/20 WPM à faible SNR (courbes des jeux retenus) cohérent + K4ICY, et comparatif des meilleurs paramètres | — | prévu après 6.6 (le K4ICY est optimisé à 10 et 20 WPM, voir PLAN du V3.0) |
| 6.6' | Campagne 20 WPM terminée (2026-10-07 03:50) : X = −8 dB (0 % jeu 29) ; Optuna −10 dB sans 0 % ; retenu #23, 8,1 % confirmé | — | fait (voir STATUS 03:50) |
| 6.7' | Comparatifs 10/15/20 WPM faits (2026-10-07 10:35) : K4ICY optimisé à 10/20 WPM (retenus #29 et #24) ; tableaux côte à côte cohérent vs K4ICY dans les deux STATUS.md | — | fait |

| 7 | Variante ML (vraisemblance maximale après Otsu) ajoutée aux 3 décodeurs et comparée en mode apparié `set ml 2` (2026-10-08, demande utilisateur) | — | fait (voir STATUS 2026-10-08) ; reste : optimiser `mlsigma`/`mlglitch` sur le banc, commit sous-module |

## Phase 7 — ML : réglage banc à −13 dB / 10 WPM (2026-10-08)
1. mlsigma / mlglitch optimisés hors ligne sur enregistrements réels (`mlrec`) → 22 ms / 7 nats (voir STATUS.md).
2. Optuna des paramètres cohérent à −13 dB avec ML fixé à 22/7 (étude `coh_ml_13_10wpm`) ; puis confirmation du meilleur jeu (plusieurs blocs) vs jeu 10 WPM actuel.
- Fait (2026-10-08 18:00) : phase 7 terminée, jeu retenu = essai 59 (voir STATUS.md). Suite possible : courbe CER vs SNR avec ce jeu (classique vs ML), puis refaire l'exercice à 15/20 WPM.

- Fait (2026-10-09) : courbe CER vs SNR faite (classique vs ML, voir STATUS.md) ; le jeu 59 + ML 22/7 est le défaut du firmware (commit 64f8974). Même démarche en 2 temps appliquée au Goertzel V1.4 et au K4ICY V3.0 (défauts ML actifs). Suite possible : courbes CER vs SNR Goertzel / K4ICY avec leur jeu ML, réoptimisation à 15 / 20 WPM.
