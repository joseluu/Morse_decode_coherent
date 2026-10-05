# STATUS — décodeur cohérent (banc)

## 2026-10-05 13:35 — départ
- Sous-module ajouté dans le dépôt principal (`git@github.com:joseluu/Morse_decode_coherent`, base `de18cdc`),
  branche locale `bench-otsu`.
- Firmware **1.7.0-bench** compilé et flashé sur la carte décodeur (17752810) ; identités des deux cartes vérifiées.
- Modifications par rapport à 1.6.1 : voir PLAN.md phase 0 + `AudioCoherentDemodSegmented4x_F32` (méthodes
  `configure`, `reset_state`, `peak_max/min`, passe-bas Butterworth calculés à l'exécution — vérifiés numériquement
  identiques aux coefficients en dur à 1800 Hz). Le filtre de Bessel n'alimente pas la chaîne de puissance
  (sortie d'affichage uniquement) : seul le pré-filtre Butterworth 4e ordre à 1800 Hz est dans le chemin.
- Premier essai : +10 dB, 10 WPM, décodage correct (`URDECODEURDECW CECIESTUNGENERATEURDET`), sans espaces mots.
  Crête ADC (ligne in niveau 0) : gain codeur 0,05 → 0,076.
- Débit message 10 WPM ≈ 55 s/cycle.

## 2026-10-05 14:50 — Passe 1 (seuil Marge 0,21), 10 WPM, firmware 1.7.0
Courbe CER vs SNR (compensée en niveau, crête ADC 0,09, 165 s/pt, 3 blocs de 50 car., sans espaces) —
`data/coh/curve_coh_marge_10wpm.csv` :

| SNR (dB) | +6 | 0 | −5 | −10 | −15 | −20 |
|---|---|---|---|---|---|---|
| CER | 0,0 % | 4,9 % | 13,0 % | 41,5 % | 58,5 % | 76,8 % |

Dynamique d'entrée à SNR −5 dB (niveaux relatifs au gain codeur 0,05, crête ADC 0,80 → 0,009),
`data/coh/level_coh_marge_10wpm.csv` : CER 13,8 / 15,5 / 16,3 / 17,9 / 13,8 / 4,1 / 13,8 / 13,8 % pour +6 … −36 dB
(8 points, 42 dB) — **plate : le décodeur est indépendant du niveau** (normalisation par maximum glissant) sur toute
la plage testée, sans saturation à crête 0,80 ni perte à crête 0,009 (≈ 0,9 % de l'échelle). Dispersion d'un point
≈ ±4 points (1 erreur ≈ 0,7 %). Objectif README (−20 dB à 10 WPM) non atteint avec les réglages d'origine.

## 2026-10-05 14:30 — Passe 2 Otsu : premier départ abandonné
Premier essai (fw 1.7.0) : +6 dB donnait déjà 23 % (vs 0 % en Marge). Diagnostic : le seuil d'Otsu (sur √puissance)
oscille de fenêtre en fenêtre (0,42 … 0,48, premier seuil à 3 s = 0,66) alors que l'enveloppe filtrée à 3 Hz est lente :
quelques % de seuil = plusieurs 10 ms de durée de tonalité. Ajouté fw 1.7.1 : `set omargin` (seuil×(1+m)) et
`set osmooth` (lissage des seuils successifs), neutres par défaut ; passe 2 relancée avec Otsu « pur » (14:53).

## 2026-10-05 15:30 — Passe 2 (Otsu pur : dvar √puissance, fenêtre 10 s, omargin 0), 10 WPM, firmware 1.7.1
`data/coh/curve_coh_otsu_10wpm.csv`, `data/coh/level_coh_otsu_10wpm.csv` :

| SNR (dB) | +6 | 0 | −5 | −10 | −15 | −20 |
|---|---|---|---|---|---|---|
| CER Marge | 0,0 % | 4,9 % | 13,0 % | 41,5 % | 58,5 % | 76,8 % |
| CER Otsu | 29,3 % | 45,1 % | 33,3 % | 28,5 % | 67,5 % | 78,0 % |

Dynamique (SNR −5 dB, +6 … −36 dB) : Otsu 44/27/22/36/44/28/40/34 % vs Marge 14/15/16/18/14/4/14/14 % — indépendante du
niveau dans les deux cas, mais Otsu pur est nettement pire (erreurs essentiellement des substitutions = erreurs de
durée d'éléments, même à +6 dB). Le gain d'Otsu à −10 dB (28 % vs 41 %) est de l'ordre de la dispersion et le CER d'Otsu
n'est pas monotone en SNR : le défaut est structurel (seuil instable d'une fenêtre à l'autre sur une enveloppe lente),
pas lié au bruit. Pistes testées par Optuna : `omargin`, `osmooth`, `owin`, `dvar`.

## 2026-10-05 15:30 — Optuna lancé (bench/coh_run_optuna.sh)
Étude 1 : Otsu (`coh_otsu_10wpm_snr-10`) jusqu'à 17:05 ; étude 2 : Marge (`coh_marge_10wpm_snr-10`) jusqu'à 18:35 ;
puis confirmation multi-SNR des meilleurs jeux (≈18:35–19:20). SNR −10 dB, 10 WPM, 110 s/essai (+20 s stabilisation),
gain codeur 0,0091 (crête ADC ≈ 0,09). Défaut enqueué en premier (seg 2500, hop 500, fciq 3, fcpow 3, fcpre 1800, τ 23 s).

## 2026-10-05 16:25 — Optuna Otsu à −10 dB : saturé
24 essais (study `coh_otsu_10wpm_snr-10`) : 5 essais à **0 % d'erreur** à −10 dB (défaut 34 %), objectif saturé (≈100 caractères
par essai). Tendance : segment long (3500–4500), hop 500–1000, fc I/Q ≈ 4–6,5 Hz, fc puissance ≈ 8–9 Hz (donc **plus rapides**
que 3 Hz d'origine), omargin −0,07…−0,24, osmooth 0,6–0,85, owin 16–29 s, startref 130–200 ms. Étude interrompue à 16:25 et
remplacée par une étude à −16 dB amorcée avec les 6 meilleurs jeux (jusqu'à 17:40), puis étude Marge à −12 dB (jusqu'à 18:35).
Script : `bench/coh_run_optuna2.sh`.

## 2026-10-05 17:45 — Optuna Otsu à −16 dB terminé (34 essais, `coh_otsu_10wpm_snr-16`)
Meilleurs CER à −16 dB (≈100 car./essai, dispersion ±4 pts ; défaut ≈ 60 %) : **39,0 %** (seg 4500, hop 300, fciq 4,0, fcpow 4,8,
fcpre 1271, decay 0,99987, startref 140, owin 13 s, omargin −0,067, osmooth 0,81), 41,5 % (seg 2600, hop 400, fcpow 9,6),
42,7 % (seg 4700, hop 400, fcpow 10,0), 46,3 % (×2). Essais avec fcpow ≈ 1–1,8 Hz : CER 1,0 (≈ 15–35 car. décodés). À −16 dB l'optimum
est moins net qu'à −10 dB (aucun 0 %). Étude Marge à −12 dB démarrée à 17:42 (jusqu'à 18:35), puis `bench/coh_confirm.sh`
lance automatiquement les courbes de confirmation (SNR 0, −10, −15, −20 dB) des meilleurs jeux Otsu et Marge
(`data/coh/curve_coh_opt_{otsu,marge}_10wpm.csv`).

## 2026-10-05 19:10 — Optuna Marge à −12 dB (25 essais, `coh_marge_10wpm_snr-12`) et confirmation multi-SNR
Défaut (seg 2500, hop 500, fciq 3, fcpow 3, fcpre 1800, marge 0,21) : 43,9 % à −12 dB. Meilleurs essais : **0,0 %** (seg 1600,
hop 650, fciq 9,24, fcpow 3,97, fcpre 2585, decay 0,999796, startref 170, marge 0,218), 3,7 % (seg 2500, hop 900, fciq 9,3,
fcpow 4,9, fcpre 2185, startref 180, marge 0,192), 7,3 %, 9,8 %. Tendance : fc I/Q **nettement plus élevée** (≈ 8–9 Hz vs 3), fc puissance
≈ 3–5 Hz, pré-filtre plus large (≈ 2200–2600 Hz), startref 170–200 ms, marge ≈ 0,19–0,26. (Un seul essai à 0 % sur ≈ 97 caractères : à
confirmer, ±4 pts de dispersion.)

Confirmation (`bench/coh_confirm.sh`, 10 WPM, 165 s/point, compensé en niveau, `--nospace`) avec les meilleurs jeux de chaque étude
(`data/coh/curve_coh_opt_{otsu,marge}_10wpm.csv`) :

| SNR (dB) | 0 | −10 | −15 | −20 |
|---|---|---|---|---|
| Marge, défaut (passe 1) | 4,9 % | 41,5 % | 58,5 % | 76,8 % |
| Otsu pur, défaut (passe 2) | 45,1 % | 28,5 % | 67,5 % | 78,0 % |
| **Marge optimisée** (−12 dB) | 0,0 % | 5,7 % | 43,9 % | 74,0 % |
| **Otsu optimisé** (−16 dB) | 0,0 % | **0,0 %** | **26,8 %** | 72,0 % |

Conclusions : (1) l'optimisation des filtres change tout (Marge −10 dB : 41,5 → 5,7 %) ; (2) Otsu réglé (omargin −0,067, osmooth 0,81,
fenêtre 13 s, seg 4500, hop 300, fcpow 4,8) devient meilleur que Marge réglé de −10 à −15 dB, alors qu'il est pire que Marge aux paramètres
d'origine : l'avantage vient surtout du lissage des seuils (osmooth) et du filtrage, moins d'Otsu lui-même ; (3) à −20 dB les deux sont à ≈ 72–74 %
(limite de l'approche à 10 WPM avec ≈ 140 caractères/point). Limites : un seul SNR d'optimisation, ~100 caractères/essai, un seul jeu confirmé par
algorithme ; la dynamique d'entrée n'a pas été re-mesurée avec les jeux optimisés (le décodeur est invariant en niveau grâce au maximum glissant ;
l'impact d'un `decay` plus lent n'a pas été vérifié).

## État de fin de session (19:10)
Décodeur flashé avec Morse_decode_coherent 1.7.1-bench (OOK V2.0 non remis, conforme à la consigne) ; réglages courants en RAM = jeu Marge
optimisé (perdus au reset/flash). Codeur : gain 0,05, identité vérifiée sur les deux cartes. Modifications du sous-module (branche locale
`bench-otsu`) et fichiers `bench/coh_*`, `data/coh/` : **non commités, rien poussé** ; le dépôt de l'utilisateur n'a pas été modifié en amont.
