# AGENTS.md

## Mission

fnTech3 is a unified source port for id Tech 3 games: one engine distribution
whose shared client and dedicated-server binaries select the installed game at
startup. The current game scope is Quake III Arena (`q3`), Quake Live (`ql`), and
Return to Castle Wolfenstein single player (`rtcw-sp`) and multiplayer (`rtcw-mp`).
RTCW is one title with two mode profiles using the same executable.
Wolfenstein: Enemy Territory (`wolfet`) is a fourth title and fifth profile.
Star Trek Voyager: Elite Force is a fifth title with two mode profiles, single
player (`stvef-sp`) and Holomatch (`stvef-mp`), sharing one installation.
The February 1999 Quake III IHV Test has a separate eighth profile (`q3-ihv`)
under the Quake III title; its native modules and protocol 33 are not retail Q3.
Star Wars Jedi Knight: Jedi Academy is a sixth title with the ninth and tenth
profiles, single player (`jka-sp`) and multiplayer (`jka-mp`), sharing one
`base/` installation. Star Wars Jedi Knight II: Jedi Outcast is a seventh
title with the eleventh and twelfth profiles, `jk2-sp` and `jk2-mp`, sharing
one `base/` installation as well. American McGee's Alice is an eighth title
with the thirteenth profile, `alice`, and one mode. Alice is the only title
outside the Quake III line: it runs Ritual Entertainment's Heavy Metal:
F.A.K.K.2 engine, so its map, model, module, wire and audio contracts are
Ritual's rather than id's and almost nothing shared applies to it unchanged.
Heavy Metal: F.A.K.K.2 itself is a ninth title with the fourteenth profile,
`fakk2`, and one mode. Its engine is the parent of Alice's, so it is served
through Alice's adapters with its own records crossing at the module boundary;
its `fakk2.exe` is encrypted on disk, so its engine behaviour is inferred from
`alice.exe` where its modules and data do not settle it.
Medal of Honor: Allied Assault is a tenth title with the fifteenth profile,
`mohaa`, and one mode. It runs 2015's engine, a descendant of Ritual's and
Quake III's, through adapters of its own: it is never routed through the
Ritual (Alice and F.A.K.K.2) profile test. Its `MOHAA.exe` is
SafeDisc-encrypted and cannot run here, so its engine behaviour is measured in
its two plain game libraries, read from its data, or taken from OpenMoHAA's
reconstruction, each marked as such.
The public Quake III test releases of 1999, Q3Test 1.02 to 1.08, are a
sixteenth profile, `q3test`, under the Quake III title. Each release has its
own protocol (35 to 40), map version (IBSP 44 or 45) and module tables, so the
profile pins exactly one release at startup from the installation's own
modules and pak, and never mixes two.

Q3 and QL are enabled; RTCW remains experimental and gated in default builds,
with `-Drtcw-development=true` enabling isolated development probes. ET is also
gated; developer builds (`-Dwolfet-development=true`) run its client and server
for isolated probes; release builds still refuse both. Elite Force is gated too;
`-Dstvef-development=true` enables its isolated probes.
IHV remains gated by default; `-Dq3-ihv-development=true` permits Windows x86
dedicated native-game probes only. Its client and network paths remain blocked.
Jedi Academy is gated too: `-Djka-development=true` enables its isolated
probes, as `-Djk2-development=true` enables Jedi Outcast's. Both developer
multiplayer servers load retail game modules through their own adapters.
Academy's protocol-26 codec, client records and static rendering have source
and unit evidence; live interoperability and complete client rendering remain
open. Its SP API-8 bootstrap serves measured callbacks and refuses remaining
ones by name; campaign, client admission and saves remain open. Outcast's own
profile ledger describes its distinct protocol and partial client/SP boundaries.
Alice is gated as well: developer builds (`-Dalice-development=true`) run its
retail game and client modules, wire, demos, savegames, audio and renderers
for isolated probes, while its compatibility gates remain open and default
builds refuse it. F.A.K.K.2 is gated the same way by `-Dfakk2-development=true`,
and Medal of Honor by `-Dmohaa-development=true`. Q3Test is gated too:
`-Dq3test-development=true` runs its Windows x86 developer client and server
with each release's retail modules.
The unified goal is not complete until all sixteen profiles pass their applicable
module ABI, network, demo, filesystem, asset, renderer, audio/input, gameplay, and
live-interoperability gates, including RTCW SP campaign/savegame behavior, in
[`docs/fntech3/COMPATIBILITY.md`](docs/fntech3/COMPATIBILITY.md).

