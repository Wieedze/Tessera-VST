# Architecture des Engines — Deep Dive

> Document technique compagnon du `spec-vst.md`. Explique le **comment ça marche** de chaque moteur DSP en termes d'architecture, structures de données, et flux d'exécution.

---

## Vue d'ensemble : qui parle à qui

Avant de creuser chaque engine, voici la chaîne complète d'un sample audio qui rentre :

```
INPUT ──► CaptureBuffer.write()
            │
            └──► SequencerEngine.tick()  ◄── HostTransport (PPQ)
                  │
                  └─ décide step actif + FX choisi
                       │
                       ▼
                   FxBank[currentFx].process()  ◄── ModulationMatrix
                                                       (modulent les params)
                       │
                       ▼
                   GrainEngine.process()  ◄── (lit dans CaptureBuffer
                                                à position modulée)
                       │
                       ▼
                   SpatialFx (Delay + Reverb)
                       │
                       ▼
                   DryWet mix ──► OUTPUT
```

**6 engines à comprendre** :
1. **CaptureBuffer** — la fondation, tout part de là
2. **SequencerEngine** — le chef d'orchestre temporel
3. **FxBank + IFxModule** — la collection d'effets, pattern stratégie
4. **GrainEngine** — le moteur granulaire (le plus complexe)
5. **ModulationMatrix** — le système nerveux qui anime les paramètres
6. **PluginProcessor** — l'orchestrateur principal qui appelle tout le monde

---

## 1. CaptureBuffer — la fondation

### Rôle

Stocker en continu l'audio qui entre dans le plugin pour permettre à n'importe quel module DSP de **relire le passé** : stutter relit le dernier 1/16, reverse joue à l'envers, granular pioche des grains à des positions arbitraires, slicer découpe en morceaux.

Sans ce buffer, **rien d'autre ne peut marcher**. C'est pour ça qu'on le code en premier.

### Analogie React

Imagine un `useRef` qui stocke un historique fixe des N dernières props reçues, dans lequel n'importe quel composant enfant peut piocher. Sauf qu'ici c'est en temps-réel, lock-free, et en C++.

### Structure de données

```cpp
class CaptureBuffer {
    juce::AudioBuffer<float> ringBuffer;   // [numChannels][bufferSize]
    std::atomic<int64_t>     writePos{0};  // sample-accurate global counter
    std::atomic<bool>        frozen{false};
    int                      bufferSize;   // = 4 bars × secs × sampleRate
    double                   sampleRate;
};
```

Points clés :

- **Buffer pré-alloué** au `prepareToPlay()`, **jamais** redimensionné dans `processBlock`
- `writePos` est un compteur global croissant (jamais wrap au type int64), on calcule l'index modulo à la lecture
- `frozen` est un atomic bool : quand `true`, on n'écrit plus mais on peut toujours lire (= effet "freeze" comme Portal)

### Algorithme d'écriture

```cpp
void write(const juce::AudioBuffer<float>& input) {
    if (frozen.load()) return;                          // freeze = no write
    
    const int numSamples = input.getNumSamples();
    const int64_t pos = writePos.load();
    
    for (int ch = 0; ch < numChannels; ++ch) {
        for (int i = 0; i < numSamples; ++i) {
            int idx = (pos + i) % bufferSize;          // modulo wrap
            ringBuffer.setSample(ch, idx, input.getSample(ch, i));
        }
    }
    writePos.store(pos + numSamples);
}
```

### Algorithme de lecture (avec interpolation)

Crucial : tous les modules qui relisent (granular, pitch, reverse) ont besoin de positions **fractionnaires** (ex : sample 12345.7). Donc on fait une **interpolation linéaire** minimum, idéalement Hermite ou cubique pour la qualité audio.

