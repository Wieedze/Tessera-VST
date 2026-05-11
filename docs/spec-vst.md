# SPEC — Multi-FX Glitch / Granular VST3

> **Nom de code** : à définir (working name : `Glitchwave`)
> **Stack** : JUCE 8 + C++20 + CMake
> **Cible** : VST3, AU, Standalone, CLAP
> **Auteur** : Maxime — début dev semaine du 18 mai 2026

---

## 1. Vision produit

Un plugin multi-FX hybride qui combine quatre paradigmes DSP normalement séparés :

1. **Séquençage d'effets** (à la *Glitch 2* / *Looperator*) — un step-sequencer 16 pas qui assigne un effet par pas
2. **Granulation temps-réel** (à la *Portal*) — moteur granulaire avec freeze, pitch, reverse par grain
3. **Beat-slicing probabiliste** (à la *Livecut*) — algorithme BBCut pour découpe rythmique générative
4. **Spatialisation** (à la *Fragments*) — reverb / delay modulables pour habiller le tout

L'utilisateur capture un signal entrant dans un buffer circulaire, puis le séquenceur choisit à chaque pas s'il laisse passer le signal *dry* ou s'il applique un FX (stutter, reverse, granular, filtre, etc.) avec une probabilité paramétrable.

**Différenciation vs concurrence** : tous les paradigmes dans un seul plugin, avec un onglet *Live* dédié à la performance scénique (macros + scènes morphables), absent des concurrents.

---

## 2. Périmètre MVP

### IN scope MVP

- Capture buffer circulaire 4 bars @ 96 kHz max
- Banque de **9 FX** (voir §4)
- Step sequencer 16 pas synchro host BPM
- Modulation matrix avec 4 sources (LFO, S&H, Env Follower, Macro)
- 3 onglets UI : **Production**, **Live**, **Browser**
- Preset system (factory + user)
- Formats : VST3, AU, Standalone, CLAP
- Tests unitaires sur chaque module DSP
- Plugin validation (Steinberg validator + pluginval)

### OUT of scope MVP (= v2)

