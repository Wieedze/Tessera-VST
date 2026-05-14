# 0012 — `// RT-OK:` marker convention for hook false positives

- **Date** : 2026-05-12
- **Tags** : `[rt-safety]` `[workflow]` `[tooling]`

## Context

The PostToolUse hook `.claude/hooks/rt-safety-check.sh` blocked another legitimate allocation: `std::make_unique<ThruFx>()` inside `FxBank::FxBank()` (constructor body of FxBank in `src/dsp/fx/FxBank.cpp`).

The hook already filters lines containing `prepare(`, `reset(`, JUCE factory functions, and a few other safe patterns. But it cannot easily detect "we are inside a constructor body" because grep is line-based and constructor bodies span multiple lines.

## Decision

Introduce an inline marker comment `// RT-OK: <short reason>` that the developer adds to a line whose allocation is intentional and safe (e.g., it runs in a constructor, destructor, or any function reachable only from outside the audio path).

The hook now treats any line containing `RT-OK:` as filtered (not flagged).

## Style

```cpp
modules[static_cast<size_t>(FxType::Thru)] = std::make_unique<ThruFx>(); // RT-OK: constructor body
```

The reason after the colon is **mandatory** as documentation — future-you will want to know WHY the line was waived. Examples of valid reasons:

- `// RT-OK: constructor body`
- `// RT-OK: prepareToPlay path`
- `// RT-OK: UI thread / editor`
- `// RT-OK: one-shot at construction`

## Rule going forward

- Use `// RT-OK:` **sparingly**. Default is to refactor to avoid the alloc; the marker is for genuinely safe cases the hook can't recognize.
- Reviewers (and the `rt-safety-auditor` agent when added to look at it) must check that the reason is real, not a way to silence the hook lazily.
- New patterns the hook should recognize automatically (e.g., a new factory naming convention) are better added to the hook's `filter` regex than annotated everywhere.

## Related

- `.claude/hooks/rt-safety-check.sh` (the updated `scan()` filter)
- `.claude/lessons/0011-rt-safety-hook-factory-false-positives.md` (the previous round of false-positive fixes)
- `.claude/rules/rt-safety.md`