```cpp
float readInterpolated(int channel, double samplePos) const {
    // samplePos peut être un double pour pitch/granular
    int   idx0 = static_cast<int>(samplePos) % bufferSize;
    int   idx1 = (idx0 + 1) % bufferSize;
    float frac = samplePos - std::floor(samplePos);
    
    float s0 = ringBuffer.getSample(channel, idx0);
    float s1 = ringBuffer.getSample(channel, idx1);
    
    return s0 + frac * (s1 - s0);  // linear interp (upgrade to Hermite later)
}
```

### Pièges à éviter

| Piège | Conséquence | Solution |
|---|---|---|
| Wrap d'un `int32` writePos | Comportement chaotique après ~13h à 48kHz | Toujours `int64_t` |
| Lock pour synchroniser write/read | Click et glitches DAW | Atomics seulement, pas de mutex |
| Lecture à `pos > writePos` | Lecture du futur = silence ou ancienne data | Toujours `readPos < writePos - safetyMargin` |
| Allocation au resize buffer | RT violation si l'utilisateur change le sampleRate | Allouer la taille MAX au start |

---

## 2. SequencerEngine — le chef d'orchestre

### Rôle

À chaque bloc audio, dire au reste du système : **"là, maintenant, on est au step 7, le FX assigné est Stutter, sa probabilité s'est validée, donc tu actives Stutter avec ces paramètres"**.

C'est lui qui transforme une grille temporelle abstraite en commandes concrètes pour le FxBank.

### Analogie React

C'est un **state machine** avec des transitions déclenchées par un clock externe (le host DAW). Comme un `useReducer` dont les actions sont émises par un timer, sauf que le timer c'est le tempo musical de la session, pas `setInterval`.

### Structure de données

```cpp
struct Step {
    bool       enabled       = true;
    FxType     fx            = FxType::Thru;   // enum class FxType { Thru, Stutter, ... }
    float      probability   = 1.0f;            // [0..1]
    float      gateLength    = 1.0f;            // [0..1] fraction du pas
    int        paramSnapshot = 0;               // index dans la table des snapshots FX
};

struct Pattern {
    std::array<Step, 16>  steps;
    float                 swing = 0.0f;
    int                   length = 16;          // 1..16, longueur effective
    StepDivision          division = StepDivision::Sixteenth;
};

class SequencerEngine {
    std::array<Pattern, 8>  patterns;
    std::atomic<int>        currentPatternIdx{0};
    int                     lastEvaluatedStep = -1;
    juce::Random            rng;
    // crossfade state pour transitions douces
    FxType                  prevFx = FxType::Thru;
    float                   crossfadeAmount = 0.0f;
};
```

### Algorithme de tick (par bloc audio)

```cpp
void tick(const juce::AudioPlayHead::PositionInfo& posInfo,
          int numSamples, double sampleRate)
{
    if (!posInfo.getIsPlaying()) return;
    
    const double ppq = posInfo.getPpqPosition().orElse(0.0);
    const double bpm = posInfo.getBpm().orElse(120.0);
    
    // ex: si division = 1/16, on a 4 steps par beat
    const double stepsPerBeat = stepsPerBeatFor(currentPattern().division);
    const double currentStepFloat = ppq * stepsPerBeat;
    const int    stepIndex = static_cast<int>(currentStepFloat) % currentPattern().length;
    
    // détection passage de step (edge detection)
    if (stepIndex != lastEvaluatedStep) {
        evaluateStep(stepIndex);
        lastEvaluatedStep = stepIndex;
    }
}
```

### Évaluation d'un step (probabilité + crossfade)

```cpp
void evaluateStep(int idx) {
    const Step& step = currentPattern().steps[idx];
    
    if (!step.enabled) {
        scheduleFxChange(FxType::Thru);
        return;
    }
    
    const float roll = rng.nextFloat();
    const FxType chosenFx = (roll < step.probability) ? step.fx : FxType::Thru;
    
    scheduleFxChange(chosenFx);
}

void scheduleFxChange(FxType newFx) {
    if (newFx != currentFx) {
        prevFx = currentFx;
        currentFx = newFx;
        crossfadeAmount = 0.0f;  // démarre crossfade ramp
    }
}
```

