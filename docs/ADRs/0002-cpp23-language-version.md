# ADR-0002: C++23 as the language version

- **Status**: accepted
- **Date**: 2026-05-11
- **Deciders**: Maxime, Claude
- **Supersedes parts of**: implicit C++20 baseline in early reference docs

## Context

The Tessera reference docs written before adopting the Pamplejuce template (`CLAUDE.md`, `.claude/rules/rt-safety.md`, `docs/spec-vst.md`) all specify **C++20** as the target language version.

When we adopted Pamplejuce as the base template, we discovered that Pamplejuce defaults to **C++23** (since cmake-includes CHANGELOG entry "2025-06-17: Bump to C++23"). The `SharedCode` INTERFACE target in `cmake/SharedCodeDefaults.cmake` sets `cxx_std_23`.

We must choose: align with the template (C++23) or force the template down to C++20.

## Decision

Tessera targets **C++23**. We update our reference docs to match.

## Alternatives considered

- **C++20** — match the original spec literally. Rejected because:
  - Pamplejuce default is C++23; forcing C++20 means patching `SharedCodeDefaults.cmake` and re-patching on every upstream pull.
  - C++23 adds features we will use: `std::expected` for fallible non-exception APIs, `std::print` for cleaner logging, monadic operations on `std::optional` (cleaner `getPlayHead()` chains), ranges improvements, deducing `this`.
  - All three compilers we use support C++23 fully: gcc 13.3, clang 18, MSVC 19.40+.
  - We have **no code yet** — zero migration cost.

- **C++26** — too bleeding-edge. Some compilers lag. Not what Pamplejuce ships. Rejected.

## Consequences

**Positive**
- One language version end-to-end; no friction with template updates.
- Access to C++23 standard library improvements.
- New code can use the cleanest available idioms from day 1.

**Negative**
- Slightly narrower compiler compatibility (no GCC < 13, no Clang < 16, no MSVC < 19.34) — none of which matter in our target environment.
- Our existing reference docs (`CLAUDE.md`, `.claude/rules/rt-safety.md`, `docs/spec-vst.md` §14) need updating to remove the "C++20" mentions. `spec-vst.md` is a frozen reference doc and stays in its original French/French-spec form; the ADR is the authoritative override.

## References

- `cmake-includes/SharedCodeDefaults.cmake` — sets `cxx_std_23`
- `cmake-includes/CHANGELOG.md` (2025-06-17) — bump to C++23
- `docs/PAMPLEJUCE_REFERENCE.md` §11
- ADR-0001 (English language) — same override pattern: when the template and our early docs diverge, the ADR is authoritative.
