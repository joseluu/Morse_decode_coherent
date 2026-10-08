// Decodeur Morse a vraisemblance maximale (variante "ML"), place APRES la decision tonalite/silence (Otsu).
//
// Entree  : flux binaire tonalite/silence (update(tone, millis)) -> suite d'evenements (marque/espace, duree ms).
// Modele  : marque = point (1T) ou trait (3T) ; espace = intra-caractere (1T), inter-caracteres (3T), inter-mots (>=7T).
//           Durees gaussiennes autour de k*T +/- delta (delta = biais de seuil, + pour les marques, - pour les espaces),
//           ecart-type sqrt(sigma0^2 + (rho*k*T)^2). Glitchs : un trou dans une marque ou un parasite dans un espace
//           peut etre absorbe (fusion de 3 evenements) moyennant une penalite. Caracteres = arbre Morse (127 noeuds),
//           a priori optionnel (lettres frequentes, chiffres/ponctuation penalises).
// Decodage: Viterbi sur la fenetre d'evenements non encore valides ; un caractere est valide quand `lag` evenements
//           ont ete observes apres lui (decodage a retard fixe). T et delta suivis par NLMS sur les elements valides ;
//           T initial = recherche en grille sur la fenetre. Purge (flush) apres un long silence.
// Aucune dependance Arduino : compile aussi sur PC (ml/ml_host.cpp).
#ifndef ML_MORSE_H
#define ML_MORSE_H

#include <stdint.h>
#include <math.h>
#include <string.h>

class MlMorse {
public:
  enum { MAXEV = 56, NNODE = 127, MAXOUT = 64 };

  // ---- reglages (modifiables en direct)
  float sigma0      = 30.0f;   // ms, gigue absolue des durees
  float rho         = 0.10f;   // gigue relative (fraction de k*T)
  float penGlitch   = 8.0f;    // cout (nats) d'une fusion de 3 evenements
  float glitchScale = 0.30f;   // duree moyenne d'un parasite, en unites T
  float priorW      = 0.5f;    // poids de l'a priori de caracteres (0 = uniforme)
  int   lag         = 6;       // evenements d'avance avant de valider un caractere
  float flushUnits  = 9.0f;    // silence (en T) declenchant la purge
  float flushMinMs  = 600.0f;  // ... mais au moins ce silence (ms)

  // ---- etat observable
  float unitMs   = 80.0f;      // T courant
  float delta    = 0.0f;       // biais marque(+)/espace(-)
  bool  locked   = false;
  uint32_t nChars = 0, nGlitch = 0, nEvents = 0;
  void (*eventHook)(char type, uint32_t dur) = nullptr;   // 'M' / 'S' + duree (ms), pour la capture

  MlMorse() { buildTables(); reset(); }

  void reset() {
    nEv = 0; nOut = outHead = 0; segValid = false; haveState = false; state = false;
    edgeT = 0; unitMs = 80.0f; delta = 0.0f; locked = false; sinceSearch = 0;
    nChars = nGlitch = nEvents = 0;
  }

  // a appeler a chaque iteration avec la decision courante
  void update(bool tone, uint32_t now) {
    if (!haveState) { haveState = true; state = tone; edgeT = now; return; }
    if (tone != state) {
      uint32_t d = now - edgeT;
      if (segValid) pushEvent(state ? 'M' : 'S', (float)d);
      segValid = true;
      state = tone; edgeT = now;
    } else if (!state && segValid && nEv > 0) {
      float d = (float)(now - edgeT);
      if (d > flushUnits * unitMs && d > flushMinMs) {   // long silence : valide tout
        pushEvent('S', d);
        decode(true);
        nEv = 0; segValid = false;
      }
    }
  }

  int read() { if (outHead == nOut) return -1; return out[outHead++ % MAXOUT]; }

private:
  // ---- tables de l'arbre
  char  sym[NNODE];
  bool  isChar[NNODE], isPrefix[NNODE];
  float cprior[NNODE];

  // ---- fenetre d'evenements (le premier est toujours une marque)
  float ev[MAXEV];
  int   nEv;
  bool  segValid, haveState, state;
  uint32_t edgeT;
  int   sinceSearch;

  char  out[MAXOUT]; unsigned nOut, outHead;

