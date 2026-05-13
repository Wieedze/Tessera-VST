# Git commit style

## Branching model

Tessera uses a **dev → main** workflow:

- **`main`** — stable, release-ready. Contains only commits that have been validated (tests pass, milestones complete). **Never push directly to main.**
- **`dev`** — active development. All new commits land here first. This is the **default working branch**.

### Daily flow

```bash
git checkout dev          # make sure you're on dev
# ... code, tests, edits ...
git add <files>
git commit -m "..."       # commit on dev
git push                  # pushes to origin/dev
```

### Promoting to main (milestone / release)

When `dev` reaches a checkpoint that is stable (e.g. end of a project week, after pluginval pass, etc.):

```bash
git checkout main
git merge --no-ff dev     # explicit merge commit — keeps history readable
git push                  # main now mirrors dev's state
git checkout dev          # back to working branch
```

The `--no-ff` flag preserves the **structure** of the dev work (you can later see "here is the chunk of commits that landed at the v0.2 milestone"). Without `--no-ff` the merge is a fast-forward and that grouping is lost.

### Rules

- **Always commit on `dev`** unless explicitly merging to main for a release.
- **No force-push to `main`** ever (see also the §"Don'ts" below).
- **No force-push to `dev`** unless you are the only one working on it AND you have a good reason.
- **Branch protection on main** can be enabled on GitHub Settings → Branches → Add classic branch protection rule, pattern `main`, "Require pull request before merging". Optional but recommended once collaborators land.

### Feature branches — required from sprint W3 onwards

Starting with sprint W3, **every feature ships on its own branch off `dev`**. Reason: each feature must be **testable in isolation** (build, unit tests, audible test in Ableton) before being merged back. This catches regressions early and keeps `dev` always in a working state.

#### Naming convention

`feat/<scope>-<short-slug>` for new features (e.g. `feat/fx-reverser`, `feat/fx-tapestop`, `feat/workflow-install-script`).

Other prefixes follow the same pattern: `fix/<scope>-<short>`, `refactor/<scope>-<short>`, `chore/<short>`, `docs/<short>`.

#### Workflow per feature

```bash
# 1. Start from latest dev
git checkout dev
git pull

# 2. Branch off
git checkout -b feat/<scope>-<short>

# 3. Work + commit granularly on this branch
# (the same granular-commits rules apply: each commit compiles + tests pass)
git add <files>
git commit -m "feat(<scope>): <message>"
# ... more commits ...

# 4. Push the branch for safety + cross-machine sync
git push -u origin feat/<scope>-<short>

# 5. Validate in isolation
#    - Linux: cmake --build Builds && ./Builds/Tests
#    - Windows: git pull (other machine), build, install VST3, audible test in Ableton
#    - Run any feature-specific tests (e.g. pluginval for an FX module)

# 6. Merge back into dev with --no-ff (preserves the branch shape in history)
git checkout dev
git merge --no-ff feat/<scope>-<short>
git push

# 7. Cleanup (optional — keeping the branch on origin is fine for traceability)
git branch -d feat/<scope>-<short>
git push origin --delete feat/<scope>-<short>
```

#### When to NOT use a feature branch

- Hotfix of a typo / one-line fix → commit directly on `dev`
- Updating learning docs (`docs/learning/*.md`) → directly on `dev`
- Routine merge of `dev` → `main` at end of sprint → direct merge, no branch needed

## Format

```
<type>(<scope>): <short summary in English, lowercase, no trailing period>

<optional body — wrap at 72 columns>

<optional trailers>
```

## Types

| Type | Use when |
|---|---|
| `feat` | New user-visible feature or DSP module |
| `fix` | Bug fix |
| `refactor` | Internal restructure with no behavior change |
| `perf` | Performance improvement (back it with a number when possible) |
| `test` | Adding or fixing tests, no production code change |
| `docs` | Documentation only |
| `build` | CMake, Pamplejuce config, dependencies |
| `ci` | GitHub Actions, pre-commit hooks |
| `chore` | Maintenance, formatting, no logic touched |

## Scopes

Aligned with engine names so the log is self-organizing:

`dsp`, `capture`, `seq`, `fx`, `grain`, `mod`, `processor`, `editor`, `params`, `ui`, `tests`, `build`, `ci`, `docs`.

## Granularity

Prefer **many small commits over one large one**. Each commit should:
- Compile by itself
- Pass tests by itself (CI green)
- Be reviewable in under 5 minutes

Counter-examples (split these):
- "feat: add CaptureBuffer + StutterFx + tests" → 3 commits
- "fix: typo + behavior bug" → 2 commits

## Examples (good)

```
feat(capture): add lock-free ring buffer with linear interpolation

Pre-allocates 4 bars at the prepared sample rate. Lock-free read/write
via std::atomic<int64_t> writePos. Interpolation is linear for now;
Hermite upgrade tracked separately.

Refs: docs/architecture-engines.md §1
```

```
test(capture): add 5 unit tests for round-trip and wrap-around
```

```
fix(seq): guard nullopt PlayHead before reading PPQ

getPlayHead()->getPosition() can return nullopt when the host is
not playing. Previously crashed on bypass-then-play.
```

```
perf(grain): cache APVTS pointers in constructor

3% CPU saved on density=32 (-MSVC release). String hashing in
getRawParameterValue showed up in the profiler.
```

## Examples (avoid)

- `update code` — useless type and scope
- `feat: lots of stuff` — too broad
- `WIP` — squash before merging
- `Fix bug` — capital + period + no scope
- `feat(capture): added CaptureBuffer.` — past tense + period

## Body / trailers

- Use the body to explain **why**, not what (the diff shows what).
- For non-trivial fixes, reference the doc section: `Refs: docs/architecture-engines.md §3`.
- Reference issues / PRs as `Closes #12`, `Refs #34`.

## Co-authoring

When asked to commit work produced jointly with Claude Code, append:
```
Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
```

## Don'ts

- No `--amend` once pushed.
- No `--force-push` to `main`.
- No `--no-verify` unless explicitly requested by the user.
- No commits straddling unrelated concerns.
