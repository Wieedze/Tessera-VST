# ADR-0001: English as the project language for code, comments, and engineering docs

- **Status**: accepted
- **Date**: 2026-05-10
- **Deciders**: Maxime, Claude

## Context

`docs/spec-vst.md` §15 (Instructions agent) initially asked Claude Code to "Toujours documenter en français dans les commentaires d'API publique". The reference docs in `docs/` (spec-vst, architecture-engines, spec-updates-v0.2, design-system, bootstrap-prompt, naming) are written in French.

During the `.claude/` infrastructure setup, the project owner instructed Claude that **"tout le projet doit être en anglais"** — meaning all engineering artifacts going forward (code, headers, comments, commit messages, tests, README, ADRs, agent prompts, lessons file) must be in English.

## Decision

All newly created engineering artifacts in this repository are in English. This includes:

- C++ source code, identifiers, comments, Doxygen headers
- Git commit messages and PR descriptions
- README.md
- `.claude/` directory contents (CLAUDE.md, agents, rules, hooks, lessons.md)
- ADRs in `docs/ADRs/`
- Tests and test descriptions
- CI/CD configuration

The existing French-language reference documents in `docs/` (`spec-vst.md`, `architecture-engines.md`, `spec-updates-v0.2.md`, `design-system.md`, `bootstrap-prompt.md`, `naming.md`) are **left untouched** in their original language. They are treated as input specifications from the user and not as living engineering artifacts.

## Alternatives considered

- **Follow `spec-vst.md` §15 verbatim (French API comments)** — rejected. The owner explicitly overrode this. Mixing French comments with English code creates context-switching cost for any external collaborator and complicates auto-doc tooling that defaults to English.
- **Translate the reference docs to English** — rejected. They are stable specifications, translation introduces drift between original intent and the translated version, and adds work without value. They remain the source of truth in their original form.
- **Bilingual artifacts (FR + EN)** — rejected. Doubles maintenance burden and provides no benefit.

## Consequences

**Positive**
- Single language across code, tooling, CI logs, and engineering docs.
- Lower friction for future external contributors (open source release plausible).
- No translation drift to manage.

**Negative**
- Reference docs in French, engineering artifacts in English — readers may switch languages when navigating between `docs/` and code. Mitigated by the convention being explicit and by `CLAUDE.md` flagging the French docs as reference-only.
- The instruction in `spec-vst.md` §15 is now overridden in practice but remains in the file. This ADR is the authoritative override.

## References

- `docs/spec-vst.md` §15 (overridden)
- `CLAUDE.md` "Language" section
- Project chat 2026-05-10 — owner instruction "tout le projet doit etre en anglais"
