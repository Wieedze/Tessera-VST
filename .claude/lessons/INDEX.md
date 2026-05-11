# Lessons learned — index

One file per lesson. Each entry: context, mistake/surprise, root cause, rule going forward, tag.

Maintained by the `lessons-keeper` agent. Add a new file when a non-trivial bug, surprising design choice, or workflow lesson emerges. If a lesson becomes a permanent rule, move it into `.claude/rules/*.md` instead.

Numbering is sequential, 4 digits, prefix `NNNN-<short-slug>.md`.

## Active lessons

| # | Title | Tags |
|---|---|---|
| 0001 | [Lorenz/Rössler chaos sources can NaN-divergence silently](0001-lorenz-nan-divergence.md) | `[dsp]` `[rt-safety]` |
| 0002 | [ModRouting struct must be frozen before any preset ships](0002-modrouting-freeze-before-presets.md) | `[arch]` `[preset]` |
| 0003 | [`std::vector::reserve` is not enough for noexcept push_back](0003-vector-reserve-insufficient.md) | `[rt-safety]` |
| 0004 | [APVTS string-based parameter lookup is a hot-path cost](0004-apvts-string-lookup-cost.md) | `[juce]` `[perf]` |
| 0005 | [`getPlayHead()` and `getPosition()` may both return nullopt](0005-playhead-nullopt.md) | `[juce]` |
| 0006 | [Mockups still carry the placeholder name "Glitchwave"](0006-mockups-glitchwave-placeholder.md) | `[ui]` `[workflow]` |
| 0007 | [Denormal numbers cause CPU spikes on prolonged silence](0007-denormals-cpu-spike.md) | `[rt-safety]` `[perf]` |
| 0008 | [Logic Pro AU validation crashes are release blockers](0008-au-validation-release-blocker.md) | `[ci]` `[release]` |
| 0009 | [JUCE Starter license is fine until $20k cumulative revenue/year](0009-juce-starter-license.md) | `[business]` `[release]` |
| 0010 | [DSP→UI viz data must be control-rate, not sample-rate](0010-dsp-ui-viz-control-rate.md) | `[arch]` `[ui]` `[perf]` |

## Archived

(empty — entries are archived here when superseded by a rule in `.claude/rules/` or by an ADR in `docs/ADRs/`)