Do not describe fnTech3 as fully compatible while any required gate remains
open. Source inspection, unit tests, and a successful build are valuable
evidence, but they are not substitutes for runtime or bidirectional interop
evidence.

## Architecture invariants

1. Select exactly one game profile before filesystem startup, protocol setup,
   config selection, module loading, Steam initialization, or other
   compatibility-sensitive work.
2. Profile selection is immutable for the process lifetime. Accept an explicit
   profile override or unambiguous installation evidence; reject missing,
   contradictory, or mixed-game evidence with an actionable error.
3. A profile owns a coherent contract set: executable identity, search paths,
   pak rules, pure checks, config names, module APIs and layouts, protocols,
   demos, asset formats, renderer contracts, Steam behavior, and optional
   services, plus mode-specific campaign, savegame, and AI behavior where
   applicable. Never mix contracts from different profiles through independent
   cvars. A mode switch relaunches the same executable with a new profile.
4. Keep engine-internal types separate from external contracts. Freeze exact Q3,
   QL, RTCW SP, and RTCW MP wire/module POD layouts and marshal at the boundary;
   do not expose one profile's expanded structures to another profile's QVMs,
   native modules, or packets.
5. Preserve exact trap/import/export ordinals, structure sizes and offsets,
   scalar encodings, delta-field order, protocol negotiation, and pak/pure
   semantics for the selected profile.
6. Q3 evidence is authoritative for the Q3 profile. Legitimate retail QL
   observations are authoritative for the QL profile; QLSRP may support an
   independently written implementation but does not outrank retail behavior.
   RTCW SP and MP use their respective original contracts and legitimate retail
   observations; keep original-engine and iortcw comparison evidence distinct.
7. Preserve working renderer, audio, platform, packaging, tooling, performance,
   and diagnostics unless profile evidence requires a narrowly scoped change.
8. fnTech3 is engine-only. Do not reconstruct or redistribute retail game
   modules, proprietary runtimes, credentials, or game assets. Compatibility is
   exercised with user-supplied legitimate installations.
9. Retail QL client modules impose an x86 boundary. The shared client proof
   target is Windows x86. A release cannot claim the complete unified client
   contract unless the same engine binaries pass Q3, QL, RTCW SP, RTCW MP, and ET.
   Partial-profile and extra architecture-specific builds may make narrower,
   explicitly documented claims; they do not complete the unified goal.
10. Optional online services must fail honestly. Never synthesize Steam
    authentication, subscription, Workshop, statistics, or service success.
11. Support each game on fnTech3's own feature set, never on a copy of its
    retail engine. Retail evidence is authoritative for what a profile must
    honour: module ABIs, wire and demo formats, saves, file and asset formats,
    pak and pure rules, gameplay-visible behaviour and timing, and the data a
    module or asset hands the engine. It is not authoritative for how the
    engine presents that data. Audio, rendering, input, interface and
    diagnostics adopt the FnQ3/FnQL systems wherever they apply -- the spatial
    audio pipeline with its distance model, occlusion, EFX environments, audio
    zones and HRTF; per-pixel, shadowed and static lighting; engine-owned
    sidecars; and the rest. Adapt the profile's own data into them (its sound
    definitions, channels, lights, shaders, fog) rather than reproducing the
    retail engine's presentation in their place. A retail-exact presentation
    may remain as an explicit, documented option, never as the default that
    switches the engine's features off. Where a feature cannot apply to a
    profile, record why in that profile's evidence.

## Evidence and non-regression

- Record observed facts separately from inferences. Pin compatibility-sensitive
  constants to a source, fixture, packet transcript, module probe, or retail
  observation.
- Keep a record's machine-readable companion beside it in `docs/fntech3/`,
  named like the record with a `.json` extension, not in a subfolder.
- Validate malformed input, bounds, partial initialization, shutdown, path
  containment, and fallback behavior alongside the happy path.
- Compare every compatibility change against all affected profiles where the
  subsystem is shared. A correction for Q3, QL, RTCW SP, or RTCW MP must not
  silently alter another profile's behavior. Existing Q3/QL evidence does not
  establish either RTCW mode's compatibility.
- Use the evidence levels and gate ledger in
  [`docs/fntech3/COMPATIBILITY.md`](docs/fntech3/COMPATIBILITY.md). Update it when
  a gate gains evidence or a new blocker is found.
