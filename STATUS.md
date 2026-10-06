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

## 2026-10-06 00:25 — Phase 5.1/5.2 : dynamique d'entrée de l'Otsu optimisé, avec et sans maximum glissant (firmware 1.7.2-bench)
Jeu Otsu retenu (seg 4500, hop 300, fciq 3,99, fcpow 4,82, fcpre 1271, startref 140, dvar 1, owin 13 s, omargin −0,067, osmooth 0,81),
SNR −5 dB, 10 WPM, 110 s/point, niveaux +6 … −48 dB par pas de 6 (gain encodeur 0,0998 … 0,0002), `data/coh/level_coh_opt_norm{1,0}_10wpm.csv` :

| Niveau (dB) | +6 | 0 | −6 | −12 | −18 | −24 | −30 | −36 | −42 | −48 |
|---|---|---|---|---|---|---|---|---|---|---|
| CER, avec normalisation (`norm 1`) | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| CER, sans normalisation (`norm 0`) | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |

(CER en %, ≈ 95–98 caractères par point.) Dynamique (CER ≤ 10 %) : **≥ 54 dB dans les deux cas** (la mesure s'arrête à −48 dB, le
bas de la plage n'est donc pas atteint) ; perte due au retrait du maximum glissant : **0 dB**. Critère (dynamique > 35 dB ou perte
< 10 dB) rempli → **le maximum glissant est retiré** (`norm 0` : puissance brute, histogramme log 1e-6…2 sur √puissance, pas de repli Marge :
aucune décision avant le premier seuil Otsu à 3 s). Marge est abandonné comme méthode de décision.
Nota : l'Otsu aux paramètres d'origine (non optimisé) était déjà indépendant du niveau (passe 2) ; ici le jeu optimisé le reste.
Optuna Otsu sans `tau` à −10 dB lancé à 00:09 (`coh_otsu_norm0_snr-10`, amorcé avec les 8 meilleurs jeux des études précédentes).