### Crossfade entre FX (anti-click)

Sans crossfade, changer brutalement de Stutter à Reverser en plein milieu d'un sample = **click audible**. Solution : pendant les premiers ~16ms d'un nouveau step, on mixe `prevFx.process()` qui décroît et `newFx.process()` qui monte.

```cpp
// Dans process(), après tick():
if (crossfadeAmount < 1.0f) {
    auto prevOut = fxBank[prevFx].process(input);
    auto newOut  = fxBank[currentFx].process(input);
    output = prevOut * (1.0f - crossfadeAmount) + newOut * crossfadeAmount;
    crossfadeAmount = std::min(1.0f, crossfadeAmount + crossfadeIncrement);
} else {
    output = fxBank[currentFx].process(input);
}
```

### Gestion du swing

Swing = décaler les steps pairs d'un offset fractionnaire :

```cpp
double effectiveStepPos = stepIndex + (isOddStep(stepIndex) ? swing * 0.5 : 0.0);
```

### Pièges à éviter

| Piège | Conséquence | Solution |
|---|---|---|
| Évaluer probabilité chaque sample | Tirages incohérents dans un step | Edge detection : 1 tirage par changement de step |
| Pas de crossfade FX | Clicks audibles | Ramp 16ms minimum |
| `getPlayHead()` retourne nullptr | Crash | Toujours check `optional` / null |
| Sync rate = 1/64 + BPM 200 | Step trop court < 1 bloc audio | Capper la rate au step minimum garantissant ≥ 1 sample |

---

## 3. FxBank + IFxModule — pattern stratégie

### Rôle

Toutes les transformations DSP (Stutter, Reverser, Filter, etc.) partagent une **même interface**, sont pré-instanciées au démarrage, et le SequencerEngine n'a qu'à pointer celle qu'il veut activer.

### Analogie React

Comme un `Map<EffectType, EffectComponent>` où chaque effet est un composant qui implémente la même API (`process` au lieu de `render`). Le Sequencer est le parent qui choisit lequel monter.

### Interface de base

```cpp
class IFxModule {
public:
    virtual ~IFxModule() = default;
    
    virtual void prepare(double sampleRate, int maxBlockSize) = 0;
    virtual void reset() = 0;
    virtual void process(juce::AudioBuffer<float>& buffer,
                         CaptureBuffer& capture,
                         const FxParams& params) = 0;
    
    virtual FxType getType() const = 0;
};
```

Pourquoi `CaptureBuffer&` est passé : la majorité des FX glitch (Stutter, Reverser, TapeStop) **ne traitent pas le buffer entrant** — ils relisent dans le passé du CaptureBuffer. Le buffer entrant courant est ignoré ou utilisé pour mesurer le temps.

### Le FxBank

```cpp
class FxBank {
    std::unordered_map<FxType, std::unique_ptr<IFxModule>> modules;
public:
    FxBank() {
        modules[FxType::Stutter]      = std::make_unique<StutterFx>();
        modules[FxType::Reverser]     = std::make_unique<ReverserFx>();
        modules[FxType::TapeStop]     = std::make_unique<TapeStopFx>();
        modules[FxType::Filter]       = std::make_unique<FilterFx>();
        modules[FxType::Bitcrusher]   = std::make_unique<BitcrusherFx>();
        modules[FxType::Gater]        = std::make_unique<GaterFx>();
        modules[FxType::PitchShifter] = std::make_unique<PitchShifterFx>();
        modules[FxType::Granular]     = std::make_unique<GranularFx>();
        modules[FxType::Slicer]       = std::make_unique<SlicerFx>();
        modules[FxType::Thru]         = std::make_unique<ThruFx>();
    }
    
    IFxModule& get(FxType t) { return *modules[t]; }
    
    void prepareAll(double sr, int blockSize) {
        for (auto& [type, mod] : modules) mod->prepare(sr, blockSize);
    }
};
```

### Exemple concret : StutterFx

