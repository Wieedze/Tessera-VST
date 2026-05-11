# Spec updates v0.2 — Modulation overhaul + Granular visualization

> Document de delta. À lire **après** `spec-vst.md`, `architecture-engines.md`, et `design-system.md` (qui restent valides). Ce doc liste tout ce qui change depuis v0.1, dans l'ordre où il faut l'implémenter.
>
> **Date** : 10 mai 2026
> **Cible** : projet en bootstrap cette semaine — toutes ces specs doivent être prises en compte dans la structure initiale du repo.

---

## Sommaire des changements

| # | Domaine | Statut | Impact |
|---|---|---|---|
| 1 | Nouvel onglet **Matrix** | nouveau | UI majeure |
| 2 | ModulationMatrix architecture v2 (9 champs) | refonte | DSP majeure |
| 3 | Sources de modulation : 4 → 17 | extension | DSP modérée |
| 4 | LFO shapes étendus (sine → 10 types incl. chaos + drawable) | extension | DSP modérée |
| 5 | Modes LFO (FREE/RETRIG/ENV) + direction + modifiers | extension | DSP mineure |
| 6 | Curves drawable custom | nouveau | UI complexe |
| 7 | Chaos sources (Lorenz + Rössler) | nouveau | DSP simple |
| 8 | Production tab : suppression spectrum + wave in/out | suppression | UI mineure |
| 9 | Production tab : ajout "Engine Activity" panel | nouveau | UI majeure |
| 10 | Granular activity visualization (têtes de lecture animées) | nouveau | UI complexe + DSP |
| 11 | Per-FX overlays sur capture buffer waveform | nouveau | UI modérée |

---

## 1. Nouvel onglet **Matrix**

L'app a maintenant **4 onglets** (au lieu de 3) :

```
PROD  |  LIVE  |  MATRIX  |  PRESETS
```

Le Matrix tab contient :
- **Haut (≈55% de la hauteur)** : grille de routings 9 colonnes (voir §2)
- **Bas (≈45% de la hauteur)** : éditeur unifié pour la source de modulation sélectionnée (voir §4)

Layout général identique à `mockup-matrix.html` fourni.

---

## 2. ModulationMatrix : architecture v2

### Ancienne struct (v0.1, à remplacer)

```cpp
// OBSOLÈTE - ne pas implémenter
struct ModRouting {
    ModSource source;
    ModDest   dest;
    float     depth;
    float     offset;
    bool      enabled;
};
```

### Nouvelle struct (v0.2)

```cpp
enum class CurveType {
    Linear, ExpUp, ExpDown, SCurve, InvertS, Quantize4, Quantize8, Custom
};

enum class Polarity { Unipolar, Bipolar };

struct ModRouting {
    bool        enabled        = false;
    ModSource   source         = ModSource::None;
    CurveType   sourceCurve    = CurveType::Linear;
    float       amount         = 0.0f;     // -1.0 to +1.0 (bipolar)
    Polarity    polarity       = Polarity::Bipolar;
    ModDest     destination    = ModDest::None;
    ModSource   auxSource      = ModSource::None;  // optional mod-of-mod
    bool        invert         = false;
    CurveType   outputCurve    = CurveType::Linear;
    float       outputLevel    = 1.0f;     // 0.0 to 1.0 (final scaling)
};
```

### Pipeline de calcul d'un routing (RT-safe, control rate)

```cpp
float ModulationMatrix::computeRouting(const ModRouting& r,
                                        std::array<float, NumSources>& sources)
{
    if (!r.enabled || r.source == ModSource::None || r.destination == ModDest::None)
        return 0.0f;

    // 1. Lire la source
    float v = sources[(int)r.source];

    // 2. Appliquer la courbe d'entrée
    v = applyCurve(r.sourceCurve, v);

    // 3. Calculer l'amount, modulé par aux source si présent
    float amt = r.amount;
    if (r.auxSource != ModSource::None) {
        amt *= sources[(int)r.auxSource];
    }

    // 4. Multiplier
    float modVal = v * amt;

    // 5. Invert
    if (r.invert) modVal = -modVal;

    // 6. Courbe de sortie
    modVal = applyCurve(r.outputCurve, modVal);

    // 7. Output level
    modVal *= r.outputLevel;

    // 8. Polarité
    if (r.polarity == Polarity::Unipolar) {
        modVal = (modVal + 1.0f) * 0.5f;  // remap [-1,+1] → [0,1]
    }

    return modVal;
}
```

