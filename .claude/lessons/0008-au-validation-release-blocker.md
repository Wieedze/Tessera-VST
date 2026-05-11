# 0008 — Logic Pro AU validation crashes are release blockers, not warnings

- **Date** : 2026-05-10
- **Tags** : `[ci]` `[release]`

## Context

`spec-vst.md` §13 flags AU validation as bloquant on Mac.

## Why

Logic Pro will refuse to load any AU plugin that fails `auval -v <type> <subtype> <manufacturer>`. The validator catches issues invisible to VST3 (parameter ranges out of spec, AUv3 cookie persistence, etc.).

## Rule going forward

- Add `auval -v aufx Tssr MxLb` to the macOS CI lane and treat any failure as a release block.
- Run it locally before any tagged release.
- Steinberg validator + `pluginval` strict mode cover VST3 / general issues but do NOT replace `auval`.

## Related

- `docs/spec-vst.md` §13
- `docs/spec-vst.md` §10 (Tests + plugin validation)
