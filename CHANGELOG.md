# fnTech3 Changelog

Reviewed, user-visible changes to fnTech3. Add changes under **Unreleased** as
they land; `python scripts/release_workflow.py prepare --version X.Y.Z` freezes
them into a dated entry before tagging. Released entries are retained.

fnTech3 starts at **0.1.0**. FnQuake3 and FnQL versions and release notes are
historical upstream records, not fnTech3 release history. See the
[release guide](docs/fntech3/RELEASING.md) for versioning and publication rules.

## [Unreleased]

### Highlights

- Introduce fnTech3's shared engine distribution and startup game profiles for
  Quake III Arena and Quake Live, based on the documented FnQuake3 and FnQL
  baselines. Supply a legitimate installation of the selected game.

### Compatibility

- Carry Elite Force single player's scissor rectangle through all three renderers,
  including fractional pixel conversion, reset, and clipping bounds.

- Name keys as both retail key tables name them. The keypad star is `KP_STAR`
  again, so a retail config line that binds it loads and is written back
  instead of being refused and lost, and keys 130 and 157-159 are written as
  `0x82` and `0x9d`-`0x9f` rather than `POWER` and `F13`-`F15`, so a config
  this engine writes loads in retail Quake III and Quake Live. Those four
  names are still read, so configs written by earlier builds keep working. The
  keypad star itself still reports `*`, as retail Quake III, Return to Castle
  Wolfenstein and Enemy Territory all report it on Windows and X11, so a
  `KP_STAR` binding does not fire and an existing `bind *` keeps working.

### Builds and Packaging

- Start independent fnTech3 versioning at 0.1.0, with reviewed changelog entries
  and manually requested GitHub draft prereleases built from an existing tag.
- Package Windows x86 builds from MSVC and MinGW, and a Linux x86 build, with
  SHA-256 checksums, source commit identity, notices and compatibility evidence.
- Distribute the engine and project-owned content only; retail game modules,
  proprietary runtimes and game assets must come from the user's installation.

### Documentation and Tooling

- Record the key names in `docs/fntech3/2026-09-20-key-names.md`, with the
  retail evidence for each corrected row, and share Enemy Territory's
  key-number table with Return to Castle Wolfenstein's instead of keeping a
  second copy of the same 110 numbers.
- Record why the keypad star stays a character key in
  `docs/fntech3/2026-09-20-keypad-star.md`, with the retail producers it
  matches, a reproducible reading of the Windows keyboard layout from
  `scripts/probe_win32_keypad_star.py`, and the cost of the alternative.
  Two commented-out fragments that read as unfinished work now carry that
  reason, and regression tests hold all three producers to it.

### Known limitations

- Full unified compatibility is not established. Q3 and QL remain subject to
  the open gates in the [compatibility ledger](docs/fntech3/COMPATIBILITY.md).
- RTCW single-player and multiplayer profiles remain experimental and disabled
  in default release builds. Development probes require `-Drtcw-development=true`.
- macOS and additional CPU architectures are outside this release matrix.
