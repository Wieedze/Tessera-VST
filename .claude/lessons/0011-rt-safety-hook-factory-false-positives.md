# 0011 — RT-safety hook needs to exclude JUCE factory functions

- **Date** : 2026-05-12
- **Tags** : `[rt-safety]` `[workflow]` `[tooling]`

## Context

While wiring `CaptureBuffer` into `PluginProcessor.cpp`, the PostToolUse hook (`.claude/hooks/rt-safety-check.sh`) blocked the edit, flagging two `new` usages:

- `return new PluginEditor (*this);` in `createEditor()`
- `return new PluginProcessor();` in `createPluginFilter()`

## Why it was a false positive

The hook treats the entire file `src/PluginProcessor.cpp` as audio-path code. That heuristic works for 99% of its content but breaks for JUCE's two factory functions:

- `createEditor()` is called by JUCE on the **message thread** when the user opens the plugin window. UI alloc is mandatory there.
- `createPluginFilter()` is called by the host **once** at plugin instantiation. Allocating the AudioProcessor is mandatory and unavoidable.

Both are explicitly outside the audio path. The RT-safety rule applies to `processBlock` and what it transitively calls, NOT to the whole file.

## Rule going forward

The hook now filters out lines that match any of:

- `createEditor` / `createPluginFilter` / `JUCE_CALLTYPE` (JUCE factory functions)
- `new\s+Plugin(Editor|Processor)` (extra safety net — these specific factory patterns)
- `prepare(`, `reset(` (already excluded — allocation allowed in prepare/reset)

If we later add other factory functions that allocate (e.g. `createParameter` if we expose programmatic param creation), we'll need to extend the filter.

The hook stays intentionally pessimistic — better to false-positive on something we can whitelist than to miss a real violation.

## Related

- `.claude/hooks/rt-safety-check.sh` (see the `filter` regex inside `scan()`)
- `.claude/rules/rt-safety.md` §"The audio path"
- ADR-0002 (English language) — comment style
