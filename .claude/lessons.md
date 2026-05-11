# Lessons learned

Concise, actionable lessons from the Tessera project. Each entry: context, mistake/surprise, root cause, rule going forward, tag.

Most recent on top. Trimmed at ~100 lines — older entries are merged or moved into `.claude/rules/*.md` when they become permanent.

---

## 2026-05-10 — Lorenz/Rössler chaos sources can NaN-divergence silently

**Context** — Spec v0.2 adds Lorenz and Rössler attractors as modulation sources.
**Surprise** — These differential systems diverge to ±∞ → NaN if `dt` is too large or numerical drift accumulates.
**Why** — Forward Euler integration is conditionally stable; large `dt` exits the attractor's basin and values explode.
**Rule going forward** — Every chaos source must call `std::isfinite()` after each tick and reset to a known-good seed (`x=0.1, y=0.0, z=0.0`) on failure. Validate stability with a unit test that runs 10M samples and asserts `isfinite` throughout.
**Tag** — `[dsp]` `[rt-safety]`

## 2026-05-10 — ModRouting struct must be frozen before any preset format ships

**Context** — Spec v0.2 redefines `ModRouting` from 5 fields (v0.1) to 9 fields, including `auxSource`, curves, polarity.
**Mistake to avoid** — Shipping presets serialized against an in-flux struct.
**Why** — The preset XML/ValueTree binds to the field layout; renaming or adding fields breaks user presets unless versioned migrations are in.
**Rule going forward** — Implement and lock `ModRouting` (9 fields, see `docs/spec-updates-v0.2.md` §2) BEFORE writing any preset save/load code. Add an explicit `presetSchemaVersion` integer in the project state from day one.
**Tag** — `[arch]` `[preset]`

## 2026-05-10 — `std::vector::reserve` is not enough for noexcept push_back in RT path

**Context** — Tempted to use `std::vector` with `.reserve(N)` for the grain voice pool.
**Mistake to avoid** — Assuming reserved capacity guarantees no allocation on push_back.
**Why** — Some standard library debug iterators / MSVC settings still touch the allocator on push_back, even at-capacity. Also: vector copy/move can still invalidate iterators in subtle ways.
**Rule going forward** — In the audio path, use `std::array<T, N>` with a manual `active` flag or a counter. Never `std::vector`, even reserved.
**Tag** — `[rt-safety]`

## 2026-05-10 — APVTS string-based parameter lookup is a hot-path cost

**Context** — Default JUCE example uses `apvts.getRawParameterValue("id")` per block.
**Surprise** — Profiler shows 2-3% CPU in string hashing under release builds for medium routings.
**Why** — Internally an `unordered_map<String, AtomicFloat*>` lookup, with a hash per call.
**Rule going forward** — In every DSP class that reads APVTS, cache `std::atomic<float>*` pointers ONCE in the constructor / `prepareToPlay`. In processBlock, only `->load()`.
**Tag** — `[juce]` `[perf]`

## 2026-05-10 — `getPlayHead()` and `getPosition()` may both return nullopt

**Context** — SequencerEngine reads PPQ, BPM from the host playhead.
**Mistake to avoid** — Dereferencing `posInfo->getBpm()` directly.
**Why** — Some hosts return `nullptr` for `getPlayHead()`; others return a valid `PlayHead*` whose `getPosition()` returns `nullopt`. Standalone returns nullopt entirely.
**Rule going forward** — Always two-step the check: `auto* ph = getPlayHead(); if (!ph) return;` then `auto pos = ph->getPosition(); if (!pos) return;` then `pos->getBpm().orElse(120.0)`.
**Tag** — `[juce]`

## 2026-05-10 — Mockups still carry the placeholder name "Glitchwave"

**Context** — `docs/mockup-*.html` reference "Glitchwave" in `<title>` and headers; project name is now Tessera.
**Note** — Cosmetic, but means: when extracting visual specs from the mockups, ignore the textual brand and replace with Tessera in code/UI.
**Rule going forward** — When implementing UI, never copy hardcoded "Glitchwave" strings from the mockups. Source brand from one C++ constant (e.g. `tessera::ProjectInfo::name`).
**Tag** — `[ui]` `[workflow]`

## 2026-05-10 — Denormal numbers cause CPU spikes on prolonged silence

**Context** — `spec-vst.md` §13 lists denormals as a "annoying but real" risk; design system §9 reuses `juce::ScopedNoDenormals`.
**Why** — Subnormal floats (very close to zero) are processed in slow microcode on x86. Filters with feedback can drift into denormals on silent input → audible CPU spike.
**Rule going forward** — `juce::ScopedNoDenormals` as line 1 of `processBlock` (already enforced). For DSP modules with feedback paths (filter, reverb, delay), additionally clamp tiny outputs to zero or use `_MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON)` at startup. Add a regression test that runs each FX on 30s of silence and asserts CPU usage stays bounded.
**Tag** — `[rt-safety]` `[perf]`

## 2026-05-10 — Logic Pro AU validation crashes are release blockers, not warnings

**Context** — `spec-vst.md` §13 flags AU validation as bloquant on Mac.
**Why** — Logic Pro will refuse to load any AU plugin that fails `auval -v <type> <subtype> <manufacturer>`. The validator catches issues invisible to VST3 (parameter ranges out of spec, AUv3 cookie persistence, etc.).
**Rule going forward** — Add `auval -v aufx Tssr MxLb` to the macOS CI lane and treat any failure as a release block. Run it locally before any tagged release. Steinberg validator + `pluginval` strict mode cover VST3 / general issues but do NOT replace `auval`.
**Tag** — `[ci]` `[release]`

## 2026-05-10 — JUCE Starter license is fine until $20k cumulative revenue/year

**Context** — `spec-vst.md` §11 covers JUCE licensing tiers.
**Why** — Starter (free) requires the JUCE splash screen on Standalone but is otherwise unrestricted. Indie ($800 perpetual or $40/mo) removes the splash and is required at >$20k/yr cumulative. Pro ($3500 / $175/mo) at >$300k/yr.
**Rule going forward** — Stay on Starter through MVP and beta. Track cumulative revenue from day 1 of release; set a calendar reminder for the $15k threshold to budget the Indie purchase before crossing $20k. The splash is acceptable for the MVP.
**Tag** — `[business]` `[release]`

## 2026-05-10 — DSP→UI viz data must be control-rate, not sample-rate

**Context** — Granular overlay needs grain head positions, write head, spawn cursor.
**Mistake to avoid** — Pushing a snapshot of voice state per sample.
**Why** — At 48 kHz × 32 voices × 16 bytes per voice = ~24 MB/s of FIFO traffic — wastes cache and CPU; also the human eye only needs ~30 Hz.
**Rule going forward** — `GrainViewExporter` snapshots once per block (or every N blocks ≈ 30 Hz target) into a lock-free single-producer/single-consumer FIFO of `GrainViewData`. UI Timer at 30 Hz reads the latest.
**Tag** — `[arch]` `[ui]` `[perf]`
