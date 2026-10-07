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

## 2026-10-06 12:41 — Revalidation du jeu retenu à −12 dB (norm 0, Otsu ; firmware 1.7.2-bench ; `bench/coh_chain5.sh`, `data/coh/campaign_reval12.log`)
Jeu : seg 4700, hop 650, fciq 5,749, fcpow 7,480, fcpre 1190,2, startref 200, owin 8000, omargin −0,120, osmooth 0,766 (`data/coh/freq_pre_best12.txt`). 10 WPM.

**Courbe CER vs SNR** (niveau compensé, crête ADC 0,09, 165 s/point, une passe, `--nospace`, `data/coh/curve_coh_best12_10wpm.csv`) :
| SNR (dB) | 0 | −6 | −10 | −12 | −14 | −16 | −20 |
|---|---|---|---|---|---|---|---|
| Jeu −12 dB | 0 % | 0 % | **0 %** | 4,9 % | 29,3 % | 58,5 % | 72,4 % |
| (réf. jeu B −16 dB, curve_coh_opt_otsu) | 0 % | – | 0 % | – | – | 26,8 % à −15 | 72,0 % |
→ 0 % jusqu'à −10 dB (une passe de ≈ 145 caractères) ; à −12 dB 4,9 % (S/I/D 2/2/2) : cohérent avec la confirmation (moyenne 1,1 %, max 3,3 %) compte tenu de la dispersion (±4 points).
Le jeu ne devient pas meilleur au-delà de −12 dB (−14 : 29 %, −16 : 59 %) ; le jeu B, optimisé à −16 dB, reste meilleur à −15/−16 dB (26,8 % à −15).

**Dynamique d'entrée** (SNR −5 dB, 110 s/point, `data/coh/level_coh_best12_10wpm.csv`) : **0 % d'erreur aux 10 niveaux, +6 … −48 dB** (n = 94…100 caractères) — dynamique ≥ 54 dB inchangée, la limite
basse n'est toujours pas atteinte (plancher de bruit de l'ADC non atteint).

**Suivi de fréquence, statique** (SNR −5 dB, 110 s/point, `data/coh/freq_static_best12.csv`) :
| Écart | track 0 : CER / foff | track 1 : CER / foff / fcorr |
|---|---|---|
| 0 Hz | 0 % / +0,13 | 0 % / −0,11 / −0,26 |
| +6 Hz | 48,8 % / +5,2 | **0 %** / +5,3 / +5,38 |
| −6 Hz | 34,1 % / −5,1 | **0 %** / −5,3 / −5,03 |
**Dérive 0 → +12 Hz sur 300 s** (`data/coh/freq_drift_best12.csv`) : track 0 : 36,6 % (320 caractères) ; **track 1 : 0,4 %** (262 caractères, fcorr final +6,15, foff +6,3).

**Comparaison avec le jeu B** (−16 dB) : même plage de suivi (±6 Hz statique) ; dérive 36,6 % → 0,4 % (jeu B : 47,3 % → 4,9 %) ; dynamique identique (0 % de +6 à −48 dB) ; courbe : 0 % à −10 dB pour les deux, jeu −12 dB 4,9 % à −12 dB.
Le jeu −12 dB (fciq 5,7, fcpow 7,5) est un peu plus rapide que le jeu B (fciq 4,0, fcpow 4,8) : meilleur suivi de dérive, un peu moins bon à −15/−16 dB. Une seule passe par point (±4 points).
Les deux cartes vérifiées après la chaîne (by-id, gain codeur 0,05).

## 2026-10-06 20:30 — 15 WPM : niveau à 0 % de CER, puis Optuna 2 dB plus bas (norm 0, Otsu ; firmware 1.7.2-bench)
Demande : « optimisation Optuna à 15 WPM, trouver le niveau où l'on atteint 0 % de CER et optimiser 2 dB plus bas ». 15 WPM = `set_speed_15wpm()` du codeur (point ≈ 80 ms contre 120 ms à 10 WPM). Mêmes réglages de banc que la synthèse
ci-dessus (gain codeur 0,0091, `--nospace`, `norm 0`, `algo 1`). Le jeu −12 dB retenu à 10 WPM (seg 4700) ne donne **aucun niveau à 0 %** à 15 WPM (`data/coh15/curve_coh_best12_15wpm.csv`) : fenêtre `seg` trop longue
par rapport au point de 80 ms. D'où trois phases : **A** Optuna exploratoire à −6 dB pour obtenir un jeu adapté à 15 WPM, **B** courbes CER/SNR des meilleurs jeux pour fixer le niveau X à 0 %, **C** Optuna à X − 2 dB.