```cpp
class StutterFx : public IFxModule {
    int captureLengthSamples = 0;
    int playPos = 0;
public:
    void process(juce::AudioBuffer<float>& buffer,
                 CaptureBuffer& capture,
                 const FxParams& params) override
    {
        // params.stutterRate = 1/16 (ex), donc capture 1/16 de note
        captureLengthSamples = samplesFromMusicalRate(params.stutterRate, sampleRate, params.bpm);
        
        const int64_t writePos = capture.getWritePos();
        const int64_t startPos = writePos - captureLengthSamples;  // début du loop
        
        for (int i = 0; i < buffer.getNumSamples(); ++i) {
            const int relativePos = (playPos % captureLengthSamples);
            const double readPos = startPos + relativePos;
            
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch) {
                buffer.setSample(ch, i, capture.readInterpolated(ch, readPos));
            }
            playPos++;
        }
        
        // décay sur la durée du gate (volume qui baisse à chaque répétition)
        applyDecay(buffer, params.stutterDecay);
    }
};
```

### Pourquoi pré-allouer toutes les instances ?

**RT-safety**. Si on instanciait `new StutterFx()` quand le sequencer le demande, on aurait une allocation dans le path audio = potentielle pause de 100µs+ = click ou xrun.

Toutes les instances vivent dans le `FxBank` du démarrage à la fermeture. Coût mémoire négligeable (quelques KB par FX).

---

## 4. GrainEngine — le plus complexe

### Rôle

Produire du son granulaire à la *Portal* : décomposer un signal en milliers de petits "grains" (5-200ms chacun) avec leur propre pitch, position, enveloppe, sens de lecture, et les mélanger pour créer des textures qui vont du léger smearing à des nuages sonores complets.

### Analogie React

Comme un **virtualized list** style `react-window` : tu ne crées/détruis pas des composants, tu as un **pool fixe** de slots qui se réutilisent. Chaque slot = un grain en cours de lecture. Le scheduler décide quand "monter" un grain dans un slot libre.

### Concept clé : le Grain comme objet temporaire

Un grain est défini par 6 propriétés au moment où il est lancé :

```cpp
struct Grain {
    bool    active = false;
    double  readPos = 0.0;        // position dans CaptureBuffer
    double  pitchRatio = 1.0;     // 1.0 = normal, 2.0 = octave up
    int     lengthSamples = 0;
    int     elapsedSamples = 0;
    bool    reverse = false;
    float   amplitude = 1.0f;
};
```

Chaque grain joue de `elapsedSamples = 0` à `lengthSamples`, avec une **enveloppe Hann** qui fade in puis fade out (= pas de click au début/fin).

### Architecture du moteur

```cpp
class GrainEngine {
    static constexpr int MaxVoices = 32;
    std::array<Grain, MaxVoices>  voices;       // pool pré-alloué
    
    double  samplesUntilNextSpawn = 0.0;
    double  sampleRate = 0.0;
    juce::Random rng;
};
```

### Boucle principale (process)

```cpp
void process(juce::AudioBuffer<float>& output,
             CaptureBuffer& capture,
             const GrainParams& params)
{
    output.clear();
    const int numSamples = output.getNumSamples();
    
    for (int i = 0; i < numSamples; ++i) {
        // 1. Spawn de nouveaux grains selon density
        samplesUntilNextSpawn -= 1.0;
        if (samplesUntilNextSpawn <= 0.0) {
            spawnGrain(capture, params);
            const double samplesPerGrain = sampleRate / params.density;  // density = grains/sec
            samplesUntilNextSpawn = samplesPerGrain;
        }
        
        // 2. Process tous les grains actifs
        float sampleL = 0.0f, sampleR = 0.0f;
        for (auto& g : voices) {
            if (!g.active) continue;
            
            const float env = hannEnvelope(g.elapsedSamples, g.lengthSamples);
            const double pos = g.reverse 
                ? g.readPos + (g.lengthSamples - g.elapsedSamples) * g.pitchRatio
                : g.readPos + g.elapsedSamples * g.pitchRatio;
            
            sampleL += capture.readInterpolated(0, pos) * env * g.amplitude;
            sampleR += capture.readInterpolated(1, pos) * env * g.amplitude;
            
            g.elapsedSamples++;
            if (g.elapsedSamples >= g.lengthSamples) g.active = false;
        }
        
        output.setSample(0, i, sampleL);
        output.setSample(1, i, sampleR);
    }
}
```

