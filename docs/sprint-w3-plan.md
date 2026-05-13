# Sprint W3 — Plan d'implémentation

> **Statut** : actif (démarrage 2026-05-13)
> **Goal** : 5 FX (Thru déjà fait + Stutter déjà fait + Reverser + TapeStop + Filter + Bitcrusher) tous "MVP-ready" selon la checklist ci-dessous.
> **Effort estimé** : 7 sessions, ~12-14 h cumulées.
> **Réfs** : `docs/spec-vst.md` §12, §16 ; `docs/architecture-engines.md` §3 ; `docs/spec-updates-v0.2.md` §8.

---

## Strategic position

Fin W2 : `IFxModule` + `ThruFx` + `FxBank` + `StutterFx` + `PluginProcessor` câblé avec APVTS. Tessera charge et joue dans Ableton.

Fin W3 (objectif) : 5 FX, workflow d'iteration fluide, premier merge `dev` → `main`.

---

## Branching model pour W3 — feature branches

À partir de W3, **chaque feature vit sur sa propre branche off `dev`** pour pouvoir la tester en isolation avant de la merger. C'est le pattern décrit dans `.claude/rules/git-style.md` §"Feature branches".

```
main           ──────────────────────────────────────────────────►
                                                                 ▲ (merge final W3)
dev            ─┬─────┬─────────┬─────────┬─────────┬───────────┘
                │     │         │         │         │
feat/workflow   ─┘     │         │         │         │
                       │         │         │         │
feat/fx-reverser       ─┘         │         │         │
                                 │         │         │
feat/fx-tapestop                 ─┘         │         │
                                           │         │
feat/fx-filter                             ─┘         │
                                                     │
feat/fx-bitcrusher                                   ─┘
... etc
```

### Workflow par feature

```bash
# 1. Démarrer une nouvelle feature
git checkout dev
git pull
git checkout -b feat/fx-reverser

# 2. Code, tests, commits granulaires sur cette branche
# ... edits ...
git add <files>
git commit -m "feat(fx): add ReverserFx skeleton"
# ... edits ...
git commit -m "test(fx): ReverserFx unit tests"
# ... edits ...
git commit -m "feat(fx): wire ReverserFx in FxBank + APVTS"

# 3. Pousser pour avoir le code archivé sur origin
git push -u origin feat/fx-reverser

# 4. Test isolation : build + tests Linux + test audible Ableton
cmake --build Builds --config Release && ./Builds/Tests
# (côté Windows : git pull, build, install, test Ableton)

# 5. Quand validé : merger sur dev (--no-ff pour garder la structure)
git checkout dev
git merge --no-ff feat/fx-reverser
git push

# 6. Cleanup local + distant
git branch -d feat/fx-reverser
git push origin --delete feat/fx-reverser  # optionnel — laisser sur origin pour archive est OK aussi
```

### Naming des branches W3

| Phase | Branche |
|---|---|
| S1 | `feat/workflow-install-script` |
| S2 | `feat/fx-reverser` |
| S3 | `feat/fx-tapestop` |
| S4 | `feat/fx-filter` |
| S5 | `feat/fx-bitcrusher` |
| S6 | `feat/quality-smoothing-crossfade` |
| S7 | `feat/quality-validation` |

À la fin de S7, on fait **un seul gros merge** `dev` → `main` qui ramène tout W3 d'un coup. Avant le merge final on peut aussi push une PR pour avoir une vue d'ensemble.

Après W3 (preview) :
- W4 — Gater, PitchShifter, Spatial layer (Delay, Reverb)
- W5 — SequencerEngine
- W6 — GrainEngine (+ GrainViewExporter couplé, cf. v0.2)
- W7 — Slicer + ModulationMatrix v2

---

## Per-FX MVP checklist

Chaque FX livré (Reverser, TapeStop, Filter, Bitcrusher) doit cocher ces 16 cases. C'est le contrat "professionnel et stable" pour cette phase :

### DSP code

