# HARNAIS.md — Harnais d'interface du décodeur cohérent (écran, oscilloscope, bouton rotatif, liaison série)

Ce document décrit le « harnais » (l'interface utilisateur et de banc) de `Morse_decode_coherent.ino`
(version `1.9.0-bench 2026-10-09 ML presets 10/15/20WPM`) pour qu'un autre projet puisse le
reproduire sans relire le code : matériel, affichage TFT, oscilloscope intégré, sélection et réglage
des paramètres au bouton rotatif, langage de commandes série. Les numéros de ligne cités sont ceux de
`Morse_decode_coherent.ino` à cette version.

Le harnais est indépendant de l'algorithme de décodage : il suppose seulement un démodulateur qui
fournit une puissance (échantillon par échantillon), une décision tonalité/silence, et un décodeur
Morse qui produit des caractères. Les points de couplage sont indiqués au §7.

---

## 1. Matériel et brochage (Teensy 4.0)

| Élément | Broche / réf. | Détail |
|---|---|---|
| Carte audio | Audio Shield Rev D (SGTL5000, I2S, I2C pins 18/19) | entrée ligne **canal droit** (`lineInLevel(0)` = 3,12 Vpp max), sortie ligne G/D + casque |
| Écran | ILI9341 320×240 SPI, bibliothèque `ILI9341_t3n`, `setRotation(1)` | CS=10, DC=2, RST=1, MOSI=11, SCLK=13, MISO=12 |
| Encodeur rotatif | bibliothèque `EncoderTool`, `encoder.begin(3, 0)` | A=pin 3, B=pin 0 (1 cran = ±1) |
| Bouton (poussoir de l'encodeur) | pin 4, `Bounce2::Button`, `INPUT_PULLUP`, antirebond 5 ms, appui = niveau BAS | |
| LED | pin 5 | image de la décision courante (voir §7) |
| Entrées debug | pin 14 = « sync start », pin 15 = « tone » | reliées aux sorties pin 6 (impulsion de début de séquence) et pin 9 (enveloppe Morse) du codeur, voir « Liaison avec le codeur » ci-dessous ; lues par l'oscilloscope, la pin 14 synchronise une capture série |
| Série USB | 115 200 bauds | commandes §5 |

Pins réservées par l'Audio Shield : 6, 7, 8, 9, 20, 21, 23 (voir README.md, tableau complet).

Réglages audio : `AudioMemory(100)`, `AudioMemory_F32(200)`, `sgtl5000.lineInLevel(0)`,
`lineOutLevel(13)`, `volume(0.6)`.

### Chaîne audio (toute en F32, OpenAudio_ArduinoLibrary)

```
Line In (droit) ──► AudioInputI2S_F32 ──► AudioCoherentDemodSegmented4x_F32 CW_In(2500, 0.8f)
                                                │  9 sorties (index ci-dessous)
                       ┌────────────────────────┘
        AudioSignalGenerator_F32 (« SIG INT »)  +  entrée brute (« INPUT »)
                       ▼
     2 × AudioMixer11_F32 (un par voie de sortie, 11 entrées) ──► AudioOutputI2S_F32 (gain I2s2.setGain)
```

Sources sélectionnables sur chacune des deux voies de sortie (11 entrées de mixeur) :

| Idx | Nom affiché | Contenu |
|---|---|---|
| 0 | POWER | puissance normalisée |
| 1 | I_SAMPLES | voie I |
| 2 | Q_SAMPLES | voie Q |
| 3 | PHASE | phase |
| 4 | SUBSAMP | ticks de sous-échantillonnage |
| 5 | DETECT | décision de détection |
| 6 | RAW_POWER | puissance brute (non normalisée) |
| 7 | FCARRIER_BESSEL | porteuse filtrée (Bessel) |
| 8 | ANTI_ALIASING | sortie pré-filtrée |
| 9 | INPUT | entrée ligne brute |
| 10 | SIG INT | générateur de signal interne |

Au démarrage : **Out Left = FCARRIER_BESSEL, Out Right = POWER**, gain 1.0, générateur = mode 0 (« 1.0 »).
Sélectionner une source = gain 1 sur l'entrée choisie du mixeur, gain 0 sur toutes les autres.

Le but de ces sorties analogiques est le **câblage d'un oscilloscope/analyseur externe** : on regarde
n'importe quel point de la chaîne sur les deux voies ligne. Gain de sortie (`setGain`) : {0,1 ; 1,0 ; 10,0}.

Générateur interne (menu « Sig Gen », `sigGen.setMode`) : `1.0`, `0.0`, `-1.0` (niveaux continus),
`0.9Hz`, `9Hz`, `90Hz`, `900Hz` (sinus). Sert à tester la chaîne sans signal.

### Liaison avec le codeur (source de test)

Le décodeur est testé avec le codeur `Codeur_CW_V1_5_T4/` (autre Teensy 4.0 + Audio Shield) qui fournit
le signal Morse bruité et deux signaux logiques de vérité terrain. Trois liaisons :

| Liaison | Codeur | Décodeur | Contenu |
|---|---|---|---|
| Audio | sortie ligne | entrée ligne **droite** | tonalité Morse (900 Hz par défaut) + bruit gaussien, SNR réglable de +10 à −20 dB |
| Début de séquence | pin 6 (`SYNC_PIN`) | **pin 14** | impulsion à l'état haut, **200 ms** (`SYNC_PULSE_MS`) |
| Modulation Morse | pin 9 (`TONE_PIN`) | **pin 15** | niveau haut = tonalité présente (enveloppe de la clé, reflet logique de l'enveloppe CW) |

Masse commune obligatoire. Niveaux logiques 3,3 V (Teensy vers Teensy, pas d'adaptation). Côté décodeur
les pins 14 et 15 sont des `INPUT` simples, sans pull-up.

Moments où le codeur émet l'impulsion de 200 ms sur la pin 6 :
- au début de **chaque message CW** ;
- toutes les **5 s** (`PERIODIC_SYNC_MS`) dans les modes continus (tonalité continue, alternance bruit /
  bruit+tonalité) ;
- en mode test AGC (cycle de 20 s) : une impulsion de 200 ms au début du cycle, puis une impulsion plus
  courte toutes les 3 s (`AGC_PERIODIC_SYNC_MS`).

Côté décodeur, ces deux signaux ne servent **qu'à l'oscilloscope** :
- la pin 14 aligne le balayage lors d'une capture série (§3.3) : le balayage de 3 s démarre sur le front
  montant de l'impulsion, donc au début du message, ce qui rend les captures comparables entre elles ;
- la pin 15 est tracée en cyan à côté de la décision du logiciel (magenta), pour comparer visuellement
  ce que le décodeur a décidé à l'enveloppe réellement émise ;
- les deux colonnes `Sync` et `Tone` du CSV de la commande `scope` en sont la copie.

Le décodage lui-même n'utilise que l'audio : sans les fils 14 et 15 le décodeur fonctionne, seuls le
déclenchement synchronisé du scope et la comparaison à la vérité terrain disparaissent (les pins flottantes
donnent alors des traces aléatoires).

---

## 2. Affichage TFT 320×240 (rotation 1, fond noir)

Trois zones superposées verticalement, plus l'oscilloscope intégré dans la zone centrale basse.

```
y=0   ┌──────────────────────────────────────────────┐
      │ texte décodé, 2 lignes de 20 px (Arial_14)   │  zone texte  y = 0..36
y=36  ├──────────────────────────────────────────────┤
y=38  │ Out Left   FCARRIER_BESSEL                   │  5 lignes de menu × 22 px
y=60  │ Out Right  POWER                             │  (nom x=4, valeur x=110)
y=82  │ Gain       1.0                               │
y=104 │ Sig Gen    1.0                               │
y=126 │ Marge      Otsu  (ou 0,30)                   │
y=148 ├─┬────────────────────────────────────────────┤
      │S│ oscilloscope : x=20..320, y=148..222       │  barre « scope » 20 px à gauche (ligne virtuelle n°5)
y=222 │ CW decod. 1.9.0-bench ...                    │  version, jaune
y=240 └──────────────────────────────────────────────┘
```

### 2.1 Zone texte décodé (y = 0..36)

- Police `Arial_14`, blanc sur noir.
- Les caractères sont affichés l'un après l'autre : départ à `posH = 10`, avance de **15 px par caractère**.
- Retour à la ligne quand `posH > 300` : `posH` revient à 10 et `posV` descend de **20 px**.
- Quand `posV > 16` (plus de place sous la 2ᵉ ligne), la zone texte est effacée (rectangle noir) et le
  texte repart en haut. Le texte n'est donc jamais défilé : c'est un affichage à page effacée.
- Les caractères viennent du décodeur via `sendToTFT1(char)` (l'ancien Serial1/TFT1 de la version à deux cartes).
  En mode duel (`set ml 2`) le décodeur classique émet en MAJUSCULES et le ML en minuscules.

### 2.2 Menu (à partir de y = 38)

5 lignes de 22 px chacune (`drawMenuRow` / `drawMenu`) :

| Ligne | Nom | Valeur affichée |
|---|---|---|
| 0 | `Out Left` | nom de la source (§1) |
| 1 | `Out Right` | nom de la source |
| 2 | `Gain` | `0.1`, `1.0` ou `10.0` |
| 3 | `Sig Gen` | `1.0`, `0.0`, `-1.0`, `0.9Hz`, `9Hz`, `90Hz`, `900Hz` |
| 4 | `Marge` | `Otsu ` si `decAlgo == 1`, sinon la marge avec 2 décimales (ex. `0.30`) |

Codes couleur :

| Élément | Navigation | Édition |
|---|---|---|
| Nom de ligne | orange | orange |
| Ligne sélectionnée : fond | **boîte blanche** de 100 px de large sous le nom, texte noir | **boîte jaune**, texte noir |
| Valeur | vert | **jaune** |

- Nom à x = 4, valeur à x = 110.
- La ligne virtuelle n°5 (« scope ») n'a pas de texte : elle est matérialisée par une **barre de 20 px de
  large à gauche de la zone oscilloscope** (`drawScopeBar`) : blanche si la ligne est sélectionnée, noire sinon ;
  elle clignote en **jaune pendant 200 ms** à l'appui du bouton (demande de capture série).

### 2.3 Pied de page et écran de démarrage

- y = 222 : `"CW decod. " + VERSION`, jaune (`displayVersion`).
- Au démarrage, écran d'accueil de **3 s**, texte orange : `decodeur Morse ` en (30, 7), la `VERSION`
  en (30, 50), ` F1FGV et F1VL` (`AUTEUR`) en (30, 93). Puis effacement et dessin du menu.
- Piège : sur la variante Goertzel (autre firmware), ne rien dessiner avant `tft.begin()`.

---

## 3. Oscilloscope intégré (`updateOscilloscope`, appelé à chaque pas de `loop()`)

### 3.1 Géométrie et balayage

- Zone : x = 20..320 (**300 px**), y = 148..222 (74 px de haut).
- Base de temps : **10 ms par pixel** → balayage de **3,0 s**.
- Un point est ajouté toutes les 10 ms (horloge `millis()`), à la colonne courante.
- Balayage **libre** : à la fin des 300 colonnes il repart à la première. S'il y a une demande de capture
  série en attente, le redémarrage attend à la place le **front montant de la pin 14** (synchro externe).
- Effacement anticipé : les **10 colonnes devant le curseur** sont effacées (bande noire qui court devant
  la trace), sans effacer toute la zone à chaque balayage.
- Traits verticaux : chaque nouvel échantillon est relié au précédent par un segment vertical pour que les
  fronts soient visibles malgré la résolution.

### 3.2 Les traces (de haut en bas)

| Bande | Contenu | Couleur | Positions (y) |
|---|---|---|---|
| Entrées debug | pin 14 (sync) **et** pin 15 (tone), superposées dans une seule bande à 2 niveaux | pin 14 vert, pin 15 cyan | niveau haut `area_y + 5`, bas `+10` (`SYNC_HIGH` / `SYNC_LOW`) |
| Décision | `scopeDecision` = tonalité/silence réellement décidé par le décodeur | magenta | haut = `SYNC_LOW + 10`, bas = `+10` (`DETECT_HIGH` / `DETECT_LOW`) |
| Analogique | puissance + seuil | jaune + rouge | de `POWER_TOP = DETECT_LOW + 5` à `POWER_BOT = 222 − 8` |

Zone analogique :

- **Jaune** : la puissance avant décision. C'est `sqrt(power)` si `algo 1` et `dvar 1`, sinon la puissance
  telle quelle.
- **Échelle automatique** : maximum glissant `scopeRunMax = max(v, scopeRunMax × 0,9995)` (constante de
  temps ≈ 20 s, mise à jour à chaque pixel de 10 ms). Le tracé est `v / scopeRunMax` ramené à la hauteur `POWER_TOP..POWER_BOT`.
- **Rouge** : le **seuil effectif** `thrEff × (1 + oMargin)` (Otsu) ou la Marge (`algo 0`), à la **même
  échelle** que le jaune. Non tracé s'il n'existe pas encore de seuil (avant la 1ʳᵉ fenêtre Otsu de 3 s).
- La pin 14/15 n'intervient que pour la synchro de capture ; la décision magenta est celle du logiciel
  (pas celle de la pin 15).

### 3.3 Capture série d'un balayage (`scope`)

Déclenchée par la commande série `scope` **ou** par l'appui bouton sur la ligne 5. Un balayage complet
(300 lignes) est émis sur le port série, en commençant au prochain front montant de pin 14 (balayage
synchronisé). Format texte, séparateur **tabulation** :

```
Time	Sync	Tone	Detect	Power
<millis>	<pin14 0/1>	<pin15 0/1>	<detect 0/1>	<puissance brute %.3f>
... (300 lignes)
```

`Power` est la puissance brute (RAW_POWER), pas la valeur mise à l'échelle de l'écran. Pendant la
capture, l'écran dessine normalement.

---

## 4. Bouton rotatif : sélection et réglage

### 4.1 Machine à deux modes

- **Mode navigation** (par défaut) : la rotation déplace la sélection sur les **6 lignes** (0..5, circulaire :
  après la 5 on revient à 0 et inversement).
- **Mode édition** : atteint par un appui sur le bouton quand la sélection est sur les lignes 0..4 ; la
  rotation modifie alors la valeur de la ligne ; un nouvel appui revient en navigation.
- Appui sur la **ligne 5 (scope)** : n'entre pas en édition, demande une capture série (§3.3) et fait
  clignoter la barre en jaune 200 ms.
- Chaque cran de rotation = ±1 ; après traitement : `delay(80)` (anti-rafale) puis `drawMenu()`.
  L'affichage de la ligne sélectionnée change de couleur selon le mode (§2.2).

### 4.2 Effet de la rotation en mode édition

| Ligne | Paramètre | Pas | Plage / cycle | Effet |
|---|---|---|---|---|
| 0 | Out Left | ±1 | cycle circulaire des 11 sources | `selectOutputChannel(0, idx)` : gain 1 sur la source choisie, 0 sur les autres |
| 1 | Out Right | ±1 | idem | `selectOutputChannel(1, idx)` |
| 2 | Gain | ±1 | cycle {0.1, 1.0, 10.0} | `I2s2.setGain(valeur)` |
| 3 | Sig Gen | ±1 | cycle des 7 modes | `sigGen.setMode(...)` |
| 4 | Marge | **±0,01** | bornée à [0, 1] (pas de bouclage) | `CW_In.set_threshold(marge)` |

Notes :
- La Marge n'a d'effet décisionnel qu'avec `algo 0` (seuil fixe) ; avec `algo 1` (Otsu) la ligne affiche
  `Otsu` et la rotation n'a pas d'utilité. Elle sert aussi de seuil de repli avant la première valeur
  d'Otsu quand la normalisation est active.
- **Divergence corrigée** : l'ancien README annonçait un pas de 0,005 pour la Marge ; le code (et donc
  ce document) utilise **0,01**.

---

## 5. Liaison série : langage de commandes

### 5.1 Généralités

- 115 200 bauds, 8N1, USB CDC du Teensy.
- Une commande par ligne (terminée par `\n` ou `\r` ; les lignes vides sont ignorées), **tampon de 64 caractères** :
  en cas de débordement le tampon est vidé et le reste de la ligne est interprété comme un début de
  nouvelle commande (donc : ne pas dépasser 63 caractères). Espaces de début/fin supprimés.
- **Sensible à la casse** : mots-clés en minuscules, noms de source exactement comme au §1 (majuscules,
  `SIG INT` avec espace).
- Réponses : `set out<N> <SRC>` → `Out Left -> SOURCE` ; `set gain` → `Gain = 1.0` ; `set siggen` →
  `Sig Gen = 900Hz` ; `set marge` → `Marge = 0.300` ; `set wpm` → `wpm = 20 (preset applied, decoder reset)` ;
  `reset` → `reset ok` ; `log` → `log on` / `log off` ; `peak reset` → `peak reset` ; `scope` →
  `Scope serial requested` ; autres `set` → `<param> = <valeur>` (ex. `seg = 3000`) ; erreurs :
  `Error: output index 0-1`, `Error: unknown source '<nom>'`, `Error: gain must be 0.1, 1.0, or 10.0`,
  `Error: siggen mode 0-6`, `Error: invalid value`, `Error: wpm must be 10, 15 or 20`,
  `Error: unknown set parameter` ; commande inconnue : `Unknown command: <cmd>` puis `Type 'help' for commands`.
- Les commandes qui touchent la sortie/gain/sig-gen **redessinent le menu** sur le TFT.
- Pour un banc automatisé, résoudre les cartes par `/dev/serial/by-id/*<n° série>*` (la numérotation
  `/dev/ttyACM*` change au rebranchement).

### 5.2 Commandes de réglage du harnais

| Commande | Effet |
|---|---|
| `help` | liste des commandes, noms des sources, modes du générateur |
| `status` | état complet (voir §5.4) |
| `reset` | remet à zéro le décodeur (démodulateur, Otsu, durées de référence, ML) sans toucher aux réglages |
| `log on` / `log off` | active/coupe l'envoi des **caractères décodés** (et des espaces de mots) sur le port série, en plus du TFT : c'est le flux lu par le banc pour mesurer le taux d'erreur. En mode `ml 2` : classique en MAJUSCULES, ML en minuscules |
| `peak` / `peak reset` | `ADC peak max=… min=…` : crête de l'entrée ADC (étalonnage du niveau d'entrée) / remise à zéro |
| `scope` | demande une capture série d'un balayage (§3.3) |
| `foff` | `foff: <Hz> <confiance> <nb mises à jour>` : écart de fréquence estimé de la porteuse |
| `set out0 <SOURCE>` / `set out1 <SOURCE>` | choisit la source de la voie gauche / droite (noms du §1 : `POWER`, `I_SAMPLES`, `Q_SAMPLES`, `PHASE`, `SUBSAMP`, `DETECT`, `RAW_POWER`, `FCARRIER_BESSEL`, `ANTI_ALIASING`, `INPUT`, `SIG INT`) |
| `set gain <0.1\|1.0\|10.0>` | gain de sortie |
| `set siggen <0..6>` | mode du générateur interne (index dans la liste du §1) |
| `set marge <v>` | seuil fixe [0,1] (`algo 0`) |

### 5.3 Commandes de traitement (les réglages optimisés par Optuna)

Toutes sont de la forme `set <param> <valeur>` et prennent effet immédiatement, sans reflasher (mais sont
perdues au reset de la carte — ou rechargées par `set wpm`).

| Paramètre | Valeurs | Rôle |
|---|---|---|
| `wpm` | `10`, `15`, `20` | **applique le jeu de préréglages optimum** (ci-dessous) et réinitialise le décodeur ; défaut au démarrage : 20 WPM |
| `algo` | 0 / 1 | 0 = seuil fixe (Marge), 1 = seuil d'Otsu |
| `norm` | 0 / 1 | normalisation par maximum glissant de la puissance (remet Otsu à zéro) |
| `track` / `trackk` | 0\|1 / réel | poursuite en fréquence (boucle fermée) et son gain |
| `dvar` | 0 / 1 | 1 = décision sur `sqrt(power)` |
| `omargin` | réel | marge multiplicative sur le seuil Otsu : décision `v > thrEff·(1+omargin)` |
| `osmooth` | réel [0,1) | lissage du seuil : `thr = osmooth·thr_prec + (1−osmooth)·thr_Otsu` |
| `owin` | ms | fenêtre de l'histogramme Otsu (1ʳᵉ fenêtre fixe 3000 ms, puis `owin`) |
| `ofirst` | ms | durée de la première fenêtre Otsu (3000 par défaut) |
| `ml` | 0 / 1 / 2 | 0 = décodage classique, 1 = ML (vraisemblance maximale), 2 = **duel** (classique en MAJUSCULES, ML en minuscules) |
| `mlrec` | 0 / 1 | émet sur le port série `~<millis> <0\|1>` à chaque transition de la décision tonalité (capture pour le rejeu hors ligne `ml_replay.py`) |
| `mlsigma` | ms | σ₀ du modèle de durée ML |
| `mlglitch` | réel | pénalité de fusion de glitchs du ML |
| `startref` | ms | référence initiale pour le décodage classique (`Start_reference`) |
| `seg` | 200..12000 (borné) | longueur de segment du démodulateur cohérent |
| `hop` | 50..6000 (borné) | pas (hop) entre segments |
| `fciq` / `fcpow` / `fcpre` | réels | fréquences de coupure des filtres I/Q, puissance, pré-filtre |
| `decay` | réel | décroissance du maximum glissant de normalisation |

Jeux de préréglages `set wpm` (table `WPM_PRESETS[3]`, fonction `applyWpmPreset(w, doReset)`) :

| Champ | 10 WPM | 15 WPM | 20 WPM |
|---|---|---|---|
| norm / algo / dvar | 0 / 1 / 1 | 0 / 1 / 1 | 0 / 1 / 1 |
| seg | 5000 | 3400 | 3000 |
| hop | 450 | 650 | 300 |
| fciq | 8,119 | 8,955 | 10,907 |
| fcpow | 7,033 | 9,567 | 12,253 |
| fcpre | 1484,9 | 2517,4 | 2654,5 |
| decay | 0,999549 | 0,999349 | 0,999699 |
| startref | 190 | 180 | 180 |
| owin | 11000 | 27000 | 26000 |
| omargin | −0,148 | −0,063 | −0,028 |
| osmooth | 0,810 | 0,776 | 0,277 |
| mlSigma | 22 | 18 | 26 |
| mlGlitch | 7 | 10 | 6 |

(Valeurs des campagnes Optuna A/B/C, voir `STATUS.md`. `mlSigma` / `mlGlitch` sont les valeurs ML ; elles
ne valent que pour `set ml 1` ou `2`.)

### 5.4 Sortie de `status`

Lignes, dans l'ordre :

```
Out Left: FCARRIER_BESSEL
Out Right: POWER
Gain: 1.0
Sig Gen: 1.0
Marge: 0.300
Decoder: Morse_decode_coherent <VERSION>
ml=2 T=<unitMs> delta=<±ms> locked=<0|1> chars=<n> glitches=<n> sigma=<ms> pen=<glitch>
wpm=20
algo=1 (0=marge 1=otsu) dvar=1 norm=0 log=0 track=0 fcorr=+0.000
foff: <Hz> <confiance> <nb mises à jour>
seg=3000 hop=300 fciq=10.907 fcpow=12.253 fcpre=2654.5 decay=0.99970 startref=180
omargin=-0.028 osmooth=0.277 thr_eff=<seuil effectif>
otsu: valid=<0|1> thr=<…> m0=<…> m1=<…> n0=<…> n1=<…> windows=<n> win=<ms> first=<ms>
```

Les scripts de banc (`bench/rig.py`, `DecoderCtl.query("status")`) s'en servent pour vérifier l'identité du
firmware flashé (ligne `Decoder:`) et le jeu de réglages courant ; ne jamais déduire un flash réussi du seul
code retour de `arduino-cli upload`.

---

## 6. Reproduire le harnais dans un autre projet

1. **Bibliothèques** : `Audio`, `OpenAudio_ArduinoLibrary` (F32), `ILI9341_t3n`, `EncoderTool`, `Bounce2`,
   police `Arial_14` (`font_Arial.h`).
2. **Variables d'état** : `menuRow` (0..5), `menuEditMode` (bool), index de source par voie, index de gain,
   index de sig-gen, `marge` (float).
3. **`loop()`** : lire l'encodeur et le bouton ; traiter les commandes série ; appeler
   `updateOscilloscope()` ; l'échantillonneur `sample()` pousse la décision et la puissance vers le scope.
4. **Boutons** : `button.update(); if (button.pressed()) handleButton();` ; encodeur : différence de
   valeur depuis la dernière lecture → `handleEncoder(delta)`.
5. **Dessin** : `drawMenu()` redessine les 5 lignes puis `drawScopeBar()` ; ne redessiner que sur
   changement (rotation, appui, commande série qui modifie l'état affiché).
6. **Scope** : buffer circulaire à une colonne par 10 ms ; trois bandes + une zone analogique avec
   échelle auto (§3.2) ; effacement par bande de 10 colonnes devant le curseur.
7. **Série** : lecture octet par octet dans un tampon de 64 octets, dispatch par `strcmp`/`strncmp` sur
   les mots-clés du §5, réponse `<param> = <valeur>`.

## 7. Points de couplage avec l'algorithme

Pour brancher un autre décodeur sur ce harnais il faut fournir, en continu (appelé à chaque tour de `loop()`) :

| Donnée | Utilisée pour |
|---|---|
| puissance brute (et sa racine) | trace jaune + colonne `Power` du CSV |
| seuil effectif | trace rouge |
| décision binaire tonalité/silence | trace magenta, LED pin 5, colonne `Detect` du CSV |
| caractères décodés | `sendToTFT1(char)` (zone texte) |
| entrées pin 14 / 15 | traces vert / cyan, synchro de capture |

Le harnais lui-même ne dépend ni d'Otsu ni du ML : si l'algorithme n'a pas de seuil, ne pas tracer la
courbe rouge ; si l'algorithme n'est pas réglable, ignorer les lignes 0/1/2/3 du menu ou les remplacer par
d'autres paramètres du même type (liste circulaire ou valeur bornée à pas fixe, voir §4.2).