### Phase B — courbes CER/SNR à 15 WPM (165 s par point, `bench/coh_chain6.sh`, `data/coh15/curve_coh15_t{2,14,20}.csv`)
| Jeu (essai de A) | 0 dB | −6 dB | −8 dB | −10 dB | −12 dB |
|---|---|---|---|---|---|
| essai 2 | 0 % | 0,98 % | **0 %** | 2,4 % | 11,7 % |
| essai 14 | — | — | **0 %** | 1,5 % | 17,6 % |
| essai 20 | — | — | 2,9 % | 4,9 % | 19,5 % |

**X = −8 dB** (CER 0 % pour deux jeux ; −6 dB de l'essai 2 à 0,98 % : dispersion de mesure ≈ ±1 point à ≈ 215 caractères). Phase C visée : **−10 dB**.

### Paramètres optimisés, zones de recherche, valeurs retenues (15 WPM)
Zones de recherche identiques à celles de la synthèse (voir plus haut) : `seg` 800…6000 pas 100, `hop` 100…1000 pas 50 (≤ seg), `fciq` 0,8…12 Hz log, `fcpow` 0,8…14 Hz log, `fcpre` 1000…4000 Hz log,
`startref` 60…200 pas 10, `dvar` 0…1, `owin` 5000…30 000 ms pas 1000, `omargin` −0,3…+0,3, `osmooth` 0…0,9 ; `tau`/`decay` et `marge` hors recherche (`norm 0`, `algo 1` : `decay` sans effet). Défaut enqueué en essai 0.
Phase A : étude `coh15_otsu_norm0_snr-6` (−6 dB, 110 s + 20 s par essai, ≈ 48 caractères seulement pour les essais très mauvais, ≈ 145 sinon ; 22 essais, 15:20 → 16:10 ; amorçage : défaut seul).
Phase C : étude `coh15_otsu_norm0_snr-10` (−10 dB, 56 essais, 17:20 → 19:27 ; amorçage : défaut + jeux A 2, 14, 20, 18, 21 sans `decay`). Les 2 jeux à 0 % de C : essais 47 et 53.

| Paramètre | Zone de recherche | Défaut (essai 0) | Phase A (−6 dB) : essai 2 · essai 14 | Phase C (−10 dB) : **retenu = essai 47** · essai 53 |
|---|---|---|---|---|
| `seg` | 800 … 6000, pas 100 | 2500 | 4900 · 3800 | **3400** · 3200 |
| `hop` | 100 … 1000, pas 50 | 500 | 700 · 600 | **650** · 700 |
| `fciq` (Hz) | 0,8 … 12, log | 3 | 6,25 · 7,20 | **8,955** · 5,95 |
| `fcpow` (Hz) | 0,8 … 14, log | 3 | 9,20 · 13,43 | **9,567** · 9,50 |
| `fcpre` (Hz) | 1000 … 4000, log | 1800 | 1427 · 1534 | **2517** · 2137 |
| `startref` | 60 … 200, pas 10 | 100 | 200 · 200 | **180** · 150 |
| `dvar` | 0 … 1 | 1 | 1 · 1 | **1** · 1 |
| `owin` (ms) | 5000 … 30 000, pas 1000 | 10 000 | 28 000 · 26 000 | **27 000** · 27 000 |
| `omargin` | −0,3 … +0,3 | 0 | −0,077 · −0,017 | **−0,063** · +0,045 |
| `osmooth` | 0 … 0,9 | 0 | 0,646 · 0,672 | **0,776** · 0,802 |
| CER meilleur essai (recherche) | — | 75,6 % (−6 dB) / 80,5 % (−10 dB) | 0 % (6 essais sur 22 : 2, 12, 14, 18, 20, 21) | 0 % (2 essais sur 56 : 47, 53) |
| Confirmation indépendante −10 dB (3 × 165 s) | — | — | essai 2 : 6,8 / 2,4 / 7,8 % (moy. 5,7 %) | **essai 47 : 1,46 / 2,44 / 0 % (moy. 1,30 %)** · essai 53 : 3,9 / 4,9 / 3,9 % (moy. 4,2 %) |

Autres jeux confirmés à −10 dB (la ligne « essai 2 » de A ci-dessus est l’essai 1 de C, qui rejoue ce jeu) (`data/coh15/confirm_snr-10.{csv,json,out}`, 5 jeux × 3 × 165 s) : essai 27 de C : 4,9 / 1,0 / 0 % (moy. 1,95 %) ; essai 20 de C : 6,8 / 0,5 / 8,3 % (5,2 %).
Tendance à 15 WPM : `seg` plus court (3200–4900 contre 4700 à 10 WPM, optimum 3400), `fcpre` plus haut (≈ 2500), `owin` à la limite haute (≈ 27 000 ms, borne 30 000), `startref` 150–200 ; `fciq` 6–9 Hz.
Le 0 % de la recherche (≈ 145 caractères) ne se reproduit pas à l'identique : la confirmation donne **1,3 % moyen à −10 dB** pour le meilleur jeu, contre 1,1 % moyen à −12 dB à 10 WPM — le seuil effectif à 15 WPM est environ 2 dB plus haut (−10 dB au lieu de −12 dB).

### Jeu retenu à −10 dB (15 WPM, Otsu, `norm 0`)
`set norm 0;set algo 1;set seg 3400;set hop 650;set fciq 8.955;set fcpow 9.567;set fcpre 2517.4;set decay 0.999349;set startref 180;set dvar 1;set owin 27000;set omargin -0.063;set osmooth 0.776`
(copie dans `data/coh15/pre_best15_m10.txt`). Non adopté comme défaut du firmware ; à appliquer par `set` après chaque flash.
Fichiers : `data/coh/optuna_coh15_otsu_norm0_snr-{6,10}.{csv,db,log}`, `data/coh/coh15_optuna_snr-{6,10}.out`, `data/coh15/`. Scripts : `bench/coh_chain6.sh`, `bench/coh_pre_trial.py`, `bench/coh_confirm_multi.py --wpm 15`.
Limites : un seul tirage de la graine, pas de test de dynamique ni de suivi de fréquence à 15 WPM avec ce jeu.

### Comparatif des paramètres optimums : 10 WPM contre 15 WPM (Otsu, `norm 0`)
10 WPM : jeu retenu à −12 dB (étude 5, 109 essais, 1,1 % moyen confirmé à −12 dB). 15 WPM : jeu retenu à −10 dB (phase C, 56 essais, 1,3 % moyen confirmé à −10 dB). Point Morse : 120 ms à 10 WPM, 80 ms à 15 WPM (rapport 1,5).
Les jeux « à 0 % à −10 dB » de 10 WPM (A, B de l'étude 4) sont rappelés pour mesurer la dispersion entre bons jeux.

| Paramètre | Zone de recherche | 10 WPM retenu (−12 dB) | 10 WPM jeu A (−10 dB) | 10 WPM jeu B (−10 dB) | 15 WPM retenu (−10 dB) | Rapport 15/10 (retenus) |
|---|---|---|---|---|---|---|
| `seg` (échantillons) | 800 … 6000 | 4700 (108,8 ms) | 4900 | 4500 | 3400 (78,7 ms) | ×0,72 |
| `seg` / durée du point | — | 0,91 | 0,95 | 0,87 | 0,98 | ≈ 1 : fenêtre ≈ un point |
| `hop` (échantillons) | 100 … 1000 | 650 (15,0 ms) | 700 | 300 | 650 (15,0 ms) | ×1,00 |
| `fciq` (Hz) | 0,8 … 12 | 5,75 | 6,25 | 3,98 | 8,955 | ×1,56 |
| `fcpow` (Hz) | 0,8 … 14 | 7,48 | 9,20 | 4,82 | 9,567 | ×1,28 |
| `fcpre` (Hz) | 1000 … 4000 | 1190 | 1427 | 1272 | 2517 | ×2,1 |
| `startref` | 60 … 200 | 200 | 200 | 140 | 180 | ×0,9 |
| `dvar` | 0 … 1 | 1 | 1 | 1 | 1 | = |
| `owin` (ms) | 5000 … 30 000 | 8000 | 28 000 | 13 000 | 27 000 | ×3,4 |
| `omargin` | −0,3 … +0,3 | −0,120 | −0,077 | −0,067 | −0,063 | ≈ −0,06 à −0,12 |
| `osmooth` | 0 … 0,9 | 0,766 | 0,646 | 0,808 | 0,776 | ≈ 0,65 à 0,8 |
| CER confirmé (3 × 165 s) | — | 1,1 % à −12 dB | 0 % à −10 dB | 0 % à −10 dB | 1,3 % à −10 dB | seuil ≈ +2 dB à 15 WPM |

Lecture : `hop` (≈ 15 ms), `dvar 1`, `startref` ≈ 180–200, `omargin` ≈ −0,06…−0,12 et `osmooth` ≈ 0,65…0,8 sont stables d'une vitesse à l'autre. `seg` suit la durée du point (fenêtre ≈ 0,9–1,0 point), `fciq`
(×1,56) suit la vitesse (×1,5). `fcpow` monte moins vite (×1,28). `fcpre` doublerait à 15 WPM mais la plage 1190–2517 Hz est plate à 10 WPM (jeux à 1272…2585 équivalents) : à ne pas sur-interpréter.
`owin` n'est pas discriminant : à 10 WPM des jeux à 0 % existent de 8000 à 28 000 ms, et à 15 WPM tous les bons essais sont à 25 000–29 000 ms (borne haute 30 000, à élargir si on poursuit).
Dispersion : la confirmation à −10/−12 dB classe les jeux à ≈ ±1–2 points ; ces écarts entre jeux voisins sont dans le bruit de mesure.

## 2026-10-07 03:50 — 20 WPM : niveau à 0 % de CER, puis Optuna 2 dB plus bas (norm 0, Otsu ; firmware 1.7.2-bench)
Même méthode qu'à 15 WPM (phases A, B, C), mode codeur `speed 1` (« 20 WPM » existant, point ≈ 60 ms ; `bench/rig.py set_speed_20wpm`, `--wpm 20` dans `coh_optuna.py`, `curve_levelcomp.py`, `coh_confirm_multi.py`). Gain codeur 0,0091, `--nospace`, 110 s + 20 s par essai (≈ 190 caractères par essai).

### Phase A — Optuna exploratoire à −6 dB (`coh20_otsu_norm0_snr-6`, 22:41 → 23:57, 33 essais)
Amorçage : défaut (essai 0, 75,6 %), jeu retenu 15 WPM (essai 47 de C), jeu 27 de C (15 WPM), jeux A14 et A21 de 15 WPM, jeu mis à l'échelle (seg 2500, hop 500, fciq 11, fcpow 11, fcpre 2600). **6 essais à 0 %** : 3, 5, 12, 21, 25, 29 (dont 12, 25, 29 trouvés par TPE).

### Phase B — courbes CER/SNR à 20 WPM (165 s par point, `bench/coh_chain7.sh`, `data/coh20/curve_coh20_t{12,25,29}.csv`)
| Jeu (essai de A) | 0 dB | −6 dB | −8 dB | −10 dB | −12 dB |
|---|---|---|---|---|---|
| essai 12 | 0 % | 0 % | 0,35 % | 4,9 % | 30,3 % |
| essai 25 | — | — | 1,0 % | 12,2 % | 33,1 % |
| essai 29 | — | — | **0 %** | 9,4 % | 37,0 % |

**X = −8 dB** (CER 0 % pour l'essai 29, 0,35 % pour l'essai 12 ; ≈ 288 caractères par point). Phase C : **−10 dB**.

### Phase C — Optuna à −10 dB (`coh20_otsu_norm0_snr-10`, 00:41 → 02:47, 56 essais)
Amorçage : défaut + jeux 12, 29, 25, 21, 3, 5 de A (sans `decay`). **Aucun essai à 0 %** : meilleur #1 (= jeu 12 de A) 3,05 % ; suivants #22 4,9 %, #23/#44/#48 5,5 %. Confirmation : 5 jeux × 3 × 165 s à −10 dB (`--cermax 0,07`, `data/coh20/confirm_snr-10.{csv,json,out}`).

| Essai de C | CER en 3 mesures | Moyenne |
|---|---|---|
| **#23** | 8,0 / 9,1 / 7,3 % | **8,1 %** |
| #1 | 9,4 / 10,6 / 8,4 % | 9,5 % |
| #22 | 8,0 / 10,8 / 10,8 % | 9,9 % |
| #48 | 15,0 / 8,7 / 8,9 % | 10,9 % |
| #44 | 18,7 / 10,1 / 7,0 % | 11,9 % |

Les essais de recherche (3–5 %) sont nettement meilleurs que leur confirmation (≈ 8–12 %) : sur-ajustement au bruit de mesure ; à −10 dB et 20 WPM le niveau réel est ≈ 8 % contre 1,3 % à 15 WPM.

### Paramètres optimisés, zones de recherche, valeurs retenues (20 WPM)
Zones de recherche inchangées (voir synthèse 10 WPM) ; `tau`/`decay` et `marge` hors recherche (`norm 0`, `algo 1`).

| Paramètre | Zone de recherche | Défaut | A (−6 dB) : essai 12 · essai 29 · essai 25 | C (−10 dB) : **retenu #23** · #1 · #22 |
|---|---|---|---|---|
| `seg` | 800 … 6000, pas 100 | 2500 | 2200 · 2300 · 2000 | **3000** · 2200 · 1700 |
| `hop` | 100 … 1000, pas 50 | 500 | 250 · 200 · 300 | **300** · 250 · 250 |
| `fciq` (Hz) | 0,8 … 12, log | 3 | 7,96 · 10,94 · 7,31 | **10,907** · 7,96 · 11,58 |
| `fcpow` (Hz) | 0,8 … 14, log | 3 | 12,22 · 12,74 · 10,40 | **12,253** · 12,22 · 10,79 |
| `fcpre` (Hz) | 1000 … 4000, log | 1800 | 3556 · 2233 · 3753 | **2655** · 3556 · 3832 |
| `startref` | 60 … 200, pas 10 | 100 | 180 · 170 · 190 | **180** · 180 · 200 |
| `dvar` | 0 … 1 | 1 | 1 · 1 · 1 | **1** · 1 · 1 |
| `owin` (ms) | 5000 … 30 000, pas 1000 | 10 000 | 30 000 · 28 000 · 24 000 | **26 000** · 30 000 · 21 000 |
| `omargin` | −0,3 … +0,3 | 0 | −0,027 · −0,094 · −0,028 | **−0,028** · −0,027 · −0,006 |
| `osmooth` | 0 … 0,9 | 0 | 0,750 · 0,667 · 0,471 | **0,277** · 0,750 · 0,548 |
| CER (recherche) | — | 75,6 % (−6 dB) | 0 % (6 essais sur 33) | aucun 0 % ; meilleur 3,05 % (#1) |
| Confirmation −10 dB (3 × 165 s) | — | — | — | **#23 : 8,1 %** · #1 : 9,5 % · #22 : 9,9 % |

Limites de la zone de recherche à 20 WPM : `fcpow`, `fciq`, `fcpre` et `owin` des bons essais se rapprochent des bornes hautes (12,3 et 11,6 Hz contre 14 et 12 ; 3,8 kHz contre 4 ; `owin` jusqu'à 30 000 ms) ; élargir ces zones si la campagne se poursuit.

### Jeu retenu à −10 dB (20 WPM, Otsu, `norm 0`)
`set norm 0;set algo 1;set seg 3000;set hop 300;set fciq 10.907;set fcpow 12.253;set fcpre 2654.5;set decay 0.999699;set startref 180;set dvar 1;set owin 26000;set omargin -0.028;set osmooth 0.277`
(copie dans `data/coh20/pre_best20_m10.txt`). Pour une utilisation à −8 dB (0 %), le jeu 29 de A convient (`bench/coh_pre_trial.py data/coh/optuna_coh20_otsu_norm0_snr-6.csv 29`). Non adopté comme défaut du firmware.
Fichiers : `data/coh/optuna_coh20_otsu_norm0_snr-{6,10}.{csv,db,log}`, `data/coh/coh20_optuna_*.out`, `data/coh20/`, `bench/coh_chain7.sh`.