- [ ] Fichiers `src/dsp/fx/<Fx>.{h,cpp}`
- [ ] Hérite `IFxModule`, override les 4 virtuals (`prepare`, `reset`, `process`, `getType`)
- [ ] Hook RT-safety passe (pas d'alloc/lock/throw/exception dans `process()`)
- [ ] Parameter smoothing via `juce::SmoothedValue<float>` sur tous les params continus
- [ ] Denormals guard si feedback path (clamp outputs < epsilon → 0)
- [ ] `JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR` sur la classe
- [ ] Doxygen header en anglais, `@note RT-safe` ou `@note NOT RT-safe` par fonction

### Tests (Catch2)

- [ ] `<Fx>_tests.cpp` dans `tests/dsp/fx/`
- [ ] `getType()` retourne le bon `FxType`
- [ ] `reset()` remet l'état interne (vérifiable via 2 process consécutifs identiques)
- [ ] `process()` : output non-silent quand input non-silent
- [ ] `process()` : silence-in → silence-out (sauf self-oscillation documentée)
- [ ] Full-scale (±1.0) input → output dans `[-1.5f, +1.5f]` (un peu de headroom OK)
- [ ] FxBank integration : `bank.get(FxType::Xxx).getType() == FxType::Xxx`

### Intégration plugin

- [ ] `FxType::Xxx` ajouté à l'enum **AVANT** `Count`
- [ ] `FxBank::FxBank()` instancie via `std::make_unique<Xxx>()` avec marker `// RT-OK: constructor body`
- [ ] APVTS params déclarés avec ranges valides + cached `std::atomic<float>*` dans `PluginProcessor`
- [ ] `AudioParameterChoice` si param catégoriel (ex: filter type LP/HP/BP/Notch)
- [ ] Testé audible dans Ableton — pas de crash, pas de clic non-musical

Tant qu'un FX n'a pas coché ces 16 cases : **on ne passe pas au suivant**.

---

## Plan détaillé — 7 sessions

### S1 — Phase 1 : Workflow polish (~45 min)

**Problème** : `COPY_PLUGIN_AFTER_BUILD TRUE` dans CMakeLists.txt copie le VST3 vers `C:\Program Files\Common Files\VST3\` après chaque build. 2 friction points :
- Demande admin Windows
- Échoue si Ableton tient le `.vst3` chargé (DLL lock)

**Actions** :

1. Désactiver `COPY_PLUGIN_AFTER_BUILD TRUE` dans le `juce_add_plugin` du `CMakeLists.txt` (commenter avec note)
2. Créer `scripts/install-vst3.ps1` (PowerShell) :
   - Détecte si Tessera est loaded dans Ableton via `Get-Process Ableton` + lock check
   - Si lock : message clair "Ferme Ableton ou disable le device"
   - Sinon : copie le bundle vers le custom folder Ableton (path en variable)
   - Confirme avec un `ls` du dest
3. Documenter le workflow dans `.claude/rules/build-and-test.md` :
   - Build Windows : `cmake --build Builds-Win --config Release`
   - Install : `.\scripts\install-vst3.ps1`
   - Reload Ableton (rescan)

**Sortie** : 1 commande PowerShell pour install, indépendante des droits admin.

### S2 — Phase 2.1 : ReverserFx (~1h30)

**Concept DSP** : Lit le `CaptureBuffer` en sens inverse. À chaque sample du bloc de sortie, on lit à `windowStart + (windowLength - i - 1)` au lieu de `windowStart + i`.

**Algo (référence `docs/architecture-engines.md` §3 — pas explicitement codé mais similaire à StutterFx)** :

```cpp
void process(buffer, capture, params) override {
    if (sampleRate <= 0.0) return;

    const int windowLength = std::max(1,
        static_cast<int>(params.reverserWindowMs * 0.001 * sampleRate));
    const int64_t writePos = capture.getWritePos();
    const int64_t windowStart = writePos - windowLength;

    const int numSamples = buffer.getNumSamples();
    const int numChannels = std::min(buffer.getNumChannels(), 2);

    for (int i = 0; i < numSamples; ++i) {
        // playPos avance, mais on lit À L'ENVERS dans la fenêtre
        const int relPos = playPos % windowLength;
        const double readPos = static_cast<double>(windowStart + (windowLength - 1 - relPos));

        for (int ch = 0; ch < numChannels; ++ch)
            buffer.setSample(ch, i, capture.readInterpolated(ch, readPos));

        ++playPos;
    }
}
```

**Params APVTS** :
- `reverser_window_ms` (`AudioParameterFloat`, range 50-2000 ms, default 250)
- `reverser_crossfade_ms` (`AudioParameterFloat`, range 0-50 ms, default 10) — pour anti-click au wrap

**Concept C++ nouveau enseigné** : `std::reverse_iterator` (optionnel — on n'en a pas vraiment besoin ici), mais surtout la **réflexion symétrique** d'un index dans une fenêtre.

### S3 — Phase 2.2 : TapeStopFx (~2h)

**Concept DSP** : Variable speed playback. La speed commence à 1.0, descend à 0.0 sur une durée paramétrable (200 ms par défaut). Lit dans `CaptureBuffer` avec un `phaseAccumulator` qui avance plus lentement à mesure que la speed descend.

**Algo** :

```cpp
void process(buffer, capture, params) override {
    const double durationSamples = params.tapeStopLengthMs * 0.001 * sampleRate;
    const int64_t startPos = (state == State::Triggered)
        ? capture.getWritePos()  // ancre au moment du trigger
        : storedStartPos;

    for (int i = 0; i < numSamples; ++i) {
        const double progress = std::min(1.0, samplesPlayed / durationSamples);
        // Courbe : linear, ou exp pour un freinage plus naturel
        const double speed = applyCurve(params.tapeStopCurve, 1.0 - progress);

        const double readPos = startPos + phaseAccumulator;
        for (int ch = 0; ch < numChannels; ++ch)
            buffer.setSample(ch, i, capture.readInterpolated(ch, readPos));

        phaseAccumulator += speed; // avance plus lentement à mesure que speed baisse
        samplesPlayed++;
    }
}
```

**Params APVTS** :
- `tapestop_length_ms` (200-3000 ms, default 600)
- `tapestop_curve` (`AudioParameterChoice` : Linear, ExpFast, ExpSlow)
- (Trigger géré plus tard par le sequencer ; pour W3 on suppose toujours en cours quand FxType=TapeStop)

**Concept C++ nouveau enseigné** : `std::pow` pour la courbe exponentielle ; design d'un FX avec **état progressif** vs StutterFx qui est stateless.

### S4 — Phase 2.3 : FilterFx (~2h)

**Concept DSP** : Multimode filter (Low Pass / High Pass / Band Pass / Notch) basé sur la classe JUCE built-in `juce::dsp::StateVariableTPTFilter<float>`. C'est une variable-state filter à 12 dB/oct, très utilisé en musique.

**Algo** :

```cpp
class FilterFx : public IFxModule {
    juce::dsp::StateVariableTPTFilter<float> filter;
    // ...

    void prepare(double sr, int blockSize) override {
        sampleRate = sr;
        juce::dsp::ProcessSpec spec { sr, (uint32_t) blockSize, (uint32_t) 2 };
        filter.prepare(spec);
        reset();
    }

    void reset() override { filter.reset(); }

    void process(buffer, capture, params) override {
        filter.setType(toJuceType(params.filterType));
        filter.setCutoffFrequency(params.filterCutoffHz);
        filter.setResonance(params.filterResonance);

        // L'API JUCE travaille sur un AudioBlock + ProcessContextReplacing
        juce::dsp::AudioBlock<float> block(buffer);
        juce::dsp::ProcessContextReplacing<float> ctx(block);
        filter.process(ctx);
    }
};
```

**Note** : FilterFx **ne lit pas** dans `CaptureBuffer` — il traite directement le signal d'entrée du bloc. C'est différent des FX de "relecture du passé" (Stutter, Reverser, TapeStop). C'est OK, `IFxModule::process()` reçoit le buffer en in-place, on peut soit lire le passé, soit traiter direct.

**Params APVTS** :
- `filter_type` (`AudioParameterChoice` : LP12, HP12, BP, Notch — default LP12)
- `filter_cutoff_hz` (`AudioParameterFloat`, 20-20000 Hz, **logarithmic range**, default 1000)
- `filter_resonance` (0.0-1.5, default 0.7)
- (drive, à voir si on l'ajoute en W3 ou plus tard)

**Concepts C++ / JUCE nouveaux enseignés** :
- `juce::dsp::` API : ProcessSpec, AudioBlock, ProcessContextReplacing
- Filter à **feedback interne** → introduction concrète aux **denormals** (un filtre LP en silence prolongé peut dériver vers subnormal floats → CPU spike)
- Logarithmic range sur le cutoff (oreilles humaines log-scale en Hz)

### S5 — Phase 2.4 : BitcrusherFx (~1h30)

**Concept DSP** : 2 dégradations indépendantes appliquées en série :
1. **Bit depth reduction** : quantize à N bits. À 8 bits, on a 256 valeurs au lieu de 65536 → distorsion harmonique
2. **Sample rate reduction** : sample-and-hold sur D samples consécutifs → repliement spectral (aliasing)

**Algo** :

```cpp
void process(buffer, capture, params) override {
    const int     bitDepth   = std::clamp(params.bitcrushBits, 1, 16);
    const int     srDiv      = std::clamp(params.bitcrushSrDiv, 1, 32);
    const float   levels     = static_cast<float>(1 << bitDepth); // 2^N

    for (int ch = 0; ch < numChannels; ++ch) {
        float* data = buffer.getWritePointer(ch);
        for (int i = 0; i < numSamples; ++i) {
            // Sample-and-hold (downsample)
            if (heldSampleCounter[ch] <= 0) {
                heldSample[ch] = data[i];
                heldSampleCounter[ch] = srDiv;
            }
            heldSampleCounter[ch]--;

            // Bit depth quantization
            const float v = std::round(heldSample[ch] * levels) / levels;
            data[i] = v;
        }
    }
}
```

**Params APVTS** :
- `bitcrush_bits` (`AudioParameterInt`, 1-16, default 8)
- `bitcrush_sr_div` (`AudioParameterInt`, 1-32, default 4)

**Concepts C++ nouveaux enseignés** :
- `std::clamp` (déjà vu, on reverra)
- Bit-shift pour le calcul `2^N` (`1 << bits`)
- **Aliasing intentionnel** comme outil musical (vs aliasing comme bug)

### S6 — Phase 3.1 : Cross-cutting quality pass (~2h)

Une fois les 4 FX livrés individuellement, **on fait un pass transverse** pour appliquer les techniques de qualité partout d'un coup :

**1. Parameter smoothing partout**

`juce::SmoothedValue<float>` pour chaque param continu (rate, cutoff, resonance, length_ms, etc.). Pas pour les params catégoriels (FxType, filter_type, bit_depth — on ne les smooth pas).

Pattern à appliquer dans chaque `Fx::prepare()` :

```cpp
void prepare(double sr, int blockSize) override {
    sampleRate = sr;
    cutoffSmoothed.reset(sr, 0.02); // 20 ms ramp
    cutoffSmoothed.setCurrentAndTargetValue(initialCutoff);
    // ...
}

void process(buffer, capture, params) override {
    cutoffSmoothed.setTargetValue(params.filterCutoffHz);
    for (int i = 0; i < numSamples; ++i) {
        const float c = cutoffSmoothed.getNextValue();
        // ... use c sample-par-sample ...
    }
}
```

**2. Crossfade FxType switch** (cf. `docs/architecture-engines.md` §2)

Dans `PluginProcessor::processBlock`, quand `currentFx` change :
- Conserver `previousFx` pendant 16 ms
- Mixer `previousFx.process() * (1-α) + currentFx.process() * α` où α monte de 0 à 1 sur 16 ms
- Une fois α=1, on lâche `previousFx`

C'est ~30 lignes de code dans PluginProcessor + un membre `float crossfadeAmount`.

**3. Denormals safety**

- `_MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON)` au début de `prepareToPlay()` (lesson 0007)
- Dans `FilterFx::process()`, après filter.process(), ajouter un clamp : si `|output| < 1e-30f` → `output = 0`. Évite la dérive subnormale en silence prolongé

**4. Test de stress RT-safety**

Étendre la suite Catch2 avec un test qui :
- Crée un PluginProcessor
- Appelle processBlock 10 000 fois avec random audio + random params
- Vérifie : pas de NaN/Inf dans l'output, pas de leak (JUCE_LEAK_DETECTOR)

### S7 — Phase 3.2 : Validation + merge dev → main (~1h)

**1. pluginval strict mode** sur Linux

```bash
# Télécharger pluginval (Tracktion, gratuit)
wget https://github.com/Tracktion/pluginval/releases/latest/download/pluginval_linux.zip
unzip pluginval_linux.zip
chmod +x pluginval