### Application des routings aux destinations

```cpp
void ModulationMatrix::process(int numSamples) {
    // 1. Update des sources (LFO ticks, envs, S&H, chaos, etc.)
    updateSources(numSamples);

    // 2. Reset destinations aux valeurs de base APVTS
    for (int d = 0; d < NumDests; ++d) {
        modulatedValues[d] = baseValues[d];
    }

    // 3. Accumuler les contributions de chaque routing
    for (const auto& routing : routings) {
        float contribution = computeRouting(routing, currentSourceValues);
        modulatedValues[(int)routing.destination] += contribution * destRange[(int)routing.destination];
    }

    // 4. Clamp aux ranges valides
    for (int d = 0; d < NumDests; ++d) {
        modulatedValues[d] = clampToRange(d, modulatedValues[d]);
    }
}
```

### Fonction applyCurve

```cpp
float applyCurve(CurveType type, float v) {
    // v est dans [-1, +1]
    switch (type) {
        case CurveType::Linear:    return v;
        case CurveType::ExpUp:     return std::copysign(v * v, v);              // power curve
        case CurveType::ExpDown:   return std::copysign(std::sqrt(std::abs(v)), v);
        case CurveType::SCurve:    return v * v * (3.0f - 2.0f * std::abs(v));  // smoothstep
        case CurveType::InvertS:   return v < 0 ? -std::sqrt(-v) : std::sqrt(v);
        case CurveType::Quantize4: return std::round(v * 2.0f) * 0.5f;          // 4 steps
        case CurveType::Quantize8: return std::round(v * 4.0f) * 0.25f;         // 8 steps
        case CurveType::Custom:    return readCustomCurve(v);                   // table lookup
    }
    return v;
}
```

### Capacité

- **32 routings actifs maximum** (pré-alloués dans un `std::array<ModRouting, 32>`)
- Plus que ça pèse sur le CPU et l'UX (illisible)
- Les routings inactifs (`enabled = false`) sont skippés en early-return

---

## 3. Sources de modulation : passage de 4 à 17 sources

### Liste complète

```cpp
enum class ModSource {
    None,
    LFO1, LFO2, LFO3, LFO4,         // 4 LFOs indépendants
    ENV1, ENV2,                       // 2 enveloppes ADSR
    SampleHold,                       // S&H avec rate sync
    ChaosA, ChaosB,                   // Lorenz + Rössler
    EnvFollower,                      // suit l'amplitude input
    Macro1, Macro2, Macro3, Macro4,
    Macro5, Macro6, Macro7, Macro8    // 8 macros UI
};
```

Total : **18 entrées** (incluant `None`), donc 17 sources réelles.

### Pourquoi ces choix

| Source | Rôle | Pourquoi |
|---|---|---|
| 4× LFO | Modulation périodique synchronisée | 4 c'est le sweet spot — 2 c'est limitant, 6+ c'est de la cosmétique. Suffit pour stack rate × shape × phase variantes. |
| 2× ENV | One-shot ADSR sur trigger host | Permet d'attacher des évolutions à des notes ou pulses MIDI. |
| S&H | Random rythmique | Classique en modular, échantillonne du noise à un tempo. |
| 2× CHAOS | Modulation non-répétitive organique | Le différenciateur "psytrance/expérimental" — le chaos déterministe donne des évolutions qui ne se répètent jamais mais restent musicales. |
| Env Follower | Modulation par l'audio entrant | Permet du sidechain creative (la kick module le filter cutoff). |
| 8× Macro | Performance + automation host | 8 macros = OK pour un live tab à 4-8 contrôles principaux + automation. |

---

## 4. LFO shapes : 10 types

### Liste

