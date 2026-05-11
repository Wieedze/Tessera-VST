# 0006 — Mockups still carry the placeholder name "Glitchwave"

- **Date** : 2026-05-10
- **Tags** : `[ui]` `[workflow]`

## Context

`docs/mockup-*.html` reference "Glitchwave" in `<title>` and headers; project name is now Tessera.

## Note

Cosmetic only, but means: when extracting visual specs from the mockups, ignore the textual brand and replace with Tessera in the actual code/UI.

## Rule going forward

When implementing UI, never copy hardcoded "Glitchwave" strings from the mockups. Source brand from one C++ constant (e.g. `tessera::ProjectInfo::name`).

## Related

- `docs/naming.md`
- `.claude/rules/naming-conventions.md`