### Spawn d'un grain (avec voice stealing)

```cpp
void spawnGrain(CaptureBuffer& capture, const GrainParams& params) {
    // 1. Trouver un slot libre, ou voler le plus vieux
    Grain* slot = findFreeVoice();
    if (!slot) slot = findOldestVoice();   // voice stealing
    
    // 2. Initialiser
    slot->active = true;
    slot->elapsedSamples = 0;
    slot->lengthSamples = static_cast<int>(params.grainSizeMs * sampleRate / 1000.0);
    
    // position dans le capture buffer avec jitter
    const int64_t writePos = capture.getWritePos();
    const double basePos = writePos - (params.position * captureBufferLengthSamples);
    const double jitter = (rng.nextFloat() - 0.5f) * params.positionJitterSamples;
    slot->readPos = basePos + jitter;
    
    // pitch avec jitter
    const float pitchSemis = params.pitch + (rng.nextFloat() - 0.5f) * params.pitchJitter;
    slot->pitchRatio = std::pow(2.0, pitchSemis / 12.0);
    
    // reverse selon probabilité
    slot->reverse = (rng.nextFloat() < params.reverseProb);
    
    // amplitude pour mixage propre (compensation density)
    slot->amplitude = 1.0f / std::sqrt(params.density);
}
```

### L'enveloppe Hann (anti-click)

```cpp
float hannEnvelope(int elapsed, int total) {
    const float t = static_cast<float>(elapsed) / total;  // [0..1]
    return 0.5f * (1.0f - std::cos(juce::MathConstants<float>::twoPi * t));
}
```

Forme : monte de 0 à 1 sur la moitié, redescend de 1 à 0. Pas de click ni au début ni à la fin du grain.

### Optimisations critiques

| Optim | Gain CPU | Notes |
|---|---|---|
| **Voice pooling** | 30%+ | aucune alloc, voice stealing au lieu de skip |
| **SIMD sur le mix grains** | 15-25% | `juce::FloatVectorOperations::add` |
| **Pré-calculer la table Hann** | 5-10% | LUT 4096 entrées avec interp |
| **Limiter density max à 50** | safety | au-delà, perceptuellement saturé + CPU explose |
| **Skip grains à amplitude < ε** | 5% | early-out si quasi-silencieux |

### Pièges spécifiques au granular

| Piège | Conséquence | Solution |
|---|---|---|
| `pos < 0` (lecture avant début) | NaN, garbage | clamp / wrap au buffer size |
| Density 0 → division par zéro | Crash | `density = max(0.01, density)` |
| Grain length > capture buffer | Lecture du futur ou wrap multiple | clamp `grainSize ≤ captureBuffer/4` |
| Pitch ratio extrême (>10) | aliasing énorme | filtre lowpass adaptatif post-grain |

---

## 5. ModulationMatrix — le système nerveux

### Rôle

Permettre à n'importe quelle source (LFO, S&H, Macro, Envelope Follower) de moduler n'importe quel paramètre (cutoff filtre, position grain, density, swing, etc.) avec depth et offset configurables.

### Analogie React

C'est un **publish/subscribe** où les sources publient leurs valeurs courantes et chaque destination subscribe à une combinaison pondérée de sources. Comme un ensemble de selectors Redux composés mathématiquement.

### Structure de données

