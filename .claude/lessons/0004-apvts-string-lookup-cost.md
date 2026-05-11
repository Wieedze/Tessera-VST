# 0004 — APVTS string-based parameter lookup is a hot-path cost

- **Date** : 2026-05-10
- **Tags** : `[juce]` `[perf]`

## Context

Default JUCE example uses `apvts.getRawParameterValue("id")` per block.

## Surprise

Profiler shows 2-3% CPU in string hashing under release builds for medium routings.

## Why

Internally an `unordered_map<String, AtomicFloat*>` lookup, with a hash per call.

## Rule going forward

In every DSP class that reads APVTS, cache `std::atomic<float>*` pointers ONCE in the constructor / `prepareToPlay`. In `processBlock`, only `->load()`.

## Related

- `docs/architecture-engines.md` §6 (PluginProcessor)
- `.claude/agents/juce-reviewer.md` (cached pointer pattern)