  // ---- Viterbi
  float   sc[MAXEV + 1][NNODE];
  uint8_t bp[MAXEV + 1][NNODE];     // noeud precedent
  uint8_t bl[MAXEV + 1][NNODE];     // etiquette (bit 7 = fusion de 3 evenements)
  enum { L_DOT = 1, L_DASH = 2, L_INTRA = 3, L_CHAR = 4, L_WORD = 5, L_MERGE = 0x80 };

  struct Group { int s, e; uint8_t lab; uint8_t node; };
  Group grp[MAXEV];

  static constexpr float NEG = -1e30f;

  void put(char c) { if (nOut - outHead < MAXOUT) { out[nOut % MAXOUT] = c; nOut++; } }

  void buildTables() {
    static const char* codes[][2] = {
      {"A", ".-"}, {"B", "-..."}, {"C", "-.-."}, {"D", "-.."}, {"E", "."}, {"F", "..-."}, {"G", "--."},
      {"H", "...."}, {"I", ".."}, {"J", ".---"}, {"K", "-.-"}, {"L", ".-.."}, {"M", "--"}, {"N", "-."},
      {"O", "---"}, {"P", ".--."}, {"Q", "--.-"}, {"R", ".-."}, {"S", "..."}, {"T", "-"}, {"U", "..-"},
      {"V", "...-"}, {"W", ".--"}, {"X", "-..-"}, {"Y", "-.--"}, {"Z", "--.."},
      {"0", "-----"}, {"1", ".----"}, {"2", "..---"}, {"3", "...--"}, {"4", "....-"}, {"5", "....."},
      {"6", "-...."}, {"7", "--..."}, {"8", "---.."}, {"9", "----."},
      {".", ".-.-.-"}, {",", "--..--"}, {"?", "..--.."}, {"'", ".----."}, {"!", "-.-.--"}, {"/", "-..-."},
      {"(", "-.--."}, {")", "-.--.-"}, {"&", ".-..."}, {":", "---..."}, {";", "-.-.-."}, {"=", "-...-"},
      {"+", ".-.-."}, {"-", "-....-"}, {"_", "..--.-"}, {"\"", ".-..-."}, {"@", ".--.-."}};
    static const float freq[26] = {8.2f, 1.5f, 2.8f, 4.3f, 12.7f, 2.2f, 2.0f, 6.1f, 7.0f, 0.15f, 0.8f, 4.0f, 2.4f,
                                   6.7f, 7.5f, 1.9f, 0.1f, 6.0f, 6.3f, 9.1f, 2.8f, 1.0f, 2.4f, 0.15f, 2.0f, 0.07f};
    for (int i = 0; i < NNODE; i++) { sym[i] = ' '; isChar[i] = false; isPrefix[i] = false; cprior[i] = 0.0f; }
    isPrefix[0] = true;
    for (unsigned k = 0; k < sizeof(codes) / sizeof(codes[0]); k++) {
      int node = 0;
      isPrefix[0] = true;
      for (const char* p = codes[k][1]; *p; p++) {
        node = 2 * node + (*p == '.' ? 1 : 2);
        isPrefix[node] = true;
      }
      char c = codes[k][0][0];
      sym[node] = c; isChar[node] = true;
      float pr;
      if (c >= 'A' && c <= 'Z') pr = 0.90f * freq[c - 'A'] / 100.0f;
      else if (c >= '0' && c <= '9') pr = 0.05f / 10.0f;
      else pr = 0.05f / 17.0f;
      cprior[node] = logf(pr);
    }
  }

  // ---- vraisemblances
  struct Model { float mu[5], sg[5], lsg[5]; float gs, lgs; float pen; } M;

  void setModel(float T, float dl) {
    float k[5] = {1, 3, 1, 3, 7};
    for (int i = 0; i < 5; i++) {
      float m = k[i] * T;
      M.mu[i] = m + (i < 2 ? dl : -dl);
      M.sg[i] = sqrtf(sigma0 * sigma0 + rho * rho * m * m);
      M.lsg[i] = logf(M.sg[i]);
    }
    M.gs = glitchScale * T; M.lgs = logf(M.gs); M.pen = penGlitch;
  }
  inline float ll(int c, float d) const {
    float z = (d - M.mu[c]) / M.sg[c];
    if (c == 4 && z > 0) z = 0;                       // espace de mot : tout ce qui depasse 7T est admis
    return -0.5f * z * z - M.lsg[c];
  }
  inline float llG(float d) const { return -d / M.gs - M.lgs; }   // parasite : loi exponentielle

