# AGENTS.md

## Mission

fnTech3 is one modern id Tech 3 engine distribution whose shared client and
dedicated-server binaries select the installed game at startup. Quake III Arena
(`q3`) and Quake Live (`ql`) are the first supported profiles. The project is
not complete until both profiles pass their profile-specific module ABI,
network, demo, filesystem, asset, renderer, and live-interoperability gates in
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
   services. Never assemble an accidental Q3/QL hybrid from independent cvars.
4. Keep engine-internal types separate from external contracts. Freeze exact Q3
   and QL wire/module POD layouts and marshal at the boundary; do not expose one
   profile's expanded structures to another profile's QVMs, native modules, or
   packets.
5. Preserve exact trap/import/export ordinals, structure sizes and offsets,
   scalar encodings, delta-field order, protocol negotiation, and pak/pure
   semantics for the selected profile.
6. Q3 evidence is authoritative for the Q3 profile. Legitimate retail QL
   observations are authoritative for the QL profile; QLSRP may support an
   independently written implementation but does not outrank retail behavior.
7. Preserve working renderer, audio, platform, packaging, tooling, performance,
   and diagnostics unless profile evidence requires a narrowly scoped change.
8. fnTech3 is engine-only. Do not reconstruct or redistribute retail game
   modules, proprietary runtimes, credentials, or game assets. Compatibility is
   exercised with user-supplied legitimate installations.
9. Retail QL client modules impose an x86 boundary. A release cannot claim the
   shared Q3/QL client contract unless the same Windows x86 engine binaries have
   passed both profiles; extra architecture-specific builds may make narrower,
   explicitly documented claims.
10. Optional online services must fail honestly. Never synthesize Steam
    authentication, subscription, Workshop, statistics, or service success.

## Evidence and non-regression

- Record observed facts separately from inferences. Pin compatibility-sensitive
  constants to a source, fixture, packet transcript, module probe, or retail
  observation.
- Validate malformed input, bounds, partial initialization, shutdown, path
  containment, and fallback behavior alongside the happy path.
- Compare every compatibility change against both profiles where the subsystem
  is shared. A QL correction is not allowed to silently alter Q3 behavior, and
  vice versa.
- Use the evidence levels and gate ledger in
  [`docs/fntech3/COMPATIBILITY.md`](docs/fntech3/COMPATIBILITY.md). Update it when
  a gate gains evidence or a new blocker is found.
- Full-compatibility evidence requires profile-appropriate clean builds, the
  complete automated suite, legitimate module/asset probes, demo playback,
  and bidirectional client/server interoperability. Record executable identity,
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
- `E:\_SOURCE\_CODE\` — additional id Tech source references.
- `E:\_SOURCE\_ASSETS\` — locally extracted assets used only as references.
- A legitimate retail Quake Live installation is normally under
  `C:\Program Files (x86)\Steam\steamapps\common\Quake Live`.

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