| # | Shape | Code | Notes implémentation |
|---|---|---|---|
| 1 | Sine | `std::sin(phase * twoPi)` | trivial |
| 2 | Triangle | `1 - 4*\|phase - 0.5\|` | trivial |
| 3 | Saw up | `2 * phase - 1` | trivial |
| 4 | Saw down | `1 - 2 * phase` | trivial |
| 5 | Square | `phase < 0.5 ? 1 : -1` | trivial |
| 6 | Pulse (PWM) | `phase < width ? 1 : -1` | param `width` 0.05-0.95 |
| 7 | Noise (white) | `rng.nextFloat() * 2 - 1` | seedable pour reproductibilité |
| 8 | Sample & Hold | snap au rate, hold entre | voir §4.7 |
| 9 | **Lorenz (chaos)** | équations différentielles 3D | voir §4.8 |
| 10 | **Custom (drawable)** | lookup table avec interp | voir §4.9 |

### 4.7 Sample & Hold

```cpp
class SampleHoldSource {
    float currentValue = 0.0f;
    double phaseAccum = 0.0;
    juce::Random rng;
public:
    float tick(double rate, double sampleRate) {
        phaseAccum += rate / sampleRate;
        if (phaseAccum >= 1.0) {
            phaseAccum -= 1.0;
            currentValue = rng.nextFloat() * 2.0f - 1.0f;
        }
        return currentValue;
    }
};
```

### 4.8 Lorenz attractor (CHAOS A)

Équations différentielles classiques :

```
dx/dt = σ(y - x)
dy/dt = x(ρ - z) - y
dz/dt = xy - βz
```

Avec valeurs canoniques `σ=10, ρ=28, β=8/3` qui donnent l'attracteur "papillon" classique.

```cpp
class LorenzSource {
    double x = 0.1, y = 0.0, z = 0.0;
    double sigma = 10.0;
    double rho = 28.0;
    double beta = 8.0 / 3.0;
public:
    void prepare(double sampleRate) {
        // Reset à un état initial valide (pas tous les zeros !)
        x = 0.1; y = 0.0; z = 0.0;
    }

    float tick(double rate, double sampleRate) {
        // dt contrôle la "vitesse" du chaos. Rate normale = 0.005,
        // rate*4 = chaos rapide, rate/4 = lent et organique.
        const double dt = rate * 0.005 / sampleRate;

        const double dx = sigma * (y - x);
        const double dy = x * (rho - z) - y;
        const double dz = x * y - beta * z;

        x += dx * dt;
        y += dy * dt;
        z += dz * dt;

        // x oscille typiquement entre -20 et +20. Normalize à [-1, +1].
        return static_cast<float>(juce::jlimit(-1.0, 1.0, x / 20.0));
    }
};
```

**Pièges Lorenz** :
- Si dt est trop grand → divergence numérique (les valeurs explosent à NaN)
- **Toujours** valider avec `std::isfinite()` et reset si NaN détecté
- Le système prend ~100-1000 itérations pour rentrer sur l'attracteur ; warm-up au `prepareToPlay`

### 4.9 Rössler attractor (CHAOS B)

Plus simple que Lorenz, oscillations plus régulières :

```
dx/dt = -y - z
dy/dt = x + ay
dz/dt = b + z(x - c)
```

Avec `a=0.2, b=0.2, c=5.7`.

```cpp
class RosslerSource {
    double x = 0.1, y = 0.0, z = 0.0;
    const double a = 0.2, b = 0.2, c = 5.7;
public:
    float tick(double rate, double sampleRate) {
        const double dt = rate * 0.01 / sampleRate;
        const double dx = -y - z;
        const double dy = x + a * y;
        const double dz = b + z * (x - c);
        x += dx * dt;
        y += dy * dt;
        z += dz * dt;
        return static_cast<float>(juce::jlimit(-1.0, 1.0, x / 10.0));
    }
};
```

### 4.10 Custom drawable curve

Stockage en tableau de breakpoints avec phase normalisée [0, 1] :

```cpp
struct DrawableCurve {
    std::array<float, 64> points{};  // valeurs en [-1, +1]
    int numActivePoints = 8;          // user-adjustable: 8, 16, 32, 64
    bool smooth = true;               // false = stepped/quantized

    float read(float phase) const {
        phase = std::fmod(phase, 1.0f);
        if (phase < 0) phase += 1.0f;

        const float idx = phase * (numActivePoints - 1);
        const int i0 = static_cast<int>(idx) % numActivePoints;
        const int i1 = (i0 + 1) % numActivePoints;
        const float frac = idx - std::floor(idx);

        if (smooth) {
            return points[i0] + frac * (points[i1] - points[i0]);
        }
        return points[i0];
    }

    void setPoint(int index, float value) {
        points[index] = juce::jlimit(-1.0f, 1.0f, value);
    }
};
```