## 2026-10-06 01:21 — Optuna Otsu sans normalisation à −10 dB : saturé, arrêté
31 essais (`coh_otsu_norm0_snr-10`, ≈ 95 caractères/essai) dont **16 à 0 %** et 19 à ≤ 3 % (défaut : 43,9 %). Les 8 jeux issus des études
précédentes (amorçage) sont presque tous à 0 % sans normalisation : la normalisation n'apportait rien. L'étude est interrompue (objectif saturé) ;
`bench/coh_confirm_multi.py` re-mesure les 5 meilleurs jeux 3 fois × 165 s à −10 dB (nouvelle séquence/bruit à chaque répétition), puis étude à −12 dB (jusqu'à 06:15).

## 2026-10-06 02:17 — Phase 5.3 : confirmation multi-séquences à −10 dB, sans normalisation (`data/coh/confirm_norm0_snr-10.{csv,json}`)
5 meilleurs jeux de l'étude, 3 répétitions × 165 s chacune (reset + nouvelle réalisation du bruit), 10 WPM, ≈ 125 caractères par mesure. CER (%) :

| Jeu | rép. 1 | rép. 2 | rép. 3 |
|---|---|---|---|
| A : seg 4900, hop 700, fciq 6,25, fcpow 9,20, fcpre 1427, startref 200, owin 28 s, omargin −0,077, osmooth 0,65 | 0 | 0 | **0** |
| B : seg 4500, hop 300, fciq 3,98, fcpow 4,82, fcpre 1272, startref 140, owin 13 s, omargin −0,067, osmooth 0,81 (jeu retenu de l'étude −16 dB) | 0 | 0 | **0** |
| C : seg 2600, hop 400, fciq 3,33, fcpow 9,62, fcpre 1733, startref 100, owin 24 s, omargin −0,160, osmooth 0,73 | 0 | 2,4 | 0,8 |
| D : seg 5600, hop 800, fciq 5,78, fcpow 8,82, fcpre 1799, startref 200, owin 23 s, omargin −0,249, osmooth 0,87 | 0 | 0 | 1,6 |
| E : seg 4800, hop 1000, fciq 5,62, fcpow 8,46, fcpre 1582, startref 200, owin 29 s, omargin −0,185, osmooth 0,85 | 0,8 | 0,8 | 0 |

→ **0 % confirmé sur 3 séquences indépendantes à −10 dB** pour A et B (≈ 375 caractères chacun), sans maximum glissant. Étude Optuna à −12 dB
(`coh_otsu_norm0_snr-12`) démarrée à ≈ 02:11, amorcée avec ces jeux (le jeu B y donne 2,4 % au premier essai) ; arrêt prévu à 06:15.

## 2026-10-06 07:17 — Phase 5.4 : Optuna −12 dB (norm 0) et confirmation multi-séquences (`data/coh/optuna_coh_otsu_norm0_snr-12.*`, `confirm_norm0_snr-12.{csv,json,log}`)
- Étude : 109 essais (02:08 → 06:15), amorcée avec 4 jeux confirmés à −10 dB. ≈ 12 essais à 0 % sur une mesure unique de ≈ 97 caractères
  (dispersion ±4 points → ces 0 % isolés sont en grande partie de la chance de tirage).
- Confirmation : 5 meilleurs jeux × 3 répétitions × 165 s (≈ 145 caractères chacune). CER moyen / max :

| Rang | seg/hop | fciq/fcpow | owin | omargin/osmooth | CER moyen | CER max |
|---|---|---|---|---|---|---|
| 1 | 4700/650 | 5,75/7,48 | 8000 | −0,120/0,77 | **1,08 %** | 3,25 % |
| 2 | 4400/800 | 7,97/4,98 | 5000 | −0,058/0,86 | 1,63 % | 4,07 % |
| 3 | 4400/650 | 4,80/10,23 | 10000 | −0,053/0,81 | 1,90 % | 4,07 % |
| 4 | 3900/650 | 9,64/4,22 | 9000 | −0,033/0,88 | 2,17 % | 5,69 % |
| 5 | 4300/650 | 6,06/12,96 | 5000 | −0,099/0,76 | 7,05 % | 8,13 % |

→ **0 % n'est pas atteint de façon reproductible à −12 dB** (meilleur : 1,1 % en moyenne, 0 % sur 1 répétition sur 3 environ) ; −10 dB reste la limite
« 0 % confirmé » (jeux A et B). Pour les essais de suivi de fréquence on utilise le jeu B (−10 dB, 0/0/0 %).

## 2026-10-06 08:16 — Phase 5.5 : suivi de fréquence, essai statique (firmware 1.7.2-bench avec suivi, jeu B, norm 0, SNR −5 dB, 10 WPM, gain 0,0091)
Mesure `bench/coh_freq.py static` (`data/coh/freq_static.{csv,log}`), 110 s par point (~97 caractères, dispersion ±4 points). `foff` = indicateur de décalage (Hz) lu par
la commande `foff`, `fcorr` = correction appliquée par le suivi (Hz).

| Écart encodeur | track 0 : CER / foff | track 1 : CER / foff / fcorr |
|---|---|---|
| 0 Hz | 0 % / +0,1 | 0 % / 0,0 / −0,06 |
| +3 Hz | 2,4 % / +2,7 | 0 % / +2,5 / +2,48 |
| −3 Hz | 0 % / −2,6 | 2,4 % / −2,4 / −2,21 |
| +6 Hz | **100 %** (29 car.) / +5,2 | **0 %** / +5,1 / +4,75 |
| −6 Hz | **100 %** (23 car.) / −5,3 | **0 %** / −5,3 / −5,39 |
| +10 Hz | 77 % / +6,7 | 34 % / +7,5 / +7,91 |
| −10 Hz | 78 % / −7,5 | 59 % / −7,2 / −6,21 |
| +15 Hz | 76 % / +5,2 | 74 % / +5,5 / +6,19 |
| −15 Hz | 74 % / −4,9 | 76 % / −4,3 / −4,35 |

Conclusions : (1) sans suivi la tolérance statique est ≈ ±3 Hz ; (2) le suivi la porte à **≥ ±6 Hz** (0 %) ; (3) au-delà (±10, ±15 Hz) l'indicateur est
sous-estimé (fenêtre de Hann de 100 ms : réponse atténuée vers la région nulle à ≈ 19 Hz, portage par le seuil de porte 0,3·pic) et le suivi ne converge pas
complètement. La plage utile de l'indicateur sans suivi va jusqu'à ≈ ±6 Hz (lecture fidèle à ≈ 10 %), au-delà elle sature vers ≈ 5–7 Hz.
Essai de dérive progressive (12 Hz sur 300 s) lancé à 08:16.

## 2026-10-06 08:31 — Phase 5.5 : dérive progressive 0 → +12 Hz sur 300 s (`data/coh/freq_drift.{csv,log}`, jeu B, SNR −5 dB)
| Suivi | CER | Caractères | fcorr final | foff final | Observation |
|---|---|---|---|---|---|
| track 0 | **47,3 %** | 355 | — | +7,4 (sature) | texte propre ≈ 1 min puis décrochage (≈ +3…4 Hz d'écart) : sortie en E/T parasites |
| track 1 | **4,9 %** | 264 | +5,2 | +6,2 | texte propre sur ≈ 4 min ; erreurs seulement dans la dernière minute (écart > ≈ 10 Hz, hors plage d'acquisition) |

→ le suivi de fréquence divise le CER de la dérive par 10 et prolonge le décodage correct de ≈ 1 min (écart ≈ 3 Hz) à ≈ 4 min (écart ≈ 9–10 Hz). Limite : la fenêtre de Hann de
100 ms atténue le phaseur au-delà de ≈ ±8 Hz et l'indicateur sature (≈ 7 Hz) ; le suivi suit une dérive lente (≈ 0,04 Hz/s ici) mais ne rattrape pas un décalage statique > ≈ 8 Hz.
Comportement par défaut (track 0) : non modifié.

## 2026-10-06 08:50 — Phase 5.5 : variante porte 0,1 rejetée ; clôture phase 5 (5.6)
- Porte 0,3·pic → 0,1·pic (`data/coh/freq_static_gate01.{csv,log}`, track 1, −5 dB) : +10 Hz 59 %, −10 Hz 51 %, +15 Hz 77 %, −15 Hz 78 % (vs 34/59/74/76 % avec 0,3) → **aucune amélioration, 0,3 conservé** (firmware reflashé avec 0,3 à 08:52).
  La plage d'acquisition (≈ ±6…8 Hz) est limitée par la fenêtre de Hann de seg 4500 (100 ms), pas par le seuil de porte.
- État final : firmware `Morse_decode_coherent` 1.7.2-bench sur le décodeur (défauts : algo 0, norm 1, track 0 → comportement historique ; jeu Otsu/norm 0 à appliquer par `set`).
  Les deux cartes vérifiées par by-id (décodeur 17752810 : 1.7.2-bench ; codeur 17765010 : gain 0,05). Code (tracking, norm) non commité dans le sous-module ; aucun push.
- Synthèse phase 5 : abandon de la marge et du maximum glissant (dynamique ≥ 54 dB inchangée) ; Otsu seul 0 % confirmé à −10 dB (jeux A, B), ≈ 1 % à −12 dB ; suivi de fréquence : ±3 → ±6 Hz statique, dérive 12 Hz/300 s 47 % → 4,9 %.

## 2026-10-06 — Synthèse des campagnes Optuna : paramètres, zones de recherche, valeurs retenues
Script `bench/coh_optuna.py` (TPE d'Optuna, graine 1, `n_startup` 12 ; défaut enqueué en essai 0 puis jeux d'amorçage). 10 WPM, gain codeur 0,0091 (crête ADC ≈ 0,09), 110 s par essai + 20 s de stabilisation
(≈ 95–100 caractères, dispersion ≈ ±4 points ; `--nospace`). Unités : `seg`, `hop` en échantillons (fs 43 200 Hz) ; `owin` en ms ; `fc*` en Hz ; `decay` = exp(−(hop/43200)/τ).
Le défaut est celui du firmware d'origine (seg 2500, hop 500, fciq 3, fcpow 3, fcpre 1800, τ 23,1 s, startref 100, marge 0,21 ; Otsu : owin 10 000, omargin 0, osmooth 0).

| Paramètre | Zone de recherche | Étude 1 Otsu −10 dB (norm 1, 24 essais) | Étude 2 Otsu −16 dB (norm 1, 34 essais) | Étude 3 Marge −12 dB (norm 1, 25 essais) | Étude 4 Otsu −10 dB (norm 0, 31 essais) | Étude 5 Otsu −12 dB (norm 0, 109 essais) |
|---|---|---|---|---|---|---|
| `seg` | entier 800 … 6000, pas 100 | 3600 | 4500 | 1600 | A 4900 · B 4500 | **4700** |
| `hop` (≤ seg) | entier 100 … 1000, pas 50 | 750 | 300 | 650 | A 700 · B 300 | **650** |
| `tau` (→ `decay`) | 2 … 120 s, log (seulement si norm 1 ; sinon fixe, sans effet) | 7,3 s | 53,8 s | 73,7 s | sans objet (norm 0) | sans objet |
| `fciq` (passe-bas I/Q) | 0,8 … 12 Hz, log | 5,13 | 3,98 | 9,24 | A 6,25 · B 3,98 | **5,75** |
| `fcpow` (passe-bas puissance) | 0,8 … 14 Hz, log | 8,72 | 4,82 | 3,97 | A 9,20 · B 4,82 | **7,48** |
| `fcpre` (pré-filtre) | 1000 … 4000 Hz, log | 1026 | 1272 | 2585 | A 1427 · B 1272 | **1190** |
| `startref` | entier 60 … 200, pas 10 | 170 | 140 | 170 | A 200 · B 140 | **200** |
| `marge` (algo 0 seulement) | 0,03 … 0,5 | — | — | 0,218 | — | — |
| `dvar` (algo 1) | entier 0 … 1 | 1 | 1 | — | 1 | **1** |
| `owin` (algo 1, fenêtre Otsu) | entier 5000 … 30 000 ms, pas 1000 | 24 000 | 13 000 | — | A 28 000 · B 13 000 | **8000** |
| `omargin` (algo 1) | −0,3 … +0,3 | −0,132 | −0,067 | — | A −0,077 · B −0,067 | **−0,120** |
| `osmooth` (algo 1) | 0 … 0,9 | 0,710 | 0,808 | — | A 0,646 · B 0,808 | **0,766** |
| CER du meilleur essai (mesure de recherche) | — | 0 % (−10 dB) | 39 % (−16 dB) | 0 % (−12 dB) | 0 % (×16 essais) | 0 % (×18 essais) |
| Confirmation indépendante | — | — | courbe : 0 % à −10 dB, 26,8 % à −15 | courbe : 5,7 % à −10, 43,9 % à −15 | A, B : 0/0/0 % à −10 dB (3×165 s) | **1,1 % moyen à −12 dB** (3×165 s) |

Notes : les valeurs « meilleur essai » des études 1–3 sont celles du meilleur essai brut ; pour les études 4–5, les valeurs retenues viennent de la **confirmation multi-séquences**
(5 meilleurs jeux × 3 répétitions × 165 s), car des dizaines d'essais à 0 % sur ≈ 97 caractères ne discriminent plus. Études 4 et 5 : sans normalisation (`set norm 0`, plus de τ), amorcées avec les jeux
des études précédentes. L'étude Marge à −10 dB prévue initialement n'a pas été menée (remplacée par la Marge à −12 dB).

### Jeu retenu à −12 dB (Otsu, `norm 0`, 10 WPM)
Meilleur jeu confirmé (CER moyen 1,08 %, max 3,25 % sur 3 × 165 s à −12 dB ; 0 % attendu à −10 dB, à revérifier : voir revalidation) :
`set norm 0;set algo 1;set seg 4700;set hop 650;set fciq 5.749;set fcpow 7.480;set fcpre 1190.2;set decay 0.999349;set startref 200;set dvar 1;set owin 8000;set omargin -0.120;set osmooth 0.766`
(copie dans `data/coh/freq_pre_best12.txt`). Non adopté comme défaut du firmware (les défauts restent `algo 0`, `norm 1`, `track 0`) ; à appliquer par `set` après chaque flash ou reset.
Revalidation de ce jeu (courbe CER/SNR, dynamique, suivi de fréquence) lancée à 11:27 (`bench/coh_chain5.sh`, `data/coh/campaign_reval12.log`).