# Run strict mode sur notre VST3
./pluginval --strictness-level 10 --validate-in-process \
    ~/.vst3/Tessera.vst3
```

À surveiller :
- Parameter range issues
- Threading issues (state-info save/load on non-message thread)
- UI lifecycle (open/close 10× sans leak)

**2. Build Windows + test Ableton de chaque FX**

Pour chacun des 4 nouveaux FX :
- Bascule `fx_type` dessus dans Ableton
- Joue un kick / loop / pad
- Vérifie : pas de crash, output audible, ajustement des params change quelque chose audible

**3. Checkpoint cpp-mentor W3** (3-4 questions)

Sujets probables :
- Strategy pattern : avantages vs alternative (switch sur enum)
- Parameter smoothing : pourquoi nécessaire, qu'est-ce qui se passe sans
- juce::dsp:: API : ProcessSpec, ProcessContextReplacing, à quoi ça sert
- Denormals : qu'est-ce que c'est, pourquoi un filter à feedback en silence dérive

**4. Commit final + merge dev → main**

```bash
git checkout main
git merge --no-ff dev
git push
git checkout dev
```

C'est le **premier milestone** mergé sur main. Historiquement marqué.

---

## Risques W3 et mitigations

| Risque | Probabilité | Mitigation |
|---|---|---|
| FilterFx dérive en denormals | élevée | Test silence-in 30 s + clamp + FZ mode |
| Bitcrusher trop bruyant en full-scale | moyenne | Gain de sortie automatique inversement proportionnel à `bitDepth` (option) |
| TapeStop speed descend à 0 → division par 0 | moyenne | `phaseAccumulator += max(speed, 1e-6)` |
| Sliders APVTS auto-générés moches | accepté | C'est la GenericAudioProcessorEditor, on remplace en W11 (LookAndFeel custom) |
| Test stress 10 000 itérations trop lent CI | faible | Garder iterations=1000 en CI, 10000 en local |

---

## Definition of Done W3

À cocher quand le sprint est terminé :

- [ ] 4 nouveaux FX livrés (Reverser, TapeStop, Filter, Bitcrusher) avec checklist 16 cases chacun
- [ ] Workflow build Windows fluide (1 commande, sans admin, sans lock)
- [ ] Parameter smoothing systématique sur tous les params continus
- [ ] Crossfade 16 ms entre FxType
- [ ] Denormals safety (FZ mode + clamps)
- [ ] pluginval strict mode pass sur Linux
- [ ] Tests Catch2 ≥ 40 cases au total (20 actuels + ~5 par nouveau FX)
- [ ] Checkpoint cpp-mentor W3 fait (avec score)
- [ ] Premier merge `dev` → `main` effectué

---

*Plan rédigé 2026-05-13. Mis à jour à mesure que des décisions changent.*