- When a profile's audio, rendering, input or interface path is added or
  revised, its record names the FnQ3/FnQL features that path uses, any it
  leaves out and why (invariant 11), and any retail-exact option kept beside
  them. Reading how retail presents something is evidence for the data it
  presents, not a specification for the engine's presentation.
- Full-compatibility evidence requires profile-appropriate clean builds, the
  complete automated suite, legitimate module/asset probes, demo playback,
  and bidirectional client/server interoperability where applicable, plus RTCW
  SP campaign/save/load and RTCW MP gameplay evidence. Record executable identity,
  commit, platform, profile, command line, fixture/installation identity, and
  outcome without committing proprietary inputs or credentials.
- Changes must remain robust and portable across supported Windows, Linux, and
  other id Tech 3 build paths. Platform-specific restrictions must be explicit
  rather than hidden behind the current development environment.

## Reference repositories and assets

- `E:\Repositories\FnQuake3` — Q3 modernization reference, pinned for this
  merger at `7d8c9629cdc7979f09697266470359101ffd9eda`.
- `E:\Repositories\FnQL` — QL compatibility baseline, pinned at
  `fce44fe714564621e6ceb79e3da7c53089646af2`.
- `E:\Repositories\QuakeLive-SRP` — QLSRP behavior/ABI evidence only. Anything
  informed by it must be independently designed for fnTech3; do not perform a
  mechanical or cosmetic source port.
