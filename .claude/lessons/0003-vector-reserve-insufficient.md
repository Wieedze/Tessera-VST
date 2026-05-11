# 0003 — `std::vector::reserve` is not enough for noexcept push_back in RT path

- **Date** : 2026-05-10
- **Tags** : `[rt-safety]`

## Context

Tempted to use `std::vector` with `.reserve(N)` for the grain voice pool.

## Mistake to avoid

Assuming reserved capacity guarantees no allocation on `push_back`.

## Why

Some standard library debug iterators / MSVC settings still touch the allocator on `push_back`, even at-capacity. Vector copy/move can still invalidate iterators in subtle ways.

## Rule going forward

In the audio path, use `std::array<T, N>` with a manual `active` flag or a counter. Never `std::vector`, even reserved.

## Related

- `.claude/rules/rt-safety.md`
- `docs/architecture-engines.md` §4 (GrainEngine voice pool)
