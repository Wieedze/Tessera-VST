# Build and test rules

Operational rules for building and testing Tessera. Aligned with the Pamplejuce template — see `docs/PAMPLEJUCE_REFERENCE.md` for the full reference.

## Canonical commands

### Configure (run once, or after `CMakeLists.txt` changes)

```bash
# Linux/macOS with Ninja (fastest)
cmake -B Builds -G Ninja -DCMAKE_BUILD_TYPE=Release

# Windows (use PowerShell, point at the VS 2022 or 2026 generator)
cmake -B Builds -G "Visual Studio 17 2022" -A x64
# or for VS 2026 once stable:
cmake -B Builds -G "Visual Studio 18 2026" -A x64
```

### Build

```bash
cmake --build Builds --config Release
```

For incremental builds during development, `Debug` config is fine:
```bash
cmake -B Builds -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build Builds --config Debug
```

### Run all tests

```bash
ctest --test-dir Builds --verbose --output-on-failure
```

Or directly without the ctest wrapper:
```bash
./Builds/Tests
```

### Run a single test

Catch2 v3 syntax — match by tag or name:
```bash
./Builds/Tests "[dsp][capturebuffer]"      # by tag
./Builds/Tests "CaptureBuffer round-trip"  # by name (substring match)
```

### Run benchmarks

```bash
./Builds/Benchmarks
```

## Sanitizers (debug-time bug hunting)

Pamplejuce ships ASan and TSan as opt-in CMake flags via `cmake/Sanitizers.cmake`:

```bash
# Address Sanitizer (memory bugs, use-after-free, leaks)
cmake -B Builds-asan -DCMAKE_BUILD_TYPE=Debug -DWITH_ADDRESS_SANITIZER=ON
cmake --build Builds-asan

# Thread Sanitizer (data races) — RT-safety relevant
cmake -B Builds-tsan -DCMAKE_BUILD_TYPE=Debug -DWITH_THREAD_SANITIZER=ON
cmake --build Builds-tsan
```

Use TSan when adding new DSP↔UI atomic patterns or lock-free FIFOs. Use ASan for the routine debug build.

## Warnings = errors policy

**Every warning must be resolved** before a task is considered complete. Pamplejuce defaults to relatively strict warnings; we treat them as errors in spirit even if CMake doesn't flip the `-Werror` switch by default.

If `juce-reviewer` sees an unaddressed warning in a review, it must flag it as a blocker.

Exception: warnings inside third-party headers (JUCE, Catch2, Melatonin) that we cannot fix. These are pragma-suppressed at our boundary, not silenced.

## LSP / clangd false positives — VS Code workflow

When working in VS Code with the WSL extension, **clangd cannot understand the JUCE module system**. It will routinely show fake errors:
- "undeclared identifier `juce::AudioBuffer`"
- "file not found: `juce/juce_core.h`"
- "use of undeclared identifier `BinaryData::logo_png`"

**Rule** — Trust only `cmake --build Builds` output. IDE squiggles are advisory only when JUCE/BinaryData is involved.

If you want to reduce false positives:
1. Run `cmake -B Builds -DCMAKE_EXPORT_COMPILE_COMMANDS=ON` at least once.
2. Symlink `Builds/compile_commands.json` to the repo root: `ln -sf Builds/compile_commands.json .`
3. Restart clangd in VS Code. It will now pick up the JUCE include paths from the compilation database. Some squiggles will remain (JUCE macro magic), but it will be much cleaner.

## BinaryData.h gotcha

After `cmake -B Builds`, JUCE's `juceaide` tool generates `BinaryData.h` and `BinaryData.cpp` from anything in `assets/`. **Before the first build, this header does not exist** — clangd will scream "file not found".

Workflow:
1. Drop your asset into `assets/`.
2. Run `cmake -B Builds` (re-configure to pick up the new file).
3. Run `cmake --build Builds` once. `BinaryData.h` now exists.
4. Restart clangd in VS Code (Cmd/Ctrl+Shift+P → "clangd: Restart language server").

## Test discovery

`tests/*.cpp` is auto-globbed by `cmake/Tests.cmake`. Just drop a file and re-configure CMake:
```bash
cmake -B Builds   # picks up new test files
cmake --build Builds
```

A new test file must include the right header:
```cpp
#include <catch2/catch_test_macros.hpp>
```

For floating-point matchers:
```cpp
#include <catch2/matchers/catch_matchers_floating_point.hpp>
```

## Gating dev-only code

Pamplejuce defines two useful preprocessor macros for the Tests / Benchmarks targets:

- `RUN_PAMPLEJUCE_TESTS=1` — defined when compiled into the Tests/Benchmarks target. Use to gate dev-only assertions, fixture helpers, or expose `private:` members for testing.
- `CI=1` — defined when the build runs in GitHub Actions. Use to skip slow tests or to tweak tolerances on CI machines.

```cpp
#if RUN_PAMPLEJUCE_TESTS
    // expose internals for test inspection
    friend class CaptureBufferTester;
#endif
```

## Plugin validation

Beyond Catch2 unit tests, two validators run in CI (and should be run before each tagged release):

1. **`pluginval`** (strict mode) — Tracktion's general-purpose validator. Covers VST3, AU, AAX. Fails on hangs, threading bugs, parameter range violations, GUI lifecycle issues.
2. **`auval`** (macOS only) — Apple's AU validator. Required for Logic Pro acceptance. Run via `auval -v aufx Tssr MxLb`.

Both must pass before any release tag.

## CI workflows

`.github/workflows/build_and_test.yml` builds on macOS + Windows + Linux on every push and runs all tests + `pluginval`. Read it before changing CMake-level concerns.

`.github/workflows/nightly.yml` does the same on a nightly schedule for early regression detection.