- `E:\_SOURCE\_CODE\` — additional id Tech source references, including
  `OpenJK`, the Jedi Academy reference, pinned at
  `1a6a643427aa347553e9073dac5570b33337c4d9`. It is a maintained port, not
  the retail engine: separate its format and layout facts, which are
  contract-bound, from engine default values it may have retuned.
- `E:\Repositories\OpenAlice` — an independent Alice engine reconstruction
  with a checked-in Ghidra corpus for `alice.exe`, `fgamex86.dll` and
  `cgamex86.dll`, and a copy of Ritual's F.A.K.K.2 v1.02 SDK. Reference
  observation only: it is GPLv3 and this tree is GPLv2, so no text from it
  may be incorporated, and the retail binary outranks it wherever they
  disagree.
- A legitimate retail American McGee's Alice installation is at
  `E:\Games\Alice`. Read it, never write to it.
- A copy of the legitimate retail Heavy Metal: F.A.K.K.2 disc is at
  `E:\Games\Heavy Metal FAKK2`. Read it, never write to it. It is not an
  installation: its game libraries are inside the InstallShield cabinets, so
  probes use a layout laid down from it under `.tmp/fakk2-install/game`.
  Ritual's F.A.K.K.2 SDK (in the OpenAlice tree, and mirrored at
  https://github.com/Sporesirius/fakk2) is under Ritual's non-free licence:
  reference observation only.
- `E:\_SOURCE\_ASSETS\` — locally extracted assets used only as references.
- A legitimate retail Quake Live installation is normally under
  `C:\Program Files (x86)\Steam\steamapps\common\Quake Live`.
- A legitimate retail Jedi Academy 1.01 installation is at
  `E:\Games\Jedi Academy`. Read it, never write to it.
- A legitimate retail Jedi Outcast 1.02a installation is at
  `E:\Games\Jedi Outcast`. Read it, never write to it.
- A legitimate retail Medal of Honor: Allied Assault 1.00 installation is at
  `E:\Games\Medal of Honor Allied Assault`. Read it, never write to it.
  [OpenMoHAA](https://github.com/openmoh/openmohaa), pinned at
  `ab43a0def6c4feccedae36fdb58c2e007d7e4c35` and checked out under
  `E:\Repositories\OpenAlice\_tmp_openmohaa_1`, is GPL-2.0-or-later: parts of
  it are incorporated with their headers (see `code/mohaa/README.md` and the
  notices), the rest is reference observation, and the retail libraries
  outrank it wherever they disagree.

The FnQuake3 reference worktree contained local uncommitted work at merger
time. fnTech3 records the pinned committed baseline lineage; those dirty changes
must be reviewed and ported semantically, not copied wholesale or represented
as part of that commit.

## External code and attribution

- Before incorporating external code, verify that its license is compatible
  with the repository's GPL license. Preserve original license headers and
  provenance comments.
- Credit incorporated or adapted work in `CREDITS.md` and, where appropriate,
  `THIRD_PARTY_NOTICES.md`, using an embedded link to the original source.
- Treat reference observations differently from incorporated implementation
  text. Document which kind of use occurred.
- The inherited engine code and the qldec-derived table are GPL-2.0-or-later,
  compatible with the GPLv2 license text in `LICENSE`; keep the qldec notice in
  `THIRD_PARTY_NOTICES.md` intact.

## Repository workflow

- Prefer small compatibility slices: profile/identity, filesystem, module ABI,
  live wire codec, demos, assets, renderers, audio/input, then optional
  services.
- Treat module ABI, networking, demos, filesystem/pure behavior, pak loading,
  Steam discovery, and asset parsing as compatibility-sensitive by default.
- Keep `.install/` deterministic and use `.tmp/` for disposable investigation
  output. Never stage retail or proprietary files in the repository.
- Meson `subprojects/*.wrap` files own bundled dependency fallbacks. Do not
  restore private in-tree copies of third-party libraries removed by the FnQL
  baseline; legacy builds should use system packages or delegate to Meson.
- Rebrand project-owned names and outputs while preserving compatibility-bound
  filenames, API symbols, protocol tokens, and legacy aliases when evidence
  requires them.
- Update `version/fntech3_version.h` first for fnTech3 version changes. Legacy
  FnQL metadata may remain temporarily during the migration but is not a second
  source of truth.
- `README.md` and `.install/README.html` are generated. Edit
  `docs/templates/README.md.in` and `docs/templates/install-readme.html.in`,
  and the supported-game line in `version/fntech3_version.h`, then run
  `python scripts/generate_docs.py`. Editing either output directly is
  overwritten by the next run and fails `tests/generate_docs_tests.py`.
- Keep the player documentation in step with the ledger. When a profile's
  status, build option or a feature's game scope changes, update
  [`docs/GAMES.md`](docs/GAMES.md), the affected guide's game notes, and the
  README templates' games table; when a game is added, also update
  [`BUILD.md`](BUILD.md)'s development options and
  [`docs/fntech3/ARCHITECTURE.md`](docs/fntech3/ARCHITECTURE.md)'s profile
  table. [`docs/README.md`](docs/README.md) indexes the documentation.

## Stable test executable paths

- Reuse executable paths across runtime tests so Windows Firewall permissions
  remain useful. Rebuild in place; do not create a new executable name or launch
  directory for each task, timestamp, game profile, or run.
- Use the existing build presets and launch paths, normally
  `meson/build/win32-dev/fntech3.exe` and
  `meson/build/win32-dev/fntech3.ded.exe` on Windows. Package validation uses the
  corresponding freshly staged `.install/` binaries. Keep runtime modules
  matched to the tested build and select games through the supported profiles.
- Isolate each test's writable configuration, saves, logs, and captures under
  `.tmp/<task>/` using supported home-path options; test-data isolation must not
  require copying the executable. Keep retail installations read-only.
- When a comparison, architecture, or build configuration genuinely requires a
  separate runtime, reuse existing preset paths or a small set of fixed
  project-local locations, such as `.tmp/runtime/baseline/` and
  `.tmp/runtime/candidate/`. Record the tested build identity and coordinate use
  so another task cannot replace a runtime during a test. Retain these reusable
  locations across tasks; clean disposable test data separately.
- Apply the same convention when adding or updating test harnesses. Resolve
  recurring firewall prompts through stable launch paths; do not disable the
  firewall or its notifications, add broad exceptions, or modify firewall rules
  without the user's explicit instruction.

## Runtime-test safety

- Never launch a game fullscreen unless the user has explicitly allowed
  fullscreen for that specific test. Force windowed mode with
  `+set r_fullscreen 0` or the engine's equivalent.
- Never control, inject, capture, or hijack mouse or keyboard input without
  explicit, narrowly scoped permission for that action. Ask again for a new
  action or after the approved action has completed.
- The only standing exception is the user's authorization to click buttons on
  game intro screens solely to progress past them, including a briefing's Ready
  button. These clicks do not require another permission request. All other
  mouse and keyboard input remains subject to the rule above.
- Never use operating-system screen capture to validate id Tech rendering. Use
  the engine's registered `screenshot` command and validate the image written
  from the render target. Repair or instrument that path if it is unavailable.
- Prefer the cheapest probe that establishes the needed fact. Runtime probes
  must use isolated writable home paths and leave installations unchanged.

## Task completion

At the end of each task, report unrelated issues discovered during the work,
even when they are outside the requested scope.
