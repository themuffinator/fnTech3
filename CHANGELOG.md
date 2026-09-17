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

### Builds and Packaging

- Start independent fnTech3 versioning at 0.1.0, with reviewed changelog entries
  and manually requested GitHub draft prereleases built from an existing tag.
- Package Windows x86 builds from MSVC and MinGW, and a Linux x86 build, with
  SHA-256 checksums, source commit identity, notices and compatibility evidence.
- Distribute the engine and project-owned content only; retail game modules,
  proprietary runtimes and game assets must come from the user's installation.

### Known limitations

- Full unified compatibility is not established. Q3 and QL remain subject to
  the open gates in the [compatibility ledger](docs/fntech3/COMPATIBILITY.md).
- RTCW single-player and multiplayer profiles remain experimental and disabled
  in default release builds. Development probes require `-Drtcw-development=true`.
- macOS and additional CPU architectures are outside this release matrix.
