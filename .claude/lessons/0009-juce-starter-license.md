# 0009 — JUCE Starter license is fine until $20k cumulative revenue/year

- **Date** : 2026-05-10
- **Tags** : `[business]` `[release]`

## Context

`spec-vst.md` §11 covers JUCE licensing tiers.

## Why

- **Starter** (free) — requires the JUCE splash screen on Standalone, but is otherwise unrestricted.
- **Indie** ($800 perpetual or $40/mo) — removes the splash, required at >$20k/yr cumulative revenue.
- **Pro** ($3500 / $175/mo) — required at >$300k/yr.

## Rule going forward

Stay on Starter through MVP and beta. Track cumulative revenue from day 1 of release; set a calendar reminder for the $15k threshold to budget the Indie purchase before crossing $20k. The splash is acceptable for the MVP.

## Related

- `docs/spec-vst.md` §11