Sérialisation dans la `ValueTree` du preset :
```xml
<lfo id="1" shape="custom" smooth="true" numPoints="8">
    <points>-1.0,-0.5,1.0,0.5,-1.0,-0.5,1.0,0.5</points>
</lfo>
```

---

## 5. Modes, direction, modifiers LFO

### Mode (3 valeurs)

| Mode | Comportement |
|---|---|
| **FREE** | Tourne en continu, ne se reset jamais |
| **RETRIG** | Reset phase à 0 sur chaque trigger MIDI/host transport |
| **ENV** | One-shot — joue UNE fois (de phase 0 à 1) puis s'arrête |

```cpp
enum class LfoMode { Free, Retrig, OneShot };
```

### Direction (4 valeurs)

| Direction | Effet sur la phase |
|---|---|
| **Forward** | Phase 0 → 1 → 0 (normal) |
| **Reverse** | Phase 1 → 0 (lit la shape à l'envers) |
| **PingPong** | 0→1→0→1→0 (forward puis reverse alternativement) |
| **Random** | À chaque cycle, choix aléatoire forward/reverse |

```cpp
enum class LfoDirection { Forward, Reverse, PingPong, Random };
```

### Modifiers (4 paramètres post-shape)

```cpp
struct LfoModifiers {
    float smoothMs = 0.0f;     // 0 to 50ms — lowpass sur la sortie
    float phaseDeg = 0.0f;     // 0 to 360° — décalage de phase
    float riseSec  = 0.0f;     // 0 to 5s — fade-in de l'amplitude
    float delaySec = 0.0f;     // 0 to 5s — délai avant démarrage
};
```

### Application chain

```
raw_shape(phase) → smooth → rise_envelope → return
                    ↑
                  phase shifted by phaseDeg
                  delay applied at trigger
```

---

## 6. Production tab : redesign visualisation

### Suppressions

- ~~Spectrum analyzer~~ (jamais utile sur un FX granulaire de glitch)
- ~~Wave in/out display~~ (redondant avec le DAW)

### Ajout : "Engine Activity" panel

Un **gros panneau unifié** qui occupe la zone précédemment partagée par spectrum et wave in/out (~110px de haut, pleine largeur).

Ce panneau affiche **toujours** :
- La **waveform du capture buffer** (les 4 bars en mémoire) en signal-primary
- Le **write head** : ligne verticale cyan claire qui défile en temps réel (où est en train d'être écrit le signal entrant)
- Une zone "stale" (passée la limite du buffer) visuellement plus sombre

Selon le **FX actif courant** (déterminé par le sequencer), un **overlay spécifique** se superpose :

| FX actif | Overlay affiché |
|---|---|
| Granular | tête de lecture principale + N grain heads + spread zone (voir §6.1) |
| Stutter | crochets de loop region (capture window répétée) |
| Reverser | flèche reverse + zone de lecture inversée |
| TapeStop | gradient de pitch (decreasing) |
| Slicer | marqueurs de slices verticaux |
| Filter / Bitcrusher / etc. | overlay neutre (juste le write head) |

### 6.1 Granular overlay (le détail attendu)

Reproduit le paradigme Pigments/Serum 2 mais adapté au capture buffer vivant.

#### Éléments à afficher

```
┌──────────────────────────────────────────────────────────────┐
│  ░░░░░░░░░░ [WAVEFORM CAPTURE BUFFER STÉRÉO] ░░░░░░░░░░░░░  │ ← waveform sombre
│            ◄══════════════════════════════►                  │ ← brackets loop region
│            ░▒▓███ [SPREAD ZONE TRANSLUCIDE] ███▓▒░          │ ← jitter range
│            │     │    │    │       │       │                 │ ← grain heads
│            │     │    ║    │       │       │   ║ ← write head│
│  ▁▂▃▅▇█▇▅▃▂▁▂▃▅▇█▇▅▃▂▁▂▃▅▇█▇▅▃▂▁▂▃▅▇█▇▅▃▂▁                  │
└──────────────────────────────────────────────────────────────┘
                  ↑                              ↑
              spawn cursor                    write head
              (jaune)                         (cyan claire)
```

#### Spec détaillée par élément

| Élément | Visuel | Semantic |
|---|---|---|
| **Waveform** | trait fin signal-primary à 60% opacity | les 4 bars du capture buffer |
| **Write head** | ligne 2px cyan vif (`--signal-active`) animée à droite-vers-gauche en wrap | position d'écriture en temps réel |
| **Loop brackets** | 2 chevrons bleus en haut, reliés par fine ligne | région où les grains peuvent spawner (params `position - range/2` à `position + range/2`) |
| **Spawn cursor** | ligne 1.5px jaune (#FFE066) animée selon `position` modulé | centre de la zone de spawn |
| **Spread zone** | gradient violet translucide (10-20% opacity) en trapèze | range du `positionJitter` |
| **Grain heads** | N lignes 1px blanches | 1 par voix granulaire active (max 32) |
| **Pitch indicator** | petite encoche horizontale sur chaque grain head, hauteur Y selon pitch | pitch du grain (centre = 0 semitones) |
| **Direction arrows** | mini ▶ ou ◀ en bas du grain head | reverse flag |

#### Données nécessaires depuis le DSP (vers UI thread)

Le moteur granulaire doit exposer (atomic / lock-free FIFO) :

```cpp
struct GrainViewData {
    int64_t writePos;                       // position d'écriture courante
    float   spawnPosition;                   // 0-1, position du curseur principal
    float   spawnPositionRange;              // 0-1, taille du spread (= jitter)
    int     activeVoiceCount;                // 0-32

    struct VoiceState {
        float position;    // 0-1, position dans le buffer
        float pitchSemis;  // pitch courant
        bool  reverse;
        float amplitude;   // pour fade-in/out visuel
    };
    std::array<VoiceState, 32> voices;
};
```

Update à 30 Hz minimum côté UI (pas 60 — c'est de la viz, pas critique).

---

## 7. Updated source tree

Nouveaux fichiers à créer :

```
source/
├── dsp/
│   ├── ModulationMatrix.{h,cpp}        ← refonte v2
│   ├── sources/                         ← nouveau dossier
│   │   ├── ModSourceBase.h
│   │   ├── LfoSource.{h,cpp}            ← gère 10 shapes + modes + dirs + mods
│   │   ├── EnvelopeSource.{h,cpp}       ← ADSR
│   │   ├── SampleHoldSource.{h,cpp}
│   │   ├── LorenzSource.{h,cpp}
│   │   ├── RosslerSource.{h,cpp}
│   │   ├── EnvelopeFollower.{h,cpp}
│   │   ├── DrawableCurve.{h,cpp}
│   │   └── CurveTransform.{h,cpp}       ← applyCurve()
│   └── viz/                             ← nouveau dossier (data exporters UI)
│       ├── GrainViewExporter.{h,cpp}    ← snapshot lock-free pour UI
│       └── CaptureBufferViewExporter.{h,cpp}
└── ui/
    ├── tabs/
    │   ├── ProductionTab.{h,cpp}
    │   ├── LiveTab.{h,cpp}
    │   ├── MatrixTab.{h,cpp}            ← nouveau
    │   └── BrowserTab.{h,cpp}
    ├── matrix/                           ← nouveau dossier
    │   ├── ModRoutingRow.{h,cpp}
    │   ├── ModMatrixGrid.{h,cpp}
    │   ├── LfoEditor.{h,cpp}
    │   ├── DrawableCurveEditor.{h,cpp}  ← canvas avec drag handles
    │   └── ShapePresetGrid.{h,cpp}
    └── viz/                              ← nouveau dossier
        ├── EngineActivityPanel.{h,cpp}
        ├── GranularOverlay.{h,cpp}
        ├── StutterOverlay.{h,cpp}
        ├── ReverserOverlay.{h,cpp}
        ├── TapeStopOverlay.{h,cpp}
        └── SlicerOverlay.{h,cpp}

tests/dsp/sources/
├── LfoSource_tests.cpp
├── LorenzSource_tests.cpp                ← critique : valider stabilité
├── DrawableCurve_tests.cpp
└── ModulationMatrix_tests.cpp
```

---

## 8. Updated phasing

Le plan original tenait en 14 semaines. Avec ces ajouts, il faut compter **2-3 semaines de plus**, soit ~16 semaines.

| Semaine | Avant (v0.1) | Maintenant (v0.2) |
|---|---|---|
| 5 | SequencerEngine | inchangé |
| 6 | GrainEngine | inchangé |
| 7 | Slicer + ModulationMatrix simple | **Slicer + ModMatrix v2 (9 champs)** |
| 8 | UI Production tab | UI Production v1 (sans viz finale) |
| **9** | UI Live tab | **Sources de modulation (10 LFO shapes + chaos + drawable)** |
| **10** | Preset system | **LfoEditor UI + DrawableCurveEditor** |
| **11** | LookAndFeel polish | **Matrix tab UI complet** |
| **12** | Validation pluginval | **EngineActivityPanel + Granular overlay** |
| **13** | Beta privée | **Per-FX overlays (Stutter/Reverser/TapeStop/Slicer)** |
| 14 | Release | UI Live tab |
| 15 | — | Preset system + Browser tab |
| 16 | — | Validation, polish, beta privée |

→ Release v1.0 visée **fin août 2026** au lieu de fin juillet.

---

## 9. Updated risks

Nouveaux risques à monitorer :

| Risque | Impact | Mitigation |
|---|---|---|
| Lorenz/Rössler diverge en NaN | bloquant son | `isfinite()` check + reset auto, validation unit test sur 10M samples |
| DrawableCurve UX confuse au drag | frustration | bezier handles, snap à grid, undo/redo, comparable à Serum 2 |
| Matrix 32 routings ralentit le control rate | CPU | profiler ; fallback : early-skip routings désactivés (déjà prévu) |
| Granular overlay gourmand en repaints | UI lag | clipping, dirty regions, OpenGL renderer obligatoire |
| Trop de sources/destinations → UX illisible | adoption | filtre/recherche dans le destination dropdown, presets de routings communs |

---

## 10. Updated Definition of Done MVP

Ajout aux conditions du DoD initial :

- [ ] **Matrix** tab : 32 routings configurables avec les 9 champs
- [ ] **17 sources** disponibles (4 LFO + 2 ENV + S&H + 2 Chaos + EnvFoll + 8 Macro)
- [ ] LFO supporte les **10 shapes** (incluant Lorenz et Drawable)
- [ ] Lorenz et Rössler validés stables sur 10M samples sans NaN
- [ ] DrawableCurve éditable avec 8/16/32/64 points + smooth toggle
- [ ] AUX SOURCE fonctionnel (mod-de-mod testé : LFO×Macro)
- [ ] Polarité uni/bi togglable par routing
- [ ] Source curves + output curves (8 types) appliquées correctement
- [ ] **Granular overlay** affiche : waveform + write head + brackets + spawn cursor + spread zone + N grain heads
- [ ] **Per-FX overlays** : Stutter, Reverser, TapeStop, Slicer chacun avec son visuel
- [ ] Refresh des overlays à 30 Hz minimum, sans bloquer le DSP

---

## Notes pour Claude Code

Quand tu reprendras ce doc :

1. **Implémente la matrix v2 AVANT les sources étendues**. La struct `ModRouting` doit être figée tôt parce qu'elle conditionne le format de preset.
2. **Code Lorenz + tests AVANT le LfoEditor UI**. Le DSP doit être stable et validé avant qu'on bricole l'interface.
3. **Le DrawableCurve a deux faces** : la struct DSP (simple) et l'éditeur UI (complexe). Code la struct + tests d'abord, l'éditeur seulement quand c'est validé.
4. **GrainViewExporter** doit être implémenté **en même temps** que le GrainEngine, pas après. Sinon tu auras du mal à exposer les données proprement (faut que les voix exposent leur état).
5. Les **overlays per-FX** sont du sucre — on peut shipper un MVP avec juste l'overlay Granular + un overlay neutre (write head only) pour les autres FX. Ne bloque pas la release pour ça.

---

*v0.2 — 10 mai 2026. Itération sur design system + spec en parallèle de Production tab v2 mockup.*
