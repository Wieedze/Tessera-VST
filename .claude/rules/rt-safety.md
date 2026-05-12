# RT-safety rules

These rules apply to **any code reachable from `processBlock`**. The audio thread runs at hard real-time deadlines (~250 Hz at 64 samples / 48 kHz). One missed deadline = audible click or xrun.

## The audio path

Includes:
- `PluginProcessor::processBlock` and everything it calls (transitively)
- All modules under `src/dsp/*`
- Callbacks from `juce::AudioPlayHead`, `juce::AudioBuffer`

Excludes (allocation allowed):
- Constructors / destructors
- `prepareToPlay` / `prepare` / `reset` / `releaseResources`
- UI thread code (`PluginEditor`, paint, timers)

## Forbidden in the audio path

### Memory allocation
| Pattern | Why forbidden | Allowed alternative |
|---|---|---|
| `new`, `malloc`, `calloc` | Dynamic allocation, unbounded latency | Pre-allocate in `prepare()` |
| `std::make_unique`, `std::make_shared` | Calls `new` | Pre-allocate, hold by `unique_ptr` member |
| `std::vector::push_back`, `emplace_back`, `resize` | May reallocate | `std::array<T, N>` or pre-reserved fixed-capacity vector with manual size counter |
| `std::string` (non-const&) | Heap allocation | Avoid in audio path; use `juce::String` only outside |
| `juce::String` mutation, concat | Heap allocation | Pre-format at prepare time, store in member |
| `std::function` capturing by value beyond SBO | Heap allocation | Plain lambda, function pointer, or inheritance |

### Synchronization
| Pattern | Why forbidden | Allowed alternative |
|---|---|---|
| `std::mutex`, `lock_guard`, `unique_lock`, `scoped_lock` | Priority inversion, blocks audio thread | `std::atomic`, `juce::AbstractFifo` |
| `juce::ScopedLock`, `juce::CriticalSection` | Same as above | Same as above |
| `juce::ReadWriteLock` | Same as above | Same as above |
| Spin-locks rolled by hand | Dangerous on heterogeneous cores | Use `std::atomic` correctly |

### Exceptions
| Pattern | Why forbidden | Allowed alternative |
|---|---|---|
| `throw` | Stack unwinding allocates | Return `bool` / `std::optional` / sentinel |
| `try` / `catch` | Forces exception machinery | Validate inputs before calling |
| `dynamic_cast` | May throw `bad_cast`, slow | `static_cast` (after design that guarantees the type) |

### Other latency hazards
- `juce::Logger::writeToLog` — writes to file
- `std::cout`, `printf` — locks `stdout`
- `std::this_thread::sleep_for` — never in audio
- `juce::MessageManager::callAsync` — pushes onto MM, can be slow under load
- `Component::repaint()` from DSP — wrong thread

## Required in the audio path

- `juce::ScopedNoDenormals noDenormals;` as the **first line** of `processBlock`
- All allocations done in `prepare()` / `prepareToPlay()`
- Worst-case capacities (max sample rate, max block size, max voices) reserved up front
- All inter-thread state via `std::atomic` or lock-free FIFO
- `JUCE_LEAK_DETECTOR(<Class>);` on every DSP class

## Patterns by use case

### "I need to send a one-shot event from UI to DSP"

→ `juce::AbstractFifo` of plain-data structs (no pointers, no strings).

### "I need the DSP to publish a value to the UI for visualization"

→ `std::atomic<float>` member, UI reads it via `Timer` callback (60 Hz).

### "I need the DSP to publish a buffer of data (spectrum analyzer)"

→ Pre-allocated double-buffer with atomic write index. UI reads the inactive buffer.

### "I need to know if a parameter changed this block"

→ Cache the previous value, compare on entry to processBlock. Don't subscribe to APVTS listeners from the audio thread.

## How violations are caught

1. **Hook**: `.claude/hooks/rt-safety-check.sh` runs after every Edit/Write to DSP files. Blocks the agent if a forbidden pattern is found.
2. **Agent**: `rt-safety-auditor` performs deeper audits on demand or after structural changes.
3. **Tests**: each DSP module has a `[rt-safety]` tag test category that calls the audio path 1000× and checks for leaks.
4. **Thread Sanitizer**: rebuild with `-DWITH_THREAD_SANITIZER=ON` (see `.claude/rules/build-and-test.md`) to catch data races at runtime.
5. **Manual**: at end of each week, run `pluginval` strict and a profiler.

## Pamplejuce's own RT-safety formulation (cross-check)

For redundancy and continuity with the template, Pamplejuce's `CLAUDE.md` states the same baseline in fewer words:

> For anything in the audio thread / hot DSP path (e.g. `processBlock`):
> - Allocate in constructors or `prepareToPlay`, not while rendering audio
> - Avoid dynamic allocations and container growth (`std::vector::push_back`, map insertion, string building)
> - Prefer fixed-size storage (`std::array`, preallocated buffers, fixed-capacity queues)
> - Keep operations deterministic and lock-free where possible

This matches our rules. Tessera's version (this file) is more thorough — adds the audio-path scope definition, the alternatives table, the patterns-by-use-case section, and the violation-catching workflow. Keep using ours as the authoritative source; their formulation is here as a sanity check / introductory summary.
