# 0005 — `getPlayHead()` and `getPosition()` may both return nullopt

- **Date** : 2026-05-10
- **Tags** : `[juce]`

## Context

`SequencerEngine` reads PPQ and BPM from the host playhead.

## Mistake to avoid

Dereferencing `posInfo->getBpm()` directly.

## Why

Some hosts return `nullptr` for `getPlayHead()`; others return a valid `PlayHead*` whose `getPosition()` returns `nullopt`. Standalone returns nullopt entirely.

## Rule going forward

Always two-step the check:

```cpp
auto* ph = getPlayHead();
if (!ph) return;
auto pos = ph->getPosition();
if (!pos) return;
auto bpm = pos->getBpm().orElse(120.0);
```

## Related

- `docs/architecture-engines.md` §2 (SequencerEngine)
