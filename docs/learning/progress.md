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
- [ ] `const` correctness
- [ ] RAII et destructeurs automatiques
- [ ] `std::array` vs `std::vector` — choix de container
- [ ] Stack vs heap — où vit quoi
- [ ] `auto` et déduction de type
- [ ] `nullptr` vs `NULL` vs `0`
- [ ] Headers vs implementation (`.h` / `.cpp`), include guards

### Niveau 2 — Types et templates

- [ ] Templates de fonction
- [ ] Templates de classe
- [ ] `concepts` (C++20)
- [ ] `std::span<T>` comme view
- [ ] `std::optional<T>` pour valeurs absentes
- [ ] Move semantics (`std::move`, rvalue refs `T&&`)
- [ ] `std::unique_ptr<T>` vs `std::shared_ptr<T>` — pourquoi shared est interdit RT

### Niveau 3 — Threading et atomics

- [ ] `std::atomic<T>` — load/store/fetch_add
- [ ] Memory ordering (`relaxed`, `acquire`, `release`, `seq_cst`)
- [ ] Pourquoi `std::mutex` est interdit dans `processBlock`
- [ ] Lock-free SPSC FIFO (concept)
- [ ] Data race vs race condition

### Niveau 4 — Polymorphisme

- [ ] `virtual` et vtable
- [ ] Pure virtual + interfaces (`IFxModule`)
- [ ] `static_cast` vs `dynamic_cast`
- [ ] Pourquoi `dynamic_cast` est interdit dans `processBlock`

### Niveau 5 — DSP

- [ ] Ring buffer + interpolation linéaire
- [ ] Phase accumulation (LFO)
- [ ] Hann envelope
- [ ] Voice pooling + voice stealing
- [ ] Forward Euler integration (Lorenz, Rössler)
- [ ] Denormals et flush-to-zero
- [ ] Convolution / IR filtering
- [ ] FFT / overlap-add

---

## Exercices faits

Tracking de [`exercises.md`](exercises.md).

- [ ] 1.1 — Value, reference, pointer
- [ ] 1.2 — RAII et lifetimes
- [ ] 1.3 — `const` partout
- [ ] 2.1 — `std::array` vs `std::vector`
- [ ] 2.2 — `std::span` comme view
- [ ] 3.1 — `std::atomic<int>` counter
- [ ] 3.2 — Ring buffer minimaliste

---

## Modules Tessera

Suit la phasing dans `docs/spec-vst.md` §12 (étendu par `spec-updates-v0.2.md` §8).

### Semaine 1 — Bootstrap + CaptureBuffer

- [ ] Pamplejuce cloné, CMake adapté à Tessera
- [ ] Build vide passe sur ma plateforme
- [ ] VST3 vide se charge dans Reaper
- [ ] `CaptureBuffer` implémenté (ring buffer + interpolation linéaire)
- [ ] Tests `CaptureBuffer_tests.cpp` passent (round-trip, wrap, freeze, interp, no-alloc)
- [ ] `processBlock` capture l'input, le repasse à l'output (bypass parfait)
- [ ] Checkpoint quiz `cpp-mentor` semaine 1 validé

### Semaine 2 — FxBank + Stutter

- [ ] À détailler quand on y arrive

### (semaines suivantes — à compléter au fur et à mesure)

---

## Checkpoints validés

Liste les questions/réponses des checkpoints de l'agent `cpp-mentor`.

(vide — premier checkpoint à venir après le CaptureBuffer)
