# Pamplejuce — Reference for Tessera

Local snapshot of the Pamplejuce template documentation, distilled from:

- `docs/PAMPLEJUCE_CLAUDE.md` — their CLAUDE.md (instructions for Claude Code)
- `docs/PAMPLEJUCE_TEMPLATE.md` — their README
- The official manual at https://melatonin.dev/manuals/pamplejuce/ (snapshotted 2026-05-11)
- The `cmake/` submodule from https://github.com/sudara/cmake-includes

This file consolidates everything we need to know about working **with** the Pamplejuce template inside Tessera. Read this once, refer back as needed.

---

## 1. Official manual — index

> Live URLs. Open these if you need depth on a specific topic.

- Getting Started
  - [Why build plugins in CI?](https://melatonin.dev/manuals/pamplejuce/getting-started/why-build-plugins-in-ci/)
  - [How does this all work?](https://melatonin.dev/manuals/pamplejuce/getting-started/how-does-this-all-work/)
  - [Setting up your project](https://melatonin.dev/manuals/pamplejuce/getting-started/setting-your-project-up/)
  - [Code signing](https://melatonin.dev/manuals/pamplejuce/getting-started/code-signing/)
- Customization
  - [File management](https://melatonin.dev/manuals/pamplejuce/customization/file-management/)
  - [Including files outside of /source](https://melatonin.dev/manuals/pamplejuce/customization/including-files-outside-of-src/)
- JUCE
  - [Adding JUCE Modules](https://melatonin.dev/manuals/pamplejuce/juce/adding-juce-modules/)
  - [Private JUCE modules](https://melatonin.dev/manuals/pamplejuce/juce/private-juce-modules/)
  - [Dealing with Binary Data](https://melatonin.dev/manuals/pamplejuce/juce/binary-data/)
  - [JuceHeader.h](https://melatonin.dev/manuals/pamplejuce/juce/juceheader-h/)
- GitHub Actions 101
  - [Tips and Gotchas](https://melatonin.dev/manuals/pamplejuce/github-actions-workflows-101/tips-and-gotchas/)
  - [How do GitHub variables work?](https://melatonin.dev/manuals/pamplejuce/github-actions-workflows-101/how-do-github-variables-work/)
  - [Using self-hosted runners](https://melatonin.dev/manuals/pamplejuce/github-actions-workflows-101/using-self-hosted-runners/)
- Life with Pamplejuce
  - [Adding Tests and Benchmarks](https://melatonin.dev/manuals/pamplejuce/life-with-pamplejuce/adding-tests-and-benchmarks/)
  - [Clang Format](https://melatonin.dev/manuals/pamplejuce/life-with-pamplejuce/clang-format/)
  - [Managing Releases](https://melatonin.dev/manuals/pamplejuce/life-with-pamplejuce/managing-releases/)
  - [How to update your Project](https://melatonin.dev/manuals/pamplejuce/life-with-pamplejuce/how-to-update-your-project/)

---

## 2. Build commands (canonical)

These are the build commands Pamplejuce expects. Use them verbatim once the CMakeLists.txt has been adapted.

```bash
# Configure (once, or after CMakeLists.txt changes). Builds/ is the build dir.
cmake -B Builds -DCMAKE_BUILD_TYPE=Release

# Faster with Ninja:
cmake -B Builds -G Ninja -DCMAKE_BUILD_TYPE=Release

# Build everything
cmake --build Builds --config Release

# Run tests (cross-platform)
ctest --test-dir Builds --verbose --output-on-failure

# Run tests directly (faster — skips ctest wrapping)
./Builds/Tests

# Run a single test by name (Catch2 tag/name syntax)
./Builds/Tests "[test name]"

# Run benchmarks
./Builds/Benchmarks
```

**macOS universal binary**: append `-DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"` to the configure step.

**Sanitizers** (debug builds):
```bash
cmake -B Builds -DCMAKE_BUILD_TYPE=Debug -DWITH_ADDRESS_SANITIZER=ON
cmake -B Builds -DCMAKE_BUILD_TYPE=Debug -DWITH_THREAD_SANITIZER=ON
```

---

## 3. Project setup checklist (when adapting CMakeLists.txt)

When we customize CMakeLists.txt for Tessera, change these explicit variables:

| Variable | Tessera value | What it controls |
|---|---|---|
| `PROJECT_NAME` | `Tessera` | Internal name + CMake target name. **No spaces.** |
| `PRODUCT_NAME` | `Tessera` | Display name in DAWs. Can have spaces if needed. |
| `COMPANY_NAME` | `MaxLab` | Used for macOS bundle name, manifests, installer |
| `BUNDLE_ID` | `com.maxlab.tessera` | macOS bundle identifier (reverse-DNS) |
| `FORMATS` | `Standalone VST3 AU` | Plugin formats to build. Add `CLAP` once needed. |
| `PLUGIN_MANUFACTURER_CODE` | `MxLb` | 4-char manufacturer code (case-sensitive) |
| `PLUGIN_CODE` | `Tssr` | 4-char plugin code (unique per plugin per manufacturer) |

The `VERSION` file at the repo root drives the plugin version. Pamplejuce reads it; bumping VERSION re-runs CMake configure on the next build.

---

## 4. File management conventions

- **All source code** goes in `src/`. Auto-globbed. No manual `add_files`.
- **All tests** go in `tests/`. Auto-globbed. Linked against `SharedCode` (see §6).
- **All benchmarks** go in `benchmarks/`. Auto-globbed too.
- **All assets** (fonts, images, factory presets) go in `assets/`. Auto-included as `BinaryData` (see §7).
- Do **not** consolidate everything into `PluginProcessor.cpp` / `PluginEditor.cpp`. Split early — better debugability, better reusability across future products.
- If a CMake-aware IDE (CLion) prompts you to "add this file to the target", **say no**. The auto-glob already covers it.

---

## 5. Adding files outside `src/`

If you must `#include` a header that lives outside `src/` (e.g. third-party single-header lib), add an explicit `target_include_directories()` to the relevant target in `CMakeLists.txt`. Pamplejuce's auto-glob does **not** reach beyond the standard folders.

---

## 6. The `SharedCode` INTERFACE target — the architectural keystone

Pamplejuce's plugin code does **not** live in the `Tessera` target directly. It lives in an `INTERFACE` CMake target called `SharedCode`.

Why this matters:
- The plugin target (`Tessera_VST3`, `Tessera_AU`, `Tessera_Standalone`, etc.) links `SharedCode`.
- The test target (`Tests`) also links `SharedCode`.
- The benchmark target (`Benchmarks`) also links `SharedCode`.

If we put plugin code directly in the plugin target, then `Tests` would need to compile the same `.cpp` files again → **One Definition Rule (ODR) violation** at link time, or duplicated symbols, or silent miscompilation.

`SharedCode` being an `INTERFACE` library means: it doesn't produce its own binary, it just declares "here are headers + compile definitions + include paths that anyone linking me inherits". The actual `.cpp` files are compiled once per consumer target.

Defaults applied to `SharedCode` by `cmake/SharedCodeDefaults.cmake`:
- C++23 (`cxx_std_23`) — **NOTE**: our reference docs say C++20. Decision pending (see §11).
- Fast-math (`/fp:fast` on MSVC, `-Ofast` on GCC/Clang) in Release builds.
- `/Zc:__cplusplus` on MSVC so feature-detection macros report correctly.

---

## 7. Binary Data — the `Assets` target

Drop fonts, images, factory presets into `assets/`. CMake auto-builds a `BinaryData.h` header that exposes each file as a C array.

Usage in C++:
```cpp
#include "BinaryData.h"

auto image = juce::ImageCache::getFromMemory(BinaryData::logo_png,
                                              BinaryData::logo_pngSize);
```

Naming: filename with dots replaced by underscores. `logo.png` → `BinaryData::logo_png` + `BinaryData::logo_pngSize`.

**Heads up**: after `cmake -B Builds`, `BinaryData.h` is generated by JUCE's `juceaide` tool. Your IDE / clangd will **not** see it until at least one build has run. False-positive include errors are normal until then.

For complex asset layouts (folder structure, JSON files), Pamplejuce author recommends **CMakeRC** as a complement to `juce_add_binary_data`.

---

## 8. Tests — Catch2 v3

Auto-globs `tests/*.cpp`. A test file looks like:

```cpp
#include <catch2/catch_test_macros.hpp>
#include "MyModule.h"   // from src/

TEST_CASE("MyModule does X", "[dsp][mymodule]") {
    REQUIRE(/* assertion */);

    SECTION("edge case") {
        CHECK(/* assertion */);
    }
}
```

Patterns:
- `REQUIRE` halts the test case on failure.
- `CHECK` continues; lets you batch multiple assertions in one case.
- `SECTION` groups setup-shared sub-tests.
- Floats: `Catch::Approx(value)` or `Catch::Approx(value).margin(tolerance)`.
- For complex boolean expressions, add **extra parens** inside `CHECK((a == b && c < d))` — Catch2's expression decomposition can't handle `&&`/`||` directly.

Pamplejuce ships a **custom `main`** in `tests/Catch2Main.cpp` that starts `juce::ScopedJuceInitialiser_GUI`. This prevents leaks when test code touches the JUCE message thread. Don't replace it.

Tests get the macro `RUN_PAMPLEJUCE_TESTS=1` defined so test-only code paths can be gated:
```cpp
#if RUN_PAMPLEJUCE_TESTS
    // dev-only assertion or fixture helper
#endif
```

CI sets `CI=1` as a compile definition for the Tests target, so you can branch on it.

LLDB debug-step-avoid (macOS):
```
settings set target.process.thread.step-avoid-regexp Catch|_catch_sr
```
in `~/.lldbinit` lets the debugger skip noisy Catch2 internals.

---

## 9. GitHub Actions

`.github/workflows/build_and_test.yml` builds on macOS + Windows + Linux on every push and runs:
1. Build the plugin in Release
2. Run the `Tests` target
3. Run `pluginval` 1.x in strict mode against the produced binaries

`.github/workflows/nightly.yml` does the same nightly for early regression detection.

For Pamplejuce-specific GitHub Actions gotchas (cache, secrets, self-hosted runners), see the manual section "GitHub Actions 101".

---

## 10. Code style — `.clang-format`

Pamplejuce ships its own `.clang-format`:
- Allman-style braces (open brace on its own line, like `K&R` but everywhere)
- 4-space indentation
- No column limit (long lines OK if they read well)

Run `clang-format -i src/*.cpp src/*.h` to format. CI does not enforce it by default — opt-in via a pre-commit hook if desired.

---

## 11. C++23 vs C++20 — decision point for Tessera

Pamplejuce defaults to **C++23** (as of June 2025, per `cmake-includes/CHANGELOG.md`). Our reference docs (`CLAUDE.md`, `.claude/rules/rt-safety.md`, `docs/spec-vst.md`) all say **C++20**.

Implications of staying on C++23:
- We get `std::expected`, `std::print`, monadic `std::optional`, deducing `this`, ranges improvements.
- Better consistency with the template default — less friction when pulling Pamplejuce updates.
- All three of our compilers (gcc 13.3, clang 18, MSVC 19.40+) support C++23.

Implications of stepping down to C++20:
- Matches our written specs literally.
- Slightly broader compiler compatibility (older toolchains).

**Decision**: defer to user. An ADR (`docs/ADRs/0002-cpp23-or-cpp20.md`) should record the choice once made.

---

## 12. LSP / clangd false positives — important note for VS Code workflow

Clangd in VS Code does **not** understand the JUCE module system. It will routinely report fake errors:
- "undeclared identifier `juce::AudioBuffer`"
- "file not found: `juce/juce_core.h`"
- "use of undeclared identifier `BinaryData::logo_png`"

**Rule**: ignore squiggles in the IDE. Trust **only** the `cmake --build` output. If `cmake --build Builds` succeeds, the code is fine even if VS Code shows red.

A `.clangd` config file at repo root can help (telling clangd to use `compile_commands.json` generated by CMake), but it never eliminates the false positives entirely.

---

## 13. Warnings = errors

Pamplejuce's stated policy: "Always resolve any compile warnings encountered during builds. Warnings should be treated as errors and fixed before considering a task complete."

We adopt this. Our `juce-reviewer` agent should flag any uninvestigated warnings.

---

## 14. JUCE includes the stdlib

JUCE modules already pull in `<vector>`, `<algorithm>`, `<string>`, `<memory>`, `<map>`, etc. via their internal headers. Redundant `#include` of these in JUCE-flavoured files is harmless but unnecessary.

For DSP code that does **not** include JUCE headers, we still include explicitly — clearer dependency tracking.

---

## 15. Realtime safety (their formulation)

Their CLAUDE.md formulation:
- Allocate in constructors or `prepareToPlay`, not while rendering audio
- Avoid dynamic allocations and container growth
- Prefer fixed-size storage (`std::array`, preallocated buffers, fixed-capacity queues)
- Keep operations deterministic and lock-free where possible

Matches our `.claude/rules/rt-safety.md`. Theirs is shorter; ours is more thorough (with the audio-path scope definition, the alternatives table, the patterns by use case). No conflict.

---

## 16. Updates from upstream

Because Pamplejuce is a **template** (not a dependency), pulling upstream improvements is manual. The `cmake/` submodule (pointing at `sudara/cmake-includes`) **does** support updates via:

```bash
git submodule update --remote --merge cmake
```

To pull broader Pamplejuce updates, the maintainer-recommended approach is to diff our root files against upstream periodically and cherry-pick. The Pamplejuce manual has a page "How to update your Project" for this.

---

## 17. Local snapshot of their cmake-includes

The `cmake/` submodule (once initialized) gives us these files:

| File | Purpose |
|---|---|
| `Assets.cmake` | Auto-include `assets/` into BinaryData |
| `Benchmarks.cmake` | Catch2 benchmark target setup |
| `CPM.cmake` | The CPM package manager (for non-submodule deps like Catch2) |
| `GitHubENV.cmake` | CI environment variable detection |
| `JUCEDefaults.cmake` | JUCE config tweaks (folder organization, static MSVC runtime, color diagnostics) |
| `PamplejuceIPP.cmake` | Optional Intel IPP integration |
| `PamplejuceLog.cmake` | Build environment logging |
| `PamplejuceMacOS.cmake` | macOS universal binary + deployment target (10.13+) |
| `PamplejuceVersion.cmake` | Reads `VERSION` file, optional auto patch-bump |
| `Sanitizers.cmake` | ASan / TSan flags (opt-in via `-DWITH_ADDRESS_SANITIZER=ON`) |
| `SharedCodeDefaults.cmake` | C++23, fast-math, MSVC `__cplusplus` correctness |
| `Tests.cmake` | Catch2 v3 test target setup |
| `XcodePrettify.cmake` | Xcode IDE niceties |

Each is well-commented; read directly when adjusting build behavior.

---

*Snapshot 2026-05-11. Refresh when pulling Pamplejuce upstream.*
