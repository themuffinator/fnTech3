# RTCW AAS queries, tag handoff and SP readiness

Status: **partial Source / Unit / Build / Fixture evidence**. The shared
experimental executable now reaches the installed SP campaign's normal ready
screen with all 29 soldiers. MP still passes local pure admission and its
deployment-screen probe. Campaign play and save/load remain unverified.

## Implemented boundaries

The checked RTCW bot dispatcher now supports imports 305, 308, 310, 316 and
318: presence bounds, point-area number, point contents, area reachability
and swimming. These invoke the existing engine algorithms against the
selected navigation world. Native and QVM arguments receive complete-span,
integer-width and finite-vector checks. Both bounding-box destinations are
validated before either is written, and unaligned/overlapping destinations
are supported. Published unknown-presence behavior remains the library's
crouching fallback. Quake's algorithms and export layout are unchanged.

This advances SP from import 308 to import 47, `G_GETTAG`. That import is now
implemented as the original synchronous server-to-cgame callback. The cgame
owns pose selection and world-space tag transformation; the server does not
substitute a separate renderer lookup. An engine-only semantic export maps
explicitly to RTCW cgame export 9 and is rejected for Q3/QL.

The tag bridge copies the bounded name and frozen 48-byte orientation through
owned buffers. It checks client capacity, module identity, complete source
spans and successful output finiteness. Nested callbacks have independent
storage. A failed callback preserves the original partial-output semantics;
an invalid successful result does not reach game memory. Dedicated builds
and runtime dedicated mode return false without reading caller buffers.
The dedicated compile guard supports both build-system spellings, `DEDICATED`
and `USE_DEDICATED`; an initial link failure exposed this distinction and is
retained in the evidence directory.

The destination remains a legacy native x86 cgame, consistent with the
existing RTCW VM policy. QVM source-buffer tests establish checked marshaling
only; they do not claim a supported RTCW QVM module configuration.

## Runtime observation

`aas-queries-map/` first records SP passing the new queries and stopping at
`G_GETTAG`, with MP still passing. After the tag bridge, `aas-tags-map/` records
both modes exiting cleanly from the same development executable. Neither run
reports an unsupported import or reliable-command overflow; isolated homes
are removed and installation metadata is unchanged.

The SP log records `rockandroll`, all 29 soldier clients, and server time
advancing from 650 to 1,500 milliseconds before the normal pregame pause.
The final state is `cl_paused=1`, `sv_paused=1`, `cg_norender=1`,
`g_reloading=1`. The personally inspected engine PNG shows the mission briefing
with its ready arrow instead of `Please Wait`. This establishes cast readiness,
not a playable-world capture. Advancing that screen requires a separately
authorized input action under the project's runtime-test rule.

The MP engine PNG shows its readable deployment UI and briefing movie; the
world is obscured. All six `aas-tags-live/` Q3/QL checks pass under GLX,
Vulkan and RTX raster, with personally inspected world/HUD or native join-UI
captures. QL was not joined and image parity is not claimed.

All runtime checks used 640×480 windowed mode on unswitched private desktops,
disabled input and muted/off audio. Screenshots came from the engine render
target. No user input was supplied.

## Retained evidence

Evidence is under `.tmp/rtcw-manager/`:

- `aas-queries-reference-{x64,x86,linux32}.json`: each original/iortcw SP/MP
  reference passes 527,849 comparisons, covering selected synthetic graphs,
  point/content results, reachability counts, swimming and presence bounds.
  Linux x86 includes ASan, UBSan and leak detection.
- `aas-queries-focused-x86.log` and `aas-queries-focused-sanitizers.log`:
  six tests pass in 7.997 and 31.812 seconds. They execute real AAS routines
  and exercise both worlds, native/QVM arguments, all content bits, malformed
  floats/spans, absent maps and shutdown.
- `aas-tags-focused-{x86,x64}.log`: six tests pass in 6.123 and 8.383 seconds.
  The native handoff, module exports and existing reverse animation handoffs
  are covered. The x64 handoff case checks explicit rejection, not pointer
  truncation or native x64 support.
- `aas-tags-focused-sanitizers.log`: five handoff/export tests pass on Linux
  x86 in 59.618 seconds. `aas-tags-dedicated-sanitizers.log` separately verifies
  the final dual-define dedicated guard in 5.400 seconds.
- `aas-tags-dev-build-fixed.log` and `aas-tags-default-build.log`: Windows
  x86 client/server and all three renderer targets build successfully.
- `aas-tags-map/manifest.json` and `aas-tags-live/manifest.json`: executable,
  module/asset, renderer, launch, cleanup and capture identities.
- `aas-tags-final-suite.log` and `aas-tags-final-testlog.txt`: 111 Meson
  checks pass with two skips; the Python suite passes 1,406 tests in 332.871
  seconds with 11 skips. The first run exposed a missing declaration in the
  isolated cvar test fixture; the fixed fixture and complete rerun pass.
- `aas-tags-source-identities.json`: verified source, reference, executable
  and evidence hashes for this dirty-worktree checkpoint.

Development executable SHA-256:
`c4ea988d9344ee0e725acdbb930d9d86e95af86913d89e9a5fc9039950f527e4`.
Default executable SHA-256:
`182077dfcb7774bb07eb3bb2abccdcc0e154488149ae9a210dfc0574c92fc904`.

## Limits and provenance

Remaining navigation queries, movement and routing updates, cast lifecycle
policies, campaign progression, save/load, entity-stream audio, letterboxing,
broader renderer parity, demos and peer interoperability remain open. The
[compatibility ledger](COMPATIBILITY.md) keeps those gates open.

The original [SP](https://github.com/id-Software/RTCW-SP/tree/master/src),
[MP](https://github.com/id-Software/RTCW-MP/tree/master/src) and
[iortcw](https://github.com/iortcw/iortcw) sources supply behavioral evidence.
Adapters and tests are independently authored; existing GPL-2.0-or-later
engine query implementations are retained. GPLv3 functions/notices are used
only in disposable comparison fixtures. No retail module or asset is copied
into the project.

No new unrelated issue was found. The inherited Q3/QL routing-cache reader
bounds audit remains open; SP sound-resource and shader-keyword warnings
remain RTCW follow-up work.