```cpp
enum class ModSource { LFO1, LFO2, SH, EnvFollower, Macro1, Macro2, Macro3, Macro4 };
enum class ModDest   { FilterCutoff, FilterReso, GrainPosition, GrainDensity,
                       GrainPitch, StutterRate, /* ... */ };

struct ModRouting {
    ModSource source;
    ModDest   dest;
    float     depth   = 0.0f;   // [-1..+1]
    float     offset  = 0.0f;
    bool      enabled = false;
};

class ModulationMatrix {
    std::array<float, NumSources>  currentSourceValues;  // mis à jour chaque bloc
    std::vector<ModRouting>        routings;              // taille fixe pré-allouée
    std::array<float, NumDests>    modulatedValues;       // résultat final
};
```

### Sources : implémentation des modulateurs

#### LFO

```cpp
class LFO {
    float phase = 0.0f;
    float frequency = 1.0f;
    LfoShape shape = LfoShape::Sine;
public:
    float tick(double sampleRate) {
        phase += frequency / sampleRate;
        if (phase >= 1.0f) phase -= 1.0f;
        
        switch (shape) {
            case LfoShape::Sine:    return std::sin(phase * twoPi);
            case LfoShape::Saw:     return 2.0f * phase - 1.0f;
            case LfoShape::Square:  return phase < 0.5f ? 1.0f : -1.0f;
            case LfoShape::Random:  /* S&H interne */ break;
        }
    }
};
```

#### Envelope Follower

```cpp
class EnvelopeFollower {
    float env = 0.0f;
    float attackCoeff = 0.0f, releaseCoeff = 0.0f;
public:
    void prepare(double sr, float attackMs, float releaseMs) {
        attackCoeff  = std::exp(-1.0f / (attackMs  * 0.001f * sr));
        releaseCoeff = std::exp(-1.0f / (releaseMs * 0.001f * sr));
    }
    float tick(float input) {
        const float rectified = std::abs(input);
        const float coeff = (rectified > env) ? attackCoeff : releaseCoeff;
        env = rectified + coeff * (env - rectified);
        return env;
    }
};
```

### Calcul à control rate (1× par bloc)

Pour économiser CPU, on **ne calcule pas la modulation par sample**. On calcule 1 fois par bloc audio (typiquement 64-256 samples). C'est suffisant pour des modulations à 10-20 Hz max, qui sont l'écrasante majorité des cas musicaux.

```cpp
void processModulation(int numSamples) {
    // 1. Tick chaque source 1 fois (ou plus pour LFO si besoin précision)
    currentSourceValues[(int)ModSource::LFO1]    = lfo1.tick(sampleRate * numSamples);
    currentSourceValues[(int)ModSource::EnvFollower] = envFollower.getCurrent();
    // macros lues depuis APVTS (pas de tick)
    currentSourceValues[(int)ModSource::Macro1]  = macroParam1->load();
    
    // 2. Reset destinations à valeurs base APVTS
    for (auto& [dest, baseVal] : baseValues) modulatedValues[dest] = baseVal;
    
    // 3. Appliquer chaque routing
    for (const auto& r : routings) {
        if (!r.enabled) continue;
        const float srcVal = currentSourceValues[(int)r.source];
        modulatedValues[(int)r.dest] += srcVal * r.depth + r.offset;
    }
    
    // 4. Clamp aux ranges valides de chaque param
    for (int d = 0; d < NumDests; ++d) {
        modulatedValues[d] = clampToParamRange(d, modulatedValues[d]);
    }
}
```

### Comment les FX consomment

```cpp
// dans FilterFx::process()
const float cutoff = modMatrix.get(ModDest::FilterCutoff);  // valeur déjà modulée
filter.setCutoff(cutoff);
```

C'est le **boulot de la modulation matrix de produire la valeur finale**. Les FX ne savent pas si elle est modulée ou pas, ils la consomment juste.

---

## 6. PluginProcessor — l'orchestrateur

### Rôle

C'est le `processBlock()` qui appelle tout dans le bon ordre. Le "main loop" du plugin. Sa beauté c'est sa simplicité.

### Le processBlock complet

