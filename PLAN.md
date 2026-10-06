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
