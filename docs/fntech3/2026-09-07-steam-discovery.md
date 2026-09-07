# 2026-09-07 profile-aware Steam discovery

This is a bounded Source/Unit/Build/Fixture checkpoint, not full compatibility
sign-off. Work was performed on `main` based on
`1a0d068ae587ae657acd183aad56f8222c123b40`; the tested source changes are recorded
in the commit containing this report. The manifests identify that base commit
and explicitly record a dirty worktree.

## Contracts and changes

Local retail `appmanifest_2200.acf` identifies Quake III Arena with installation
directory `Quake 3 Arena`; `appmanifest_282440.acf` identifies Quake Live with
installation directory `Quake Live`. These are installation observations, not
incorporated external implementation. Account fields and manifests are not
copied into the repository.

Each profile now owns its discovery AppID and conventional installation name.
Windows registry discovery and shared Steam-library parsing accept an explicit
profile before process-wide activation. Windows and Unix maintain separate Q3
and QL path caches. QL's optional provider explicitly requests the QL path;
finding Q3 does not initialize Steam services or change its `usesSteam` contract.

Automatic startup combines evidence from both discovered Steam installations.
With no higher-priority base/application tree, finding both is ambiguous.
An explicit profile selects its discovery path. The filesystem uses a matching
application installation before a discovered Steam default. Q3 discovery only
supplies its default `fs_basepath`: it does not add another Steam installation
to an explicitly chosen Q3 search tree. QL retains its additional Steam search
path and profile-home behavior.

Shared metadata parsing supports modern object and legacy numeric library
entries, including reordered or empty app indexes. It bounds files to 1 MiB,
rejects embedded NULs, truncated/unterminated tokens, malformed objects and
duplicate identity fields, matches manifest AppIDs, and restricts `installdir`
to one directory component. Installation markers must be profile-specific base
pak files; a loose `default.cfg` alone is no longer installation evidence.
Marker existence still does **not** validate ZIP contents, retail authenticity,
complete asset/module inventories, or all path/symlink behavior.

## Automated and build evidence

- Strict standalone Clang C++17 discovery tests pass; the same helper finds both
  local retail installations. The synthetic suite exercises both profiles,
  relocated installations, modern/legacy libraries, malformed metadata, bounds,
  missing/mismatched markers, and automatic ambiguity/base-path precedence.
- The shared Windows x86 MSVC release client, dedicated server and existing
  renderer/test targets build successfully. The new discovery executable is
  registered with Meson.
- The complete Meson suite in a clean PowerShell environment reports **110
  passed, 0 failed, 1 skipped**. The Python aggregate reports **1242 tests,
  7 skipped**. OpenAL loopback returns its existing skip code 77.
- A subsequently added probe-evidence regression passes separately: it requires
  the exact mounted base-pak path and rejects lookalike names and diagnostics.
- An earlier suite attempt inside `vcvars32` reproduced the already documented
  default-x64 Clang versus x86 library mismatch. The clean-environment rerun
  above resolved it; no test expectations were weakened to hide that failure.

Local build/test logs: `.tmp/steam-profile-final-build.log`,
`.tmp/steam-profile-final-tests.log`, and the release build's Meson test log.

| Windows x86 artifact | SHA-256 at validation |
| --- | --- |
| `.tmp/meson-msvc-x86-release/fntech3.exe` | `41213b6606d66d6869747fbc2f6dee69d0be66a4aea8b7826db1b9672ec3ac1f` |
| `.tmp/meson-msvc-x86-release/fntech3.ded.exe` | `fa1a9c31a81f8ee37d3f4ad9380d9cafe7a263d98ff72568aaf2849f17bb1748` |

## Dedicated discovery probes

`scripts/probe_game_profiles.py --steam-discovery` omits `fs_basepath` from the
engine command line and uses explicit profile selection. Its `--q3`/`--ql`
arguments identify the expected installations for verification and hashing;
they are not forwarded as engine installation overrides. The registered `path`
command records the actual mounted base pak after logging starts.

The same dedicated executable independently discovers both local installations,
loads Q3 `qagame.qvm` / QL API-10 native qagame, loads `q3dm6` / `campgrounds`,
responds with protocol 68 / 91 and the expected map/challenge, and exits 0.
Both probes use isolated temporary homes, loopback-only binding, `pure=1`,
Steam integration disabled, Workshop skipped, and `r_fullscreen=0`. No game
window, input control, OS screenshot, proprietary copy, or installation write
is needed. These are startup and connectionless probes, not live pure admission
or retail client/server interoperability.

Exact arguments, executable/base-pak hashes and outcomes are retained locally
in `.tmp/steam-profile-discovery-runtime-final/manifest.json`. The initial
`.tmp/steam-profile-discovery-runtime/` probes also loaded maps/modules and
exited 0, but the verifier falsely failed because it expected a filesystem
startup line that preceded logfile initialization. The corrected verifier uses
the engine's mounted-pak report and does not relabel those initial results.

The four ordinary basepath probes also pass with this same executable: Q3
automatic/explicit selection in 15.422/15.343 seconds, and QL automatic/explicit
selection in 7.953/7.922 seconds. Both Steam-discovery probes pass in
15.266/7.890 seconds respectively. Every run verifies the mounted base pak,
module startup, protocol/map response and exit 0. The additional manifest is
`.tmp/steam-profile-explicit-runtime/manifest.json`.

## Remaining and separately discovered issues

- Runtime automatic selection with only one Steam installation, additional
  installations, native Unix discovery, archive identity, and wider path
  behavior still need evidence. Unit ambiguity evidence is not a runtime gate.
- Windows fatal startup handling waits for console dismissal. Negative startup
  runtime probes must account for this without controlling user input.
- Publication exposed a pre-existing Linux compile failure in both Make and
  Meson: `Sys_PrintBinVersion` still refers to the removed `FNQL_PROJECT_NAME`
  macro. [Linux run 34159986422](https://github.com/themuffinator/fnTech3/actions/runs/34159986422)
  is authoritative failure evidence for the published checkpoint.
- The RTX CI configuration could not download FreeType after repeated timeouts
  and HTTP 502. Its subsequent missing-Meson-directory error was secondary;
  the Vulkan raster job built successfully. See
  [Vulkan-family run 34159986500](https://github.com/themuffinator/fnTech3/actions/runs/34159986500).
  This dependency failure does not establish a renderer compile defect.

The [compatibility ledger](COMPATIBILITY.md) retains the broader open gates.