```cpp
void PluginProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                   juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    
    // 0. Récupérer info host
    auto playHead = getPlayHead();
    auto posInfo = playHead ? playHead->getPosition() : std::nullopt;
    
    // 1. Capturer l'input dans le ring buffer
    captureBuffer.write(buffer);
    
    // 2. Update modulation (1 fois par bloc)
    modMatrix.processModulation(buffer.getNumSamples());
    
    // 3. Tick sequencer (détecte changement de step + crossfade)
    sequencer.tick(*posInfo, buffer.getNumSamples(), getSampleRate());
    
    // 4. Process FX courant (le sequencer décide lequel)
    FxParams fxParams = collectFxParamsFromAPVTS();
    fxBank.get(sequencer.getCurrentFx()).process(buffer, captureBuffer, fxParams);
    
    // 5. Spatial layer (toujours actif)
    delay.process(buffer);
    reverb.process(buffer);
    
    // 6. Dry/wet final
    applyDryWet(buffer, originalInput, dryWetParam->load());
}
```

### Pourquoi cet ordre ?

| Ordre | Raison |
|---|---|
| Capture EN PREMIER | Tous les FX qui suivent doivent pouvoir relire le sample courant |
| Mod AVANT process FX | Pour que les params déjà modulés soient utilisés |
| Sequencer décide AVANT FX | Pour savoir lequel activer ce bloc |
| Spatial APRÈS FX | Reverb/Delay habillent le résultat des FX, pas l'inverse |
| Dry/wet EN DERNIER | Permet de mixer original + tous les FX combinés |

### Communication UI ↔ DSP

| Direction | Mécanisme | Exemple |
|---|---|---|
| UI → DSP (setting param) | APVTS atomic store | l'utilisateur tourne un knob |
| UI → DSP (action ponctuelle) | Lock-free FIFO | "freeze maintenant" |
| DSP → UI (visualisation) | Atomic load + repaint timer | VU meter, step indicator |
| DSP → UI (data buffer) | Ring buffer secondaire | spectrum analyzer |

**Règle d'or** : jamais de message en chaîne. UI poste un atomic, DSP le lit au prochain bloc, point.

---

## 7. Diagramme final d'interaction

```
┌─────────────────────────────────────────────────────────────┐
│                    UI THREAD (60 Hz repaint)                │
│  Knobs ──► APVTS atomic stores ──┐                          │
│  Pads ──► LockFreeFIFO ──────────┤                          │
│                                   │   ▲                     │
│                                   │   │ atomic loads        │
└───────────────────────────────────┼───┼─────────────────────┘
                                    ▼   │
┌─────────────────────────────────────────────────────────────┐
│                AUDIO THREAD (RT, ~250 Hz @ 64 samples)      │
│                                                              │
│  processBlock(buffer)                                        │
│    │                                                         │
│    ├─► captureBuffer.write(input)                            │
│    │                                                         │
│    ├─► modMatrix.process()  ──► reads APVTS + sources       │
│    │                                                         │
│    ├─► sequencer.tick(host)  ──► reads pattern from APVTS   │
│    │                                                         │
│    ├─► fxBank[sequencer.currentFx].process(                  │
│    │       buffer, captureBuffer, modulatedParams)          │
│    │                                                         │
│    ├─► spatial.process(delay+reverb)                         │
│    │                                                         │
│    └─► dryWetMix(original, processed)                        │
│                                                              │
│  Output ──► host                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## TL;DR architecture

1. **CaptureBuffer** = ring buffer lock-free, fondation de tout
2. **SequencerEngine** = FSM driven by host PPQ, dit quel FX activer
3. **FxBank** = pool d'effets pré-instanciés, pattern stratégie
4. **GrainEngine** = pool de 32 voix, scheduler par density, enveloppe Hann
5. **ModulationMatrix** = pub/sub à control rate, sources × destinations × depth
6. **PluginProcessor** = orchestrateur dans l'ordre Capture → Mod → Seq → Fx → Spatial → DryWet

Tout est **pré-alloué au prepareToPlay**, **lock-free entre threads**, et **RT-safe** dans le path audio. C'est non négociable.
