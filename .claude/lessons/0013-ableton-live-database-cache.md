# 0013 — Ableton Live Database aggressively caches plugin param layouts

- **Date** : 2026-05-13
- **Tags** : `[workflow]` `[daw]` `[ableton]`

## Context

While iterating on ReverserFx (S2 of W3), we added a new FxType value and a new APVTS parameter (`reverser_window_ms`). The plugin binary on disk was correct (3 params confirmed by running the Standalone), the install path was up-to-date (timestamp matched the build), but Ableton kept showing the **old** UI with only 2 parameters even after multiple "Rescan Plug-Ins" invocations.

## Why it happens

Ableton 12 keeps a global plugin database at `%LOCALAPPDATA%\Ableton\Live Database\`. This database stores the discovered parameter list per plugin, keyed by plugin UID (which comes from `PLUGIN_MANUFACTURER_CODE` + `PLUGIN_CODE` in JUCE — both unchanged when we add params). "Rescan Plug-Ins" re-discovers the file but does NOT refresh the cached parameter list for an already-known plugin.

Additional gotchas we tripped over in the same session:
- 3 stale copies of `Tessera.vst3` existed simultaneously (`C:\Program Files\Common Files\VST3\`, `C:\Program Files (x86)\VSTPlugIns\`, and `%APPDATA%\VST3\`). Ableton was loading the oldest one (system path, scanned first).
- Wiping the Live Database also **reset Ableton's preferences for plugin folders** — the "Use VST3 Plug-In Custom Folder" setting got disabled, requiring a re-configuration.

## Rule going forward

For Tessera development iteration on Windows:

1. **Single source of truth for the install** — only `C:\Program Files\Common Files\VST3\Tessera.vst3` (the system path, scanned by Ableton out of the box). The `install-vst3.ps1` script + admin elevation handles this cleanly.
2. **When adding APVTS params**, expect Ableton to silently keep the old layout. Force a true re-scan by:
   - Closing Ableton completely
   - Deleting `%LOCALAPPDATA%\Ableton\Live Database\` and `%LOCALAPPDATA%\Ableton\Cache\`
   - Reopening Ableton (which now scans from scratch)
   - Re-enabling "Use VST3 Plug-In Custom Folder" in Preferences if you use one
3. **Quick sanity-check** before debugging the cache: run the JUCE Standalone (`Builds-Win\Tessera_artefacts\Release\Standalone\Tessera.exe`). It bypasses every DAW and shows the live param list. If Standalone has the new param, the binary is correct and the problem is in the host's cache.

## Related

- `.claude/rules/build-and-test.md` §"Windows build + install workflow"
- `scripts/install-vst3.ps1`
- Lesson 0011 (RT-safety hook factory false positives) — different topic but same vibe of "tool caches state we want to invalidate".
