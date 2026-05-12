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

- [ ] À détailler quand on y arrive

### (semaines suivantes — à compléter au fur et à mesure)

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
- **Pattern JUCE perf** — `getReadPointer/getWritePointer` = perf, pas sécurité ; on les utilise systématiquement dans les hot loops
