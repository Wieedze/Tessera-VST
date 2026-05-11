# 0010 — DSP→UI viz data must be control-rate, not sample-rate

- **Date** : 2026-05-10
- **Tags** : `[arch]` `[ui]` `[perf]`

## Context

The granular overlay needs grain head positions, write head, and spawn cursor.

## Mistake to avoid

Pushing a snapshot of voice state per sample.

## Why

At 48 kHz × 32 voices × 16 bytes per voice ≈ 24 MB/s of FIFO traffic — wastes cache and CPU; the human eye only needs ~30 Hz anyway.

## Rule going forward

`GrainViewExporter` snapshots once per block (or every N blocks ≈ 30 Hz target) into a lock-free single-producer/single-consumer FIFO of `GrainViewData`. UI Timer at 30 Hz reads the latest.

## Related

- `docs/spec-updates-v0.2.md` §6.1 (granular overlay)
- `.claude/agents/ui-reviewer.md`