  // Viterbi sur ev[0..n-1] ; renvoie le meilleur score, `bestNode` = noeud final
  float viterbi(int n, float T, float dl, bool needRoot, int& bestNode) {
    setModel(T, dl);
    for (int i = 0; i <= n; i++) for (int j = 0; j < NNODE; j++) sc[i][j] = NEG;
    sc[0][0] = 0.0f;
    for (int i = 0; i < n; i++) {
      bool mark = (i % 2) == 0;
      float d = ev[i];
      float D3 = 0.0f; bool can3 = (i + 2 < n);
      if (can3) D3 = ev[i] + ev[i + 1] + ev[i + 2];
      for (int nd = 0; nd < NNODE; nd++) {
        float s0 = sc[i][nd];
        if (s0 <= NEG * 0.5f) continue;
        if (mark) {
          for (int dash = 0; dash < 2; dash++) {
            int c = 2 * nd + 1 + dash;
            if (c >= NNODE || !isPrefix[c]) continue;
            uint8_t lab = dash ? L_DASH : L_DOT;
            float s = s0 + ll(dash, d);
            if (s > sc[i + 1][c]) { sc[i + 1][c] = s; bp[i + 1][c] = nd; bl[i + 1][c] = lab; }
            if (can3) {
              float s3 = s0 + ll(dash, D3) + llG(ev[i + 1]) - M.pen;
              if (s3 > sc[i + 3][c]) { sc[i + 3][c] = s3; bp[i + 3][c] = nd; bl[i + 3][c] = lab | L_MERGE; }
            }
          }
        } else {
          // espace : intra (reste sur le noeud), ou fin de caractere (retour a la racine)
          {
            float s = s0 + ll(2, d);
            if (s > sc[i + 1][nd]) { sc[i + 1][nd] = s; bp[i + 1][nd] = nd; bl[i + 1][nd] = L_INTRA; }
            if (can3) {
              float s3 = s0 + ll(2, D3) + llG(ev[i + 1]) - M.pen;
              if (s3 > sc[i + 3][nd]) { sc[i + 3][nd] = s3; bp[i + 3][nd] = nd; bl[i + 3][nd] = L_INTRA | L_MERGE; }
            }
          }
          if (isChar[nd]) {
            float cp = priorW * cprior[nd];
            for (int w = 0; w < 2; w++) {
              int cls = w ? 4 : 3;
              uint8_t lab = w ? L_WORD : L_CHAR;
              float s = s0 + ll(cls, d) + cp;
              if (s > sc[i + 1][0]) { sc[i + 1][0] = s; bp[i + 1][0] = nd; bl[i + 1][0] = lab; }
              if (can3) {
                float s3 = s0 + ll(cls, D3) + llG(ev[i + 1]) - M.pen + cp;
                if (s3 > sc[i + 3][0]) { sc[i + 3][0] = s3; bp[i + 3][0] = nd; bl[i + 3][0] = lab | L_MERGE; }
              }
            }
          }
        }
      }
    }
    if (needRoot) { bestNode = 0; return sc[n][0]; }
    float best = NEG; bestNode = 0;
    for (int j = 0; j < NNODE; j++) if (sc[n][j] > best) { best = sc[n][j]; bestNode = j; }
    return best;
  }

  int trace(int n, int node) {          // remplit grp[] (ordre chronologique), renvoie le nombre de groupes
    int m = 0, idx = n;
    Group tmp[MAXEV];
    while (idx > 0 && m < MAXEV) {
      uint8_t lab = bl[idx][node];
      int step = (lab & L_MERGE) ? 3 : 1;
      int pn = bp[idx][node];
      tmp[m].s = idx - step; tmp[m].e = idx; tmp[m].lab = lab & 0x7F; tmp[m].node = (uint8_t)pn;
      m++; idx -= step; node = pn;
    }
    for (int i = 0; i < m; i++) grp[i] = tmp[m - 1 - i];
    return m;
  }

