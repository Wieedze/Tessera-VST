# Progress tracker — Maxime apprend le C++ via Tessera

Suivi en temps réel : ce qui est vu, ce qui est compris, ce qui reste.

Légende :
- `[ ]` : pas commencé
- `[~]` : en cours / vu mais flou
- `[x]` : maîtrisé (peut l'expliquer + checkpoint validé)

---

## Concepts C++

### Niveau 1 — Fondamentaux

- [ ] Value / reference / pointer (`T`, `T&`, `T*`)
- [~] `const` correctness (sait que `const` sur méthode = "ne modifie pas l'objet" ; **piège à clarifier** : `const` ≠ thread safety, checkpoint 2026-05-12)
- [~] RAII et destructeurs automatiques (vu via JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR et destruction auto de membres value)
- [x] `std::array` vs `std::vector` — choix de container (exo 3.2 : array, taille fixe à la compile)
- [x] Stack vs heap — où vit quoi (array sur la pile, atomic membre = dans l'objet)
- [ ] `auto` et déduction de type
- [ ] `nullptr` vs `NULL` vs `0`
- [~] Headers vs implementation (`.h` / `.cpp`), include guards (vu `#pragma once`, séparation déclaration/implémentation, `static constexpr` dans header)
- [x] Namespaces et `::` (std::, ::-resolution, `using namespace` à éviter)
- [~] `noexcept` (vu : contrat "ne lance jamais d'exception", utilisé sur les méthodes RT-safe)
- [~] `static_cast<T>` (vs conversion implicite, vs `std::floor` pour valeurs négatives)
- [~] C++ name hiding et `using Base::method` (warning `-Woverloaded-virtual`, fix `using juce::AudioProcessor::processBlock`)

### Niveau 2 — Types et templates

- [ ] Templates de fonction
- [ ] Templates de classe
- [ ] `concepts` (C++20)
- [ ] `std::span<T>` comme view
- [ ] `std::optional<T>` pour valeurs absentes
- [ ] Move semantics (`std::move`, rvalue refs `T&&`)
- [ ] `std::unique_ptr<T>` vs `std::shared_ptr<T>` — pourquoi shared est interdit RT

### Niveau 3 — Threading et atomics

- [~] `std::atomic<T>` — load/store (vu via exo 3.2 + CaptureBuffer ; **distinction range vs atomicity à renforcer** — checkpoint 2026-05-12 Q1)
- [ ] Memory ordering (`relaxed`, `acquire`, `release`, `seq_cst`)
- [ ] Pourquoi `std::mutex` est interdit dans `processBlock`
- [ ] Lock-free SPSC FIFO (concept)
- [~] Data race vs race condition (**confusion avec `const` détectée** — checkpoint 2026-05-12 Q2, à revoir)

### Niveau 4 — Polymorphisme

- [ ] `virtual` et vtable
- [ ] Pure virtual + interfaces (`IFxModule`)
- [ ] `static_cast` vs `dynamic_cast`
- [ ] Pourquoi `dynamic_cast` est interdit dans `processBlock`

### Niveau 5 — DSP

- [x] Ring buffer + interpolation linéaire (CaptureBuffer implémenté + 8 tests + bonne réponse au checkpoint Q4 sur `std::floor` vs `static_cast`)
- [ ] Phase accumulation (LFO)
- [ ] Hann envelope
- [ ] Voice pooling + voice stealing
- [ ] Forward Euler integration (Lorenz, Rössler)
- [ ] Denormals et flush-to-zero
- [ ] Convolution / IR filtering
- [ ] FFT / overlap-add

### JUCE-spécifique

- [~] `juce::AudioBuffer<float>` : `getReadPointer`/`getWritePointer` vs `getSample`/`setSample` (**piège perf détecté** — checkpoint Q3, à revoir : raw pointers = perf, pas sécurité)
- [~] `juce::ScopedNoDenormals` comme RAII guard en première ligne de processBlock
- [~] `JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR` : supprime copy + active leak detector

---

## Exercices faits

Tracking de [`exercises.md`](exercises.md).

- [ ] 1.1 — Value, reference, pointer
- [ ] 1.2 — RAII et lifetimes
- [ ] 1.3 — `const` partout
- [ ] 2.1 — `std::array` vs `std::vector`
- [ ] 2.2 — `std::span` comme view
- [ ] 3.1 — `std::atomic<int>` counter
- [x] 3.2 — Ring buffer minimaliste (validé 2026-05-10, 6 étapes pas-à-pas)

---

## Modules Tessera

Suit la phasing dans `docs/spec-vst.md` §12 (étendu par `spec-updates-v0.2.md` §8).

### Semaine 1 — Bootstrap + CaptureBuffer

- [x] Pamplejuce cloné, CMake adapté à Tessera (Tssr / MxLb / com.maxlab.tessera)
- [x] Build VST3 + CLAP + Standalone passe sur Linux (gcc 13.3, JUCE 8.0.12)
- [x] Plugin installé auto dans `~/.vst3/Tessera.vst3`
- [x] `CaptureBuffer` implémenté : ring buffer stéréo 32s + interpolation linéaire + freeze + wrap-around
- [x] Tests `CaptureBuffer_tests.cpp` passent : 8 cases, 6029 assertions (round-trip, wrap, freeze, interp, RT-safety stress)
- [x] `processBlock` capture l'input via `captureBuffer.write(buffer)` après `ScopedNoDenormals` ; bypass parfait
- [x] Checkpoint quiz `cpp-mentor` semaine 1 — 1.5/4 (zones à renforcer : threading/atomic + JUCE perf patterns)

### Semaine 2 — FxBank + Stutter

- [x] `IFxModule` interface (pure virtual + FxType enum + FxParams struct)
- [x] `ThruFx` — bypass FX, preuve du polymorphisme via `unique_ptr<IFxModule>`
- [x] `FxBank` — registry pré-alloué `std::array<unique_ptr<IFxModule>, kNumFxTypes>`
- [x] `StutterFx` — 1er FX musical (loop 1/N note depuis CaptureBuffer)
- [x] `PluginProcessor` câblé : APVTS (fx_type, stutter_rate) + cached atomic ptrs + dispatch via FxBank
- [x] 20 test cases, 6220 assertions, build clean (VST3 + CLAP + Standalone)
- [~] Checkpoint quiz `cpp-mentor` semaine 2 — **skipped 2026-05-12, à revisiter** (4 questions reportées plus bas)

### Semaine 3 — FX 2-5 (Reverser, TapeStop, Filter, Bitcrusher) + quality pass

Plan détaillé : [docs/sprint-w3-plan.md](../sprint-w3-plan.md). Une feature branch par phase (cf. `.claude/rules/git-style.md` §"Feature branches").

- [ ] **S1** `feat/workflow-install-script` — désactiver COPY_PLUGIN_AFTER_BUILD + PowerShell helper
- [ ] **S2** `feat/fx-reverser` — Reverser FX + tests + APVTS
- [ ] **S3** `feat/fx-tapestop` — TapeStop FX (variable speed, curve)
- [ ] **S4** `feat/fx-filter` — Filter FX (juce::dsp::StateVariableTPTFilter)
- [ ] **S5** `feat/fx-bitcrusher` — Bitcrusher FX (quantize + S&H)
- [ ] **S6** `feat/quality-smoothing-crossfade` — Parameter smoothing partout + crossfade FxType + denormals
- [ ] **S7** `feat/quality-validation` — pluginval strict pass + checkpoint cpp-mentor W3 + merge dev→main

### Semaine 4 — FX 6-7 + Spatial

- [ ] Gater FX (cf. spec-vst.md §4.2 #6)
- [ ] PitchShifter FX (lit CaptureBuffer avec pitchRatio != 1)
- [ ] Spatial layer : DelayFx + ReverbFx (toujours actifs, post-FX)

### Semaine 5 — SequencerEngine

- [ ] 16-step pattern, sync host PPQ (cf. architecture-engines.md §2)
- [ ] Edge detection (1 tirage par step)
- [ ] Crossfade FX switch déjà fait W3-S6
- [ ] Swing, length, division

### Semaine 6 — GrainEngine + GrainViewExporter (couplés, cf. v0.2)

- [ ] Pool 32 voix granulaires (cf. architecture-engines.md §4)
- [ ] Spawn schedule, voice stealing
- [ ] Enveloppe Hann par grain
- [ ] GrainViewExporter (atomic snapshot pour UI) — implémenté en parallèle

### Semaine 7 — Slicer + ModulationMatrix v2 (9 fields)

- [ ] Slicer BBCut
- [ ] ModRouting struct **v2** (9 champs : enabled, source, sourceCurve, amount, polarity, dest, auxSource, invert, outputCurve, outputLevel) — cf. spec-updates-v0.2.md §2
- [ ] **Lock la struct AVANT le preset format** (lesson 0002)
- [ ] 32 routings max, std::array pré-alloué

### Semaine 8 — UI Production tab v1 (sans viz finale)

- [ ] Custom LookAndFeel `TesseraLookAndFeel : juce::LookAndFeel_V4`
- [ ] Step sequencer strip (16 cells)
- [ ] FX detail panel
- [ ] Macros panel
- [ ] Pas encore les overlays granulaire/FX

### Semaine 9 — Sources étendues (v0.2)

- [ ] 4 LFO indépendants (vs 1 dans v0.1)
- [ ] 10 LFO shapes (Sine, Triangle, Saw, Square, Pulse, Noise, S&H, Lorenz, Rössler, Custom drawable)
- [ ] **Lorenz + tests stabilité (10M samples, isfinite) AVANT le LfoEditor UI** (lesson 0001)
- [ ] Modes (FREE/RETRIG/ENV) + direction + modifiers
- [ ] 2 ENV ADSR + S&H + EnvFollower + 8 Macros

### Semaine 10 — LfoEditor UI + DrawableCurveEditor (v0.2)

- [ ] LfoEditor : visualisation phase + shape preview
- [ ] DrawableCurveEditor : canvas avec drag handles, snap à grid, undo/redo

### Semaine 11 — Matrix tab UI complet (v0.2)

- [ ] ModRoutingRow component
- [ ] ModMatrixGrid (32 routings)
- [ ] Drag-to-modulate depuis sources → knobs

### Semaine 12 — EngineActivityPanel + Granular overlay (v0.2)

- [ ] Waveform du capture buffer en temps réel
- [ ] Write head animé
- [ ] Granular overlay : têtes de lecture par voix, spawn cursor, spread zone
- [ ] OpenGL renderer obligatoire pour fluidité

### Semaine 13 — Per-FX overlays (v0.2)

- [ ] Stutter, Reverser, TapeStop, Slicer chacun avec son overlay sur la waveform
- [ ] **MVP peut shipper sans ces overlays** si nécessaire (cf. spec-updates-v0.2.md note 5)

### Semaine 14 — UI Live tab

- [ ] Layout pour perf scénique (gros boutons, lisible à 2m)
- [ ] 8 macros XY pads
- [ ] 4 trigger pads patterns
- [ ] Step indicator géant

### Semaine 15 — Preset system + Browser tab

- [ ] APVTS ValueTree sérialisable en XML
- [ ] Factory presets (30) embedded dans BinaryData
- [ ] User presets dans `~/Documents/Tessera/Presets/`
- [ ] Tags + recherche fulltext
- [ ] Save / Save As / Delete / Export

### Semaine 16 — Validation, polish, beta privée

- [ ] pluginval strict sur Mac + Windows
- [ ] auval sur Mac (`auval -v aufx Tssr MxLb`)
- [ ] Profiler CPU < 8% sur projet moyen
- [ ] 10 beta testers
- [ ] Site web + Gumroad/LemonSqueezy
- [ ] **Release v1.0** — fin août 2026

---

## Checkpoints validés

Liste les questions/réponses des checkpoints de l'agent `cpp-mentor`.

### 2026-05-12 — Semaine 1 / CaptureBuffer (1.5 / 4)

| Q | Sujet | Note | Note libre |
|---|---|---|---|
| Q1 | Pourquoi `std::atomic<int64_t>` pour `writePos` ? | 0.5 / 1 | A vu la raison "range int64" (évite wrap 13h). N'a pas distingué la 2ème raison "atomicity" (thread-safety lecture UI). |
| Q2 | Que se passe-t-il si write/read sur CaptureBuffer en simultané ? | 0 / 1 | A répondu "const = thread safety" — **confusion à corriger**. La vraie protection vient de `std::atomic` sur `writePos`, pas de `const`. Le contenu du ringBuffer est en data race "techniquement UB, inaudible en pratique sur x86". |
| Q3 | Pourquoi `getReadPointer/getWritePointer` plutôt que `getSample/setSample` ? | 0 / 1 | A répondu "sécurité" — **inverse**. C'est de la **performance** (pas de bounds check, SIMD-friendly). La sécurité se gère par les guards en amont (jmin sur les channels). |
| Q4 | Pourquoi `std::floor` plutôt que `static_cast<int>` ? | 1 / 1 | ✅ A bien capté : différence pour `samplePos < 0`. Cas concret donné par le mentor : `-1.5` → cast donne -1 / floor donne -2 → interpolation `frac` négative si cast, propre avec floor. |
| Q5 | Comment supporter 5.1 ? | skip | Skip. Réponse donnée par le mentor : `prepare(sr, int numChannels)`, le reste presque inchangé grâce au design channel-agnostic. |

**Concepts à reprendre avant la semaine 2** :
- **Data race vs thread safety** — `const` ne protège pas, `std::atomic` oui (pour les types triviaux uniquement)
- **Pattern JUCE perf** — `getReadPointer/getWritePointer` = perf, pas sécurité

### 2026-05-12 — Semaine 2 / FxBank + StutterFx — DEFERRED (pas eu le temps)

Les 4 questions à reprendre à froid :

1. **Virtual destructor** — si on enlève `virtual` devant `~IFxModule() = default;`, que se passe-t-il quand `std::unique_ptr<IFxModule> fx = std::make_unique<StutterFx>(); fx.reset();` ? Quel destructeur est appelé, et quels membres ne sont pas libérés ?

2. **`std::array` vs `std::unordered_map`** dans FxBank — l'arch doc suggérait map, on a fait array. Donne 2 raisons distinctes pour lesquelles array est meilleur pour notre cas.

3. **Cached APVTS pointers** — pourquoi on cache `fxTypeParam = apvts.getRawParameterValue("fx_type")` au constructeur, et qu'est-ce qui se passe **concrètement** si on appelle `apvts.getRawParameterValue("fx_type")` à chaque processBlock ? (Indice : lesson 0004)

4. **Design extrapolation** — ajouter un `ReverserFx` : liste les 5 étapes (fichiers + actions) concrètes.

Note : à revisiter avant la semaine 3 idéalement, pour solidifier les bases OOP/JUCE avant d'ajouter plus de FX ou d'attaquer le Sequencer.
- **Pattern JUCE perf** — `getReadPointer/getWritePointer` = perf, pas sécurité ; on les utilise systématiquement dans les hot loops
