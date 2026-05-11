# Tessera — VST3 multi-FX glitch/granular plugin

JUCE 8 / C++20 audio plugin, based on the Pamplejuce template. Full specs in `docs/`.

## Language

**All project artifacts (code, comments, docs, commit messages, tests) are in English.**
The reference docs in `docs/` (spec-vst, architecture-engines, spec-updates-v0.2, design-system, bootstrap-prompt, naming, mockup-*.html) are kept as-is in their original language — do not translate them. See [docs/ADRs/0001-english-as-project-language.md](docs/ADRs/0001-english-as-project-language.md).

## Non-negotiables (RT-safety)

Inside `processBlock` or any code reachable from the audio path:

- No allocation: no `new`, `malloc`, `make_unique`, `make_shared`, `push_back`, `resize`, `std::string`
- No locks: no `std::mutex`, `std::lock_guard`, `juce::ScopedLock`. **Atomics only** for UI ↔ DSP
- No exceptions: no `throw`, `try`, `catch` in the audio path
- `juce::ScopedNoDenormals` first line of `processBlock`
- All allocation happens in `prepareToPlay`

Details: [.claude/rules/rt-safety.md](.claude/rules/rt-safety.md)

## Method

- **TDD light** — write the test before the implementation
- **C++20** — concepts, ranges, `std::span`
- **Public DSP API documented with Doxygen comments in English**
- **Granular Git commits** — see [.claude/rules/git-style.md](.claude/rules/git-style.md)

## Architecture (6 engines + v0.2 extensions)

`CaptureBuffer → SequencerEngine → FxBank → GrainEngine → ModulationMatrix → PluginProcessor`

v0.2 adds: 17 modulation sources (4 LFO + 2 ENV + S&H + 2 Chaos + EnvFollower + 8 Macros), 10 LFO shapes incl. Lorenz/Rössler chaos and DrawableCurve, ModRouting v2 (9 fields), Matrix tab, EngineActivityPanel + per-FX overlays.

Reference docs (do not modify, kept in original language):
- Product spec + MVP scope + 14-week phasing: [docs/spec-vst.md](docs/spec-vst.md)
- Engines deep-dive: [docs/architecture-engines.md](docs/architecture-engines.md)
- Spec delta v0.2 (Matrix tab, chaos sources, drawable curves, granular viz): [docs/spec-updates-v0.2.md](docs/spec-updates-v0.2.md)
- Design system (palette, typo, components): [docs/design-system.md](docs/design-system.md)
- UI mockups: `docs/mockup-production-v2.html`, `docs/mockup-matrix.html`, `docs/mockup-live.html`
- Week 1 bootstrap: [docs/bootstrap-prompt.md](docs/bootstrap-prompt.md)
- Naming candidates: [docs/naming.md](docs/naming.md)

Architectural decisions are logged in [docs/ADRs/](docs/ADRs/) (one file per decision).

## When to ask before acting

- Ambiguity in `architecture-engines.md` → propose 2-3 options with trade-offs
- Design choice not covered by docs → ask, do not invent
- Adding an external dependency → ask

## Available agents

- `rt-safety-auditor` — RT-safety audit
- `dsp-test-writer` — Catch2 tests per module
- `juce-reviewer` — JUCE / C++20 review
- `ui-reviewer` — JUCE UI / LookAndFeel / OpenGL / DSP↔UI viz review
- `doc-keeper` — keeps docs, ADRs, and public DSP headers in sync
- `lessons-keeper` — updates [.claude/lessons.md](.claude/lessons.md) post-incident
- `cpp-mentor` — explains C++ concepts as they appear, runs module checkpoints, maintains [docs/learning-notes.md](docs/learning-notes.md)

## Learning mode

Maxime is learning C++ through this project. Conversation defaults to **French**, code stays **English**. After each module is written, the `cpp-mentor` produces a short concept summary and a 3-5 question checkpoint quiz. Inline explanations are added to the chat and to `docs/learning-notes.md` for later cold review. Use React/TypeScript analogies when they fit naturally.

## Skills

- `frontend-design` (Anthropic, installed) — invoke when designing UI components, mockups, or the LookAndFeel. Already-installed mockups in `docs/mockup-*.html` define the existing aesthetic; the skill is for new components or refinements.

## RT-safety hook

A PostToolUse hook scans every file edited under `source/dsp/`, `source/PluginProcessor.cpp` and blocks if a forbidden pattern is detected. See [.claude/hooks/rt-safety-check.sh](.claude/hooks/rt-safety-check.sh).
