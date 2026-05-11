# 0007 — Denormal numbers cause CPU spikes on prolonged silence

- **Date** : 2026-05-10
- **Tags** : `[rt-safety]` `[perf]`

## Context

`spec-vst.md` §13 lists denormals as an "annoying but real" risk; `design-system.md` §9 reuses `juce::ScopedNoDenormals`.

## Why

Subnormal floats (very close to zero) are processed in slow microcode on x86. Filters with feedback can drift into denormals on silent input → audible CPU spike.

## Rule going forward

- `juce::ScopedNoDenormals` as line 1 of `processBlock` (already enforced).
- For DSP modules with feedback paths (filter, reverb, delay), additionally clamp tiny outputs to zero or call `_MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON)` at startup.
- Add a regression test that runs each FX on 30s of silence and asserts CPU usage stays bounded.

## Related

- `.claude/rules/rt-safety.md`
- `docs/spec-vst.md` §13