  void pushEvent(char type, float d) {
    nEvents++;
    if (eventHook) eventHook(type, (uint32_t)d);
    if (nEv == 0 && type == 'S') return;                // la fenetre commence par une marque
    if (d > 20000.0f) d = 20000.0f;
    if (nEv >= MAXEV) decode(false, true);              // fenetre pleine : validation forcee
    if (nEv >= MAXEV) { memmove(ev, ev + 2, (MAXEV - 2) * sizeof(float)); nEv -= 2; }
    ev[nEv++] = d;
    decode(false);
  }

  void learn(float d, int k, int sgn) {                 // NLMS sur (T, delta)
    float e = d - (k * unitMs + sgn * delta);
    float nrm = (float)(k * k + 1) + 25.0f;
    float mu = 0.25f;
    unitMs += mu * e * k / nrm;
    delta  += mu * e * sgn / nrm;
    if (unitMs < 25.0f) unitMs = 25.0f;
    if (unitMs > 400.0f) unitMs = 400.0f;
    float lim = 0.3f * unitMs;
    if (delta > lim) delta = lim;
    if (delta < -lim) delta = -lim;
  }

  void searchT(int n, bool full) {
    int bn;
    float cur = viterbi(n, unitMs, delta, false, bn);
    float bestS = cur, bestT = unitMs;
    if (full) {
      for (float T = 28.0f; T < 330.0f; T *= 1.08f) {
        float s = viterbi(n, T, 0.0f, false, bn);
        if (s > bestS) { bestS = s; bestT = T; }
      }
      // affinage autour du meilleur
      float T0 = bestT;
      for (int i = -3; i <= 3; i++) {
        float T = T0 * (1.0f + 0.025f * i);
        float s = viterbi(n, T, 0.0f, false, bn);
        if (s > bestS) { bestS = s; bestT = T; }
      }
      unitMs = bestT; delta = 0.0f;
    } else {
      static const float r[] = {0.8f, 0.87f, 0.93f, 1.07f, 1.15f, 1.25f};
      for (unsigned i = 0; i < sizeof(r) / sizeof(r[0]); i++) {
        float T = unitMs * r[i];
        float s = viterbi(n, T, delta, false, bn);
        if (s > bestS + 3.0f) { bestS = s; bestT = T; }
      }
      unitMs = bestT;
    }
  }

  // decode la fenetre ; flush = tout valider ; force = valider tous les caracteres complets (fenetre pleine)
  void decode(bool flush, bool force = false) {
    int n = nEv;
    if (n == 0) return;
    if (!locked) {
      if (n >= 14 || (flush && n >= 8)) { searchT(n, true); locked = true; sinceSearch = 0; }
      else if (!flush) return;
    } else if (++sinceSearch >= 30 && n >= 14) { searchT(n, false); sinceSearch = 0; }

    int bn;
    viterbi(n, unitMs, delta, flush, bn);
    int m = trace(n, bn);

    int keepLag = (flush || force) ? 0 : lag;
    int commitEnd = 0, lastG = -1;
    for (int g = 0; g < m; g++)
      if ((grp[g].lab == L_CHAR || grp[g].lab == L_WORD) && (n - grp[g].e) >= keepLag) { commitEnd = grp[g].e; lastG = g; }
    if (lastG < 0) return;
    for (int g = 0; g <= lastG; g++) {
      const Group& G = grp[g];
      bool merged = (G.e - G.s) == 3;
      if (merged) nGlitch++;
      float d = merged ? (ev[G.s] + ev[G.s + 1] + ev[G.s + 2]) : ev[G.s];
      if (!merged) {
        if (G.lab == L_DOT) learn(d, 1, +1);
        else if (G.lab == L_DASH) learn(d, 3, +1);
        else if (G.lab == L_INTRA) learn(d, 1, -1);
        else if (G.lab == L_CHAR) learn(d, 3, -1);
      }
      if (G.lab == L_CHAR || G.lab == L_WORD) {
        put(sym[G.node]); nChars++;
        if (G.lab == L_WORD) put(' ');
      }
    }
    int rest = n - commitEnd;
    if (rest > 0) memmove(ev, ev + commitEnd, rest * sizeof(float));
    nEv = rest;
  }
};

#endif
