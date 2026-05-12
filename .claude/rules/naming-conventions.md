# Naming conventions

## Project identity

- **Name**: Tessera
- **Company**: MaxLab (placeholder — confirm before any public release)
- **Plugin code**: `Tssr` (4 chars, JUCE convention)
- **Manufacturer code**: `MxLb`
- **Bundle ID**: `com.maxlab.tessera`
- **Formats**: VST3, AU, Standalone (CLAP added later)

## C++ naming

| Element | Convention | Example |
|---|---|---|
| Classes | `PascalCase` | `CaptureBuffer`, `GrainEngine` |
| Functions / methods | `camelCase` | `prepareToPlay`, `readInterpolated` |
| Members | `camelCase` (no `m_` prefix) | `writePos`, `sampleRate` |
| Constants / `constexpr` | `kPascalCase` or `UPPER_SNAKE` | `kMaxVoices`, `MAX_BUFFER_SIZE` |
| Enums | `enum class PascalCase` + `PascalCase` values | `enum class FxType { Stutter, Reverser }` |
| Files | match the primary class | `CaptureBuffer.h`, `CaptureBuffer.cpp` |
| Namespaces | `lower_snake` or `tessera` | `tessera::dsp` |
| Template params | single uppercase or `PascalCase` | `T`, `SampleType` |

## Files / folders

```
src/
├── PluginProcessor.{h,cpp}
├── PluginEditor.{h,cpp}
├── dsp/
│   ├── CaptureBuffer.{h,cpp}
│   ├── SequencerEngine.{h,cpp}
│   ├── GrainEngine.{h,cpp}
│   ├── ModulationMatrix.{h,cpp}
│   └── fx/
│       ├── IFxModule.h
│       ├── StutterFx.{h,cpp}
│       ├── ReverserFx.{h,cpp}
│       └── ...
└── ui/
tests/
└── dsp/
    ├── CaptureBuffer_tests.cpp
    └── ...
docs/
```

- Test files: `<Module>_tests.cpp` (suffix, not prefix — better autocomplete).

## APVTS parameter IDs

- `lower_snake` with module prefix: `grain_position`, `seq_swing`, `filter_cutoff`
- `juce::Identifier` cached static in the file that uses it.
- Consistent units: `_ms` for milliseconds, `_hz` for hertz, `_db` for decibels, `_norm` for normalized 0..1.

## Git branches

- `feat/<scope>-<short>` — new features
- `fix/<scope>-<short>` — bug fixes
- `refactor/<scope>-<short>` — refactor without behavior change
- `chore/<short>` — build, deps, config
- `docs/<short>` — docs only

Examples: `feat/grain-voice-pool`, `fix/capture-wraparound`, `chore/cmake-clap`.

## Commit prefix scope

Aligned with engine names: `dsp`, `processor`, `editor`, `seq`, `grain`, `mod`, `fx`, `build`, `ci`, `docs`, `tests`. See `.claude/rules/git-style.md`.