- Sidechain audio
- MIDI Out (pour driver d'autres plugins)
- Synchro à un timecode externe
- Mode polyphonique multi-voix granulaire avancé
- Cloud preset sharing
- Multi-instance linking

---

## 3. Architecture technique haut niveau

### Analogie React pour cadrer

| Concept JUCE | Équivalent React |
|---|---|
| `AudioProcessor` | App backend (logique pure, pas d'UI) |
| `AudioProcessorEditor` | Composant racine UI |
| `AudioProcessorValueTreeState` (APVTS) | Redux store / Zustand |
| `AudioParameterFloat` etc. | Atoms de state |
| `juce::ValueTree` listeners | `useEffect` sur changement state |
| Process block (callback audio) | Pas d'équivalent — c'est du temps-réel hard, RT-safe obligatoire |

### Couches du système

```
┌────────────────────────────────────────────┐
│  UI LAYER (juce::Component, OpenGL)        │
│  - ProductionTab, LiveTab, BrowserTab      │
└────────────────────────────────────────────┘
                    ▲ APVTS
                    ▼
┌────────────────────────────────────────────┐
│  STATE LAYER (APVTS + ValueTree)           │
│  - Parameters, Modulation routings, Preset │
└────────────────────────────────────────────┘
                    ▲ atomic reads
                    ▼
┌────────────────────────────────────────────┐
│  DSP ENGINE (RT-safe, lock-free)           │
│  ┌──────────────┐  ┌────────────────────┐  │
│  │ CaptureBuffer│→ │ SequencerEngine    │  │
│  └──────────────┘  └────────────────────┘  │
│         ▼                  ▼               │
│  ┌──────────────────────────────────────┐  │
│  │ FxBank (9 modules)                   │  │
│  └──────────────────────────────────────┘  │
│         ▼                                  │
│  ┌──────────────┐  ┌────────────────────┐  │
│  │ GrainEngine  │  │ ModulationMatrix   │  │
│  └──────────────┘  └────────────────────┘  │
│         ▼                                  │
│  ┌──────────────┐                          │
│  │ DryWet + Out │                          │
│  └──────────────┘                          │
└────────────────────────────────────────────┘
```

### Règles RT-safety strictes

Dans le `processBlock`, **JAMAIS** :
- d'allocation (`new`, `malloc`, `std::vector::push_back` qui réalloue)
- de mutex (`std::mutex::lock`)
- de fichier I/O
- de `std::string` qui alloue
- d'exception qui déroule la pile

Pour tout échange UI ↔ DSP : **lock-free FIFOs** (`juce::AbstractFifo` ou `moodycamel::ReaderWriterQueue`).

---

## 4. Modules DSP détaillés

### 4.1 CaptureBuffer

- Buffer circulaire stéréo, dimensionné en samples = `4 bars × 60/BPM × sampleRate × 2 channels`
- Allocation au `prepareToPlay`, jamais dans `processBlock`
- Méthodes : `write(buffer)`, `readAt(samplePos, length)`, `freeze(bool)`
- Sync au host transport pour wrap-around aligné

### 4.2 FX Bank (9 effets MVP)

Chaque FX hérite d'une interface :

```cpp
class IFxModule {
public:
    virtual void prepare(double sampleRate, int maxBlockSize) = 0;
    virtual void process(juce::AudioBuffer<float>& buffer,
                         const FxParameters& params) = 0;
    virtual void reset() = 0;
    virtual ~IFxModule() = default;
};
```

| # | Effet | Inspiration | Paramètres clés |
|---|---|---|---|
| 1 | **Stutter / Repeat** | Glitch | rate (1/2 → 1/64), gate, decay |
| 2 | **Reverser** | Glitch | window size, crossfade |
| 3 | **Tape Stop** | Glitch | speed curve, length |
| 4 | **Multimode Filter** | Fragments | cutoff, reso, type (LP/HP/BP/Notch), drive |
| 5 | **Bitcrusher** | Glitch | bit depth, sample rate reduction |
| 6 | **Gater** | Looperator | rate, shape, swing |
| 7 | **Pitch Shifter** | Portal | semitones, formant, mix |
| 8 | **Granular Cloud** | Portal | grain size, density, position, jitter, pitch range |
| 9 | **Slicer (BBCut)** | Livecut | division, probability, repeat depth |

**Spatial layer** (toujours actif, post-FX) :
- **Delay** : ping-pong, feedback, sync host
- **Reverb** : algo simple (Schroeder/FDN), size, damping, mix

### 4.3 GrainEngine (détail spécial)

Le moteur granulaire est le plus complexe, c'est lui qui justifie le "Portal-like".

- **Voix polyphoniques** : 32 voix max (pool pré-alloué)
- **Source** : lit dans le `CaptureBuffer` à une position modulable
- **Enveloppe par grain** : Hann window (cosinus simple)
- **Paramètres** :
  - `grainSizeMs` (5-500ms)
  - `density` (0.1-50 grains/sec)
  - `position` (0-1, où dans le buffer on lit)
  - `positionJitter` (random offset)
  - `pitchSemitones` (-24 à +24)
  - `pitchJitter`
  - `reverseProb` (0-1)
  - `freeze` (bool, gèle la position)

### 4.4 ModulationMatrix

- 4 sources : `LFO`, `S&H` (Sample & Hold), `EnvFollower`, `Macro` (controllable user)
- N destinations : tout paramètre des FX et du grain engine
- Per routing : `depth` (-1 to +1), `offset`, `enabled`
- Storage dans la `ValueTree` (sérialisable preset)
- Computation au taux de control rate (= 1× par sample-bloc, pas par sample) pour économiser CPU

---

## 5. Sequencer Engine

### Modèle de données

```cpp
struct Step {
    bool         enabled;
    FxType       fx;          // enum : Thru, Stutter, Reverser, etc.
    float        probability; // 0-1
    float        gateLength;  // 0-1 (fraction du pas)
    int          fxParamSnap; // index vers preset interne du FX
};

struct Pattern {
    std::array<Step, 16> steps;
    float swing;     // 0-1
    int   length;    // 1-16
    int   division;  // 1/4, 1/8, 1/16, 1/32
};
```

### Logique d'exécution

1. À chaque `processBlock`, lire `getPlayHead()->getPosition()` pour PPQ position
2. Calculer le step courant : `stepIndex = floor(ppqPos × stepsPerBeat) % patternLength`
3. Si nouveau step : tirer `random < probability`, si oui activer le FX
4. Au moment du gate-end : crossfade vers next step (8ms ramp pour éviter clicks)

### Patterns multiples + scènes

- 8 patterns A → H stockables, switchable au pas
- 2 scènes (A/B) avec morph crossfade entre les deux states (paramètre `sceneMorph`)

---

## 6. Architecture UI

### Stack UI

- JUCE Component natif + LookAndFeel custom
- OpenGL renderer activé pour fluidité (`openGLContext.attachTo(*this)`)
- Pas de WebView (décision actée — VST natif full C++)

### Onglet Production (vue principale)

Layout en 4 zones :
- **Top bar** : preset name, save/load, A/B compare, undo/redo
- **Sequencer strip** : 16 pas avec mini-LED, choix FX par dropdown, prob slider
- **FX detail panel** : params du FX sélectionné (zoom-in)
- **Modulation panel** : sources + matrix routing
- **Bottom bar** : input gain, dry/wet, output gain, CPU meter

### Onglet Live

Pour la perf scénique, **gros boutons, peu d'info, lisible à 2m** :
- 8 **macros** assignables (XY pads ou knobs géants)
- 4 **trigger pads** pour switcher de pattern à la volée
- Sélecteur scène A / B / Morph
- Gros indicateur de step actuel
- BPM + sync indicator

### Onglet Browser

- Liste de presets avec tags (`glitch`, `ambient`, `live`, `transition`…)
- Recherche fulltext
- Boutons : `Save`, `Save As`, `Delete`, `Export .preset`, `Import`

---

## 7. Système de paramètres (APVTS)

- **Toutes** les valeurs automatisables passent par `AudioProcessorValueTreeState`
- IDs versionnés : `step_0_fx`, `step_0_prob`, `fx_stutter_rate`, etc.
- Layout statique défini dans `createParameterLayout()`
- Listeners pour repaint UI sans polling

### Estimation paramètres MVP

- 16 steps × 4 params = 64
- 9 FX × ~5 params = 45
- Modulation matrix : 4 sources × ~3 params + N routings = ~30
- Globaux (gain, dry/wet, BPM, sync) : ~10
- **Total : ~150 paramètres** — gérable, mais on doit grouper proprement par catégorie pour l'automation host

---

## 8. Preset System

- Format : **XML** (natif JUCE, sérialisation `ValueTree::toXmlString()`)
- Factory presets : ~30 presets bundlés dans les BinaryData de l'IDE Projucer / CMake `juce_add_binary_data`
- User presets : `~/Documents/Glitchwave/Presets/`
- Tags stockés dans le XML
- Versionning du format pour migration future (`<preset version="1">`)

---

## 9. Build system

### CMake structure

```
project/
├── CMakeLists.txt
├── source/
│   ├── PluginProcessor.{h,cpp}
│   ├── PluginEditor.{h,cpp}
│   ├── dsp/
│   │   ├── CaptureBuffer.{h,cpp}
│   │   ├── GrainEngine.{h,cpp}
│   │   ├── ModulationMatrix.{h,cpp}
│   │   ├── SequencerEngine.{h,cpp}
│   │   └── fx/
│   │       ├── IFxModule.h
│   │       ├── Stutter.{h,cpp}
│   │       ├── Reverser.{h,cpp}
│   │       └── ...
│   ├── ui/
│   │   ├── ProductionTab.{h,cpp}
│   │   ├── LiveTab.{h,cpp}
│   │   └── BrowserTab.{h,cpp}
│   └── presets/
│       └── factory_presets.xml
├── tests/
│   ├── CMakeLists.txt
│   └── dsp/
│       ├── CaptureBuffer_tests.cpp
│       ├── GrainEngine_tests.cpp
│       └── ...
└── third_party/
    └── (vide — JUCE via FetchContent)
```

### Recommandation forte : utiliser **Pamplejuce**

[`sudara/pamplejuce`](https://github.com/sudara/pamplejuce) est un template moderne qui pré-configure :
- CMake + JUCE 8 via FetchContent
- Catch2 pour les tests unitaires
- pluginval intégré au CI
- GitHub Actions multi-OS (Mac/Windows/Linux)
- Code signing macOS / Windows
- Versioning auto

**→ Gain estimé : 1-2 semaines de setup boilerplate évité.**

### Targets JUCE

```cmake
juce_add_plugin(Glitchwave
    COMPANY_NAME "ToiCorp"
    PLUGIN_MANUFACTURER_CODE Toic
    PLUGIN_CODE Glwv
    FORMATS VST3 AU Standalone CLAP   # CLAP via clap-juce-extensions
    PRODUCT_NAME "Glitchwave"
    BUNDLE_ID com.toi.glitchwave
)
```

CLAP nécessite [`clap-juce-extensions`](https://github.com/free-audio/clap-juce-extensions) (open source, MIT).

---

## 10. Tests

### Unit tests (Catch2)

Pour chaque module DSP :
- **CaptureBuffer** : write+read round-trip, wrap correct, freeze fige bien
- **GrainEngine** : un grain produit du son, density 0 = silence, freeze fige position
- **SequencerEngine** : transport sync, step index correct au PPQ donné, probabilité statistique
- **FX modules** : chaque FX doit produire un buffer non-NaN, non-Inf, dans [-1, +1] ± headroom

### Plugin validation

- **Steinberg validator** (`validator` binary) sur chaque build VST3
- **pluginval** (Tracktion) en strict mode au CI
- Cibles validées : Ableton Live 12, Bitwig 5, Reaper 7, FL Studio 21, Logic Pro 11

### Tests d'intégration manuels (checklist beta)

- [ ] Pas de click au changement de preset
- [ ] Pas de denormals (CPU spike) sur silence prolongé
- [ ] Resize UI fluide sans memory leak
- [ ] Save/load preset round-trip parfait
- [ ] Automation host enregistre/rejoue tous les params

---

## 11. Stratégie de licence JUCE

| Phase | Licence | Action |
|---|---|---|
| Dev (semaines 1-12) | **Starter** (gratuit) | rien à payer |
| Release beta gratuite | **Starter** (gratuit) | OK tant que <$20k revenu/an |
| Release payante early | **Starter** (gratuit) | OK jusqu'à $20k cumul/an |
| Si revenu > $15k/an | **Indie** ($800 perpétuel ou $40/mois) | acheter avant de dépasser |
| Si revenu > $300k/an | **Pro** ($3500 perpétuel ou $175/mois) | upgrade |

**À ne jamais oublier** : la licence Starter exige le splash screen JUCE au lancement du standalone. Acceptable pour MVP.

---

## 12. Phasing / Milestones

**Hypothèse** : 20-25h dev/semaine en parallèle de Sofia/ARP.

| Sem. | Objectif | Livrable validable |
|---|---|---|
| 1 | Setup Pamplejuce, CMake, JUCE 8 fetch, hello-world VST3 | VST3 vide qui se charge dans Reaper |
| 2 | CaptureBuffer + 1er FX (Stutter) + tests unitaires | Stutter fonctionne sur input live |
| 3 | FX 2-5 (Reverser, TapeStop, Filter, Bitcrusher) | Banque FX testable manuellement |
| 4 | FX 6-7 (Gater, PitchShifter) + Spatial (Delay, Reverb) | Tous les FX MVP en place |
| 5 | SequencerEngine + sync host transport | 16 pas qui déclenchent des FX au tempo |
| 6 | GrainEngine (Portal-like) | Granular cloud audible |
| 7 | Slicer BBCut + ModulationMatrix | LFO module un cutoff de filtre |
| 8 | UI Production tab (sequencer strip + FX panel) | UI minimale fonctionnelle |
| 9 | UI Live tab + macros assignables | Onglet Live utilisable |
| 10 | Preset system + UI Browser tab | Save/load presets OK |
| 11 | LookAndFeel custom, OpenGL, polish UI | UI presque finale |
| 12 | Plugin validation, optim CPU, denormals fix | pluginval strict pass |
| 13 | Beta privée (10 users), feedback | itération basée feedback |
| 14 | Release v1.0 — site, payment, distribution | premier €€€ |

---

## 13. Risques techniques et plan d'atténuation

| Risque | Impact | Mitigation |
|---|---|---|
| Granular CPU trop gourmand | bloquant | profiler tôt, voice stealing, SIMD si besoin |
| Sync host transport buggé selon DAW | bloquant | tester FL/Live/Bitwig dès semaine 5, fallback BPM manuel |
| Denormals = CPU spikes | gênant | `_MM_SET_FLUSH_ZERO_MODE` au début du process |
| Memory leaks UI sur close repeated | gênant | smart pointers partout, ne pas oublier `removeAllChildren()` |
| Plugin scan crash dans Logic AU | bloquant si Mac | tester `auval -v` à chaque release Mac |
| Click au changement de preset | UX | crossfade 16ms sur tout changement de paramètre non-automatisable |
| Latence trop haute pour live | UX | mesurer + reporter au host via `setLatencySamples()` |

---

## 14. Stack de dépendances

### Obligatoires
- **JUCE 8** (Starter licence) — framework, audio, UI
- **CMake ≥ 3.22** — build
- **C++20** compiler — Clang 16+, MSVC 19.34+, GCC 12+
- **Catch2 v3** — tests unitaires
- **pluginval** — validation CI
- **Pamplejuce** template — boilerplate setup

### Optionnelles utiles
- **clap-juce-extensions** (MIT) — pour le format CLAP
- **moodycamel ReaderWriterQueue** (BSD) — FIFOs lock-free très rapides
- **gin** (gin-plugin) (par Roland Rabien, MIT) — modules DSP utilitaires de qualité (LFO, EnvelopeFollower, Easing) qui évitent de réinventer la roue

### À ne PAS utiliser (RT-unsafe)
- `std::shared_ptr` dans le path audio (atomic ops coûteuses)
- `std::function` dans `processBlock` (alloue selon le contenu)
- `juce::String` opérations dans `processBlock`

---

## 15. Ce que j'attends de Claude Code (instructions agent)

Quand tu donnes ce spec à Claude Code, demande-lui de :

1. **Démarrer par cloner Pamplejuce** et adapter le `CMakeLists.txt` au nom du projet
2. **Implémenter par couches** : CaptureBuffer → 1 FX → tests → suite. Pas de big-bang.
3. **Écrire un test unitaire AVANT chaque module DSP** (TDD light)
4. **Commit atomiques** par module, message conventionnel
5. **Vérifier la RT-safety** à chaque PR (pas d'alloc, pas de lock, pas de NaN)
6. **Logger les choix architecturaux dans `/docs/ADRs/`** (Architecture Decision Records)
7. **Toujours documenter en français** dans les commentaires d'API publique (toi tu parles français, garde la cohérence)

---

## 16. Definition of Done MVP

Le MVP est considéré "done" quand **toutes** les conditions sont vraies :

- [ ] 9 FX + granular + slicer + spatial fonctionnent sans crash
- [ ] Sequencer 16 pas synchro host à 100%
- [ ] Modulation matrix opérationnelle avec 4 sources
- [ ] 3 onglets UI (Production / Live / Browser) fonctionnels
- [ ] 30 presets factory inclus
- [ ] pluginval strict mode pass sur Mac + Win
- [ ] Couverture tests unitaires DSP > 70%
- [ ] CPU < 8% sur projet moyen (Macbook M1 ou équivalent)
- [ ] Latence reportée correcte au host
- [ ] Pas de fuite mémoire (testé avec leak detector JUCE)
- [ ] Site web minimal avec achat possible (Gumroad ou LemonSqueezy en MVP)

---

*Fin du spec. Version 0.1 — 10 mai 2026.*
