# File organization

Convention for Tessera, aligned with Pamplejuce auto-globbing. See `docs/PAMPLEJUCE_REFERENCE.md` §4 for the source.

## Top-level folders

| Folder | Contents | Auto-included? |
|---|---|---|
| `source/` | Plugin C++ source (`.h`, `.cpp`) | ✅ yes — globbed into `SharedCode` |
| `tests/` | Catch2 unit tests (`.cpp`) | ✅ yes — globbed into the `Tests` target |
| `benchmarks/` | Catch2 benchmarks (`.cpp`) | ✅ yes — globbed into the `Benchmarks` target |
| `assets/` | Fonts, images, factory presets (any binary) | ✅ yes — wrapped into `BinaryData.h` by JUCE's `juceaide` |
| `packaging/` | Installer scripts, icons, EULA, dmg config | manual, used by release CI only |
| `cmake/` | CMake modules (submodule from `sudara/cmake-includes`) | included via `CMAKE_MODULE_PATH` |
| `modules/` | JUCE-style modules (CLAP support, melatonin_inspector) | manual `add_subdirectory` in `CMakeLists.txt` |
| `JUCE/` | JUCE framework itself (git submodule) | yes via `add_subdirectory(JUCE)` |
| `docs/` | Project documentation, ADRs, learning notes, mockups, reference docs | not part of the build |
| `.claude/` | Claude Code infrastructure (agents, rules, hooks, lessons) | not part of the build |

## Inside `source/`

Pamplejuce ships `PluginProcessor.{h,cpp}` and `PluginEditor.{h,cpp}` at the top of `source/`. For Tessera, the structure follows `docs/architecture-engines.md` and `.claude/rules/naming-conventions.md`:

```
source/
├── PluginProcessor.{h,cpp}
├── PluginEditor.{h,cpp}
├── dsp/
│   ├── CaptureBuffer.{h,cpp}
│   ├── SequencerEngine.{h,cpp}
│   ├── GrainEngine.{h,cpp}
│   ├── ModulationMatrix.{h,cpp}
│   ├── fx/
│   │   ├── IFxModule.h
│   │   ├── StutterFx.{h,cpp}
│   │   ├── ReverserFx.{h,cpp}
│   │   └── ...
│   ├── sources/
│   │   ├── LfoSource.{h,cpp}
│   │   ├── LorenzSource.{h,cpp}
│   │   └── ...
│   └── viz/
│       ├── GrainViewExporter.{h,cpp}
│       └── ...
└── ui/
    ├── tabs/
    ├── matrix/
    └── viz/
```

The auto-glob is **recursive** — sub-folders are included automatically. No manual `add_files` in `CMakeLists.txt`.

## Inside `tests/`

```
tests/
├── Catch2Main.cpp        # ⚠️ ships custom JUCE-aware main, DO NOT REPLACE
├── PluginBasics.cpp      # Pamplejuce starter — keep or remove
├── helpers/
│   └── test_helpers.h
└── dsp/
    ├── CaptureBuffer_tests.cpp
    ├── SequencerEngine_tests.cpp
    └── ...
```

Test naming convention: `<Module>_tests.cpp` (suffix, not prefix — better autocomplete in editors).

The `Catch2Main.cpp` provided by Pamplejuce starts `juce::ScopedJuceInitialiser_GUI` so any test code touching the message thread doesn't leak. **Do not replace this main.** Add your test cases in separate files.

## Splitting logic out of `PluginProcessor` and `PluginEditor`

**Rule** — Do not let `PluginProcessor.{h,cpp}` or `PluginEditor.{h,cpp}` grow beyond ~300 lines.

Reason:
- Easier to diff/review small focused files
- Easier to test individual modules in isolation
- Reusable across future products
- Pamplejuce author's explicit recommendation: keep these files as thin "wiring" between modules

Pattern: when a feature lands, it goes into a new file under `source/dsp/`, `source/ui/`, etc., and is included from `PluginProcessor.cpp` / `PluginEditor.cpp`. The processor/editor just orchestrates.

## Including files outside `source/`

If you absolutely need to `#include` a header that lives outside `source/` (rare — usually for a third-party single-header lib in `external/` or similar), you must explicitly extend the include paths in `CMakeLists.txt`:

```cmake
target_include_directories(SharedCode INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/external/some-library
)
```

The auto-glob does **not** reach beyond the standard folders.

## What lives where

| If you need to add… | Drop it in… |
|---|---|
| A new DSP module | `source/dsp/<Module>.{h,cpp}` |
| A new FX module | `source/dsp/fx/<Fx>.{h,cpp}` (implementing `IFxModule`) |
| A new modulation source | `source/dsp/sources/<Source>.{h,cpp}` |
| A new UI component | `source/ui/<Component>.{h,cpp}` |
| A unit test for module X | `tests/dsp/<X>_tests.cpp` |
| A benchmark | `benchmarks/<thing>_bench.cpp` |
| A factory preset, font, image | `assets/<file>` — accessed via `BinaryData::<file_with_underscores>` |
| A new architectural decision | `docs/ADRs/<NNNN>-<slug>.md` |
| A new project pitfall lesson | `.claude/lessons/<NNNN>-<slug>.md` |
| A new C++ concept note | append to `docs/learning/notes.md` |
| A new C++ exercise | append to `docs/learning/exercises.md` |

## SharedCode INTERFACE target

Pamplejuce wraps all `source/` C++ code into a CMake `INTERFACE` library called `SharedCode`. The plugin targets (`Tessera_VST3`, `Tessera_AU`, `Tessera_Standalone`) and the `Tests` target all **link** `SharedCode`. This avoids One Definition Rule violations and lets us test the same code that ships in the plugin.

Practically, **you never touch `SharedCode` directly**. CMake handles it. The rule for us: drop files in `source/` and they will end up in `SharedCode` automatically.
