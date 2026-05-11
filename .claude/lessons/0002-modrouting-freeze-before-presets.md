# 0002 — ModRouting struct must be frozen before any preset ships

- **Date** : 2026-05-10
- **Tags** : `[arch]` `[preset]`

## Context

Spec v0.2 redefines `ModRouting` from 5 fields (v0.1) to 9 fields, including `auxSource`, source/output curves, polarity.

## Mistake to avoid

Shipping presets serialized against an in-flux struct.

## Why

The preset XML / `ValueTree` binds to the field layout; renaming or adding fields breaks user presets unless versioned migrations are in.

## Rule going forward

Implement and lock `ModRouting` (the 9 fields documented in `docs/spec-updates-v0.2.md` §2) BEFORE writing any preset save/load code. Add an explicit `presetSchemaVersion` integer in the project state from day one.

## Related

- `docs/spec-updates-v0.2.md` §2
- `docs/spec-vst.md` §8 (preset system)
