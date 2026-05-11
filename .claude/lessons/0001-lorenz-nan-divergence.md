# 0001 — Lorenz/Rössler chaos sources can NaN-divergence silently

- **Date** : 2026-05-10
- **Tags** : `[dsp]` `[rt-safety]`

## Context

Spec v0.2 adds Lorenz and Rössler attractors as modulation sources.

## Surprise

These differential systems diverge to ±∞ → NaN if `dt` is too large or numerical drift accumulates.

## Why

Forward Euler integration is conditionally stable; large `dt` exits the attractor's basin and values explode.

## Rule going forward

Every chaos source must call `std::isfinite()` after each tick and reset to a known-good seed (`x=0.1, y=0.0, z=0.0`) on failure. Validate stability with a unit test that runs 10M samples and asserts `isfinite` throughout.

## Related

- `docs/spec-updates-v0.2.md` §4.8 (Lorenz), §4.9 (Rössler)
