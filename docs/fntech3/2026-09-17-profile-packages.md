# Profile-scoped engine package

## Change

The engine package is now `fntech3-pkg.fnz`, with the same directory layout in
the source `pkg/` tree and the ZIP: `games/<canonical-profile>/<gamedir>/<qpath>`.
`Com_GameProfile()->key` selects the namespace before lookup. The existing
mod/base ordering, adjacent-executable precedence, fixed allowlist, CRC checks,
and ordinary-filesystem fallback are retained. Unscoped and other-profile
entries are never candidates. See the [package contract](PACKAGES.md).

QL keeps its 300 original assets in `games/quakelive/`. The complete reviewed
92-file FnQuake3 package is incorporated under `games/quake3/`, including
Team Arena's `missionpack/`. Its package subtree is clean and identical to
the pinned merger commit `7d8c9629cdc7979f09697266470359101ffd9eda`, despite
the reference repository's newer HEAD. Its main built archive matches that
source tree exactly. The [import inventory](2026-09-17-fnq3-package-assets.json)
records source and destination paths, normalized hashes, and source archive
identity. GPL compatibility was checked before incorporation; attribution is
in `CREDITS.md` and `THIRD_PARTY_NOTICES.md`.

The shipped archive contains 392 files: 184 audio zones, 203 fog presets,
three sound shaders, one renderer shader, and one retained HUD preset.
Only the two Q3 sound shader filenames are adapted to the existing runtime
consumer path. Authored contents/provenance comments are retained and text
uses LF for reproducible archives. The legacy HUD preset has no fnTech3
consumer and remains outside the runtime allowlist. RTCW modes currently
ship no package assets; their namespaces are exercised with synthetic fixtures.

## Evidence

- The production candidate builder and reader execute against controlled ZIP
  I/O, linked to the real immutable profile catalog, for `quake3`, `quakelive`,
  `rtcw-sp`, and `rtcw-mp`. Tests cover identical names across profiles,
  mod changes, archive precedence, absent entries, partial initialization,
  traversal, path bounds, CRC failures, truncated reads, and ownership cleanup.
- Packaging tests verify namespace validation, complete inventories,
  deterministic ZIP bytes, and preservation of the previous archive when
  validation fails. A separate import test verifies every FnQuake3 asset hash.
- The rebuilt Windows x86 audio-zone tool parses all 35 imported `.azb` files.
  The native renderer-independent fog parser accepts all 203 shipped `.fog`
  files, including all 54 imported Q3/Team Arena presets.
- A clean Windows x86 `debugoptimized` client/dedicated build uses the existing
  VS Code Meson helper, GLx, strict warnings, and the RTCW development option.
  Build and automated-suite logs are retained locally under
  `.tmp/fnz-layout/`.
  The full run reports 111 Meson passes and two skips; its aggregate Python
  gate runs 1,499 tests with 11 skips and one unrelated cleanup error:
  `rtcw_tactical_tests` cannot delete a temporary `probe.cpp` because Windows
  reports a sharing violation (`WinError 32`). All five tests in that module
  pass on the isolated x86 rerun. Package-specific and final deterministic
  metadata checks pass. The aggregate run itself is therefore recorded as
  failed, not presented as a completely green suite.
- The same dedicated executable and archive run against legitimate Q3 and QL
  installations with isolated writable homes, loopback networking, Steam
  services disabled, and `r_fullscreen 0`. Startup commands request owned fog
  and sound files through the real `exec` / `FS_ReadFile` path. Q3 reads
  `games/quake3/baseq3/maps/q3dm12.fog` and its own sound shader, while rejecting
  QL's `campgrounds.fog`; QL does the converse. A Q3 `fs_game missionpack` run
  reads `games/quake3/missionpack/maps/mpteam1.fog` and the Team Arena sound
  shader. All three runs exit normally. Commands and observed read lines are
  recorded in [the runtime transcript](2026-09-17-profile-packages-runtime.json).

The runtime probe requests fog/sound text through `exec` solely to exercise
the whole-file resolver and its diagnostics. It does not test audible output
or rendered appearance; the engine parsers are tested separately above.
The first probe used overwrite-mode logging and lost its earlier diagnostics
on shutdown. Repeating with append/flush logging produced the retained
transcripts. No mouse/keyboard control or screen capture was used.

These are package lookup and parser observations. They do not close rendering,
audio presentation, pure/network interoperability, or RTCW compatibility gates.
An existing build limitation was also found: legacy CMake does not generate
the engine archive automatically. `BUILD.md` now records the manual builder
command; Meson and Make generate it automatically.

Temporary runtime homes and test scratch directories were resolved beneath
`.tmp/fnz-layout/` and checked for links before cleanup. Automatic approval
review rejected both cleanup attempts with `blocked by policy`; the temporary
folders therefore remain there, alongside the retained build and evidence.
