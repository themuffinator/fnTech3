# RTCW activation, pickup policy, and character eyes

This slice starts from `0b6af9ba7a17d73bf27c5c016c00772a1a93899a` with an
already modified worktree, including separate input/menu and renderer work.
It fixes three reported RTCW regressions without changing the game modules.

## Causes and corrections

`+activate` and `-activate` were absent from the engine command registry.
The RTCW profiles now register both against button 6, using the existing
held-key, short-tap, generation, release, and focus-reset machinery. Activation
plus keyboard strafe produces RTCW's lean bits and suppresses lateral movement.
RTCW's any-key bit is also encoded as 128; Q3/QL retain their existing value.
RTCW-only commands do not become engine commands in Q3 or QL.

The SP pickup disagreement was a missing engine-owned userinfo registration.
The retail cgame registers `cg_autoactivate` as archive-only and defaults to 1;
its item prediction therefore acts on touch. The game reads this setting from
userinfo, treats an absent value as 0, and then requires activation before
granting the item. The engine now publishes `cg_autoactivate` for SP and MP
before connection. SP also publishes the related `cg_emptyswitch` preference.
Existing user settings, including `cg_autoactivate 0`, are preserved.

SP eyelids use an `h_blink` surface drawn over the open eyes. The earlier
renderer applied that visibility rule only to MDS surfaces, leaving eyelids
permanently drawn on MD3/MDC head attachments. A shared mesh-material helper
now applies the rule to all three formats in GL, Vulkan, and RTX. MP's `_b`
skin alternate also works on MD3/MDC, with the original normal-skin fallback
and custom-shader precedence. Native RTX geometry uses the same policies and
includes blink state when deciding whether to rebuild a dynamic scene.

## Verification

The Win32 MSVC development and Release clients and all three renderer modules build.
The focused Win32 run passes 100 tests and 39 subtests. An additional native
MD3 ray-geometry regression passes alongside the raster mesh test. The tests
exercise actual input functions, userinfo registration, renderer surface
traversal, and MD3/MDC/MDS geometry, including Q3/QL isolation, invalid skin
handles, missing blink alternates, and release/reset behavior.

The new activation, pickup-policy, and all three raster eye regressions were
also compiled against the original committed implementation. All five fail
at their intended behavioral assertions, then pass with the corrections.

The four final interaction/mesh tests also pass on Linux x86. The Windows full
suite attempt reports 109 passed, two skipped, and two failed Meson targets.
Its Python aggregate runs 1,488 tests with eleven skips and five failures: stale
menu/WebUI source assertions about the pre-existing pointer-routing changes.
Those assertions were updated concurrently outside this slice. A subsequent
run of both affected test files together with the final interaction/mesh tests
passes 59 tests and 40 subtests. This records the full-run failure and the
passing targeted rerun separately; it is not a fresh all-green full-suite run.

Five installed-runtime probes pass using the legitimate installation at
`E:/SteamLibrary/steamapps/common/Return to Castle Wolfenstein`:

- SP `escape1` on GL, Vulkan, and RTX raster: the server's `dumpuser 0` contains
  `cg_autoactivate 1` and `cg_emptyswitch 0`. Engine screenshots at about 5 and
  35 seconds show the opening cinematic; the close character shots show eyes.
- SP with explicit `cg_autoactivate 0`: the server receives 0 unchanged.
- MP `mp_beach` with defaults: the server receives `cg_autoactivate 1`.

Every process exits normally without engine errors. Each launch uses windowed
mode, an isolated writable home, disabled input devices, and an unswitched
private desktop. Visual probes use exactly one intro Ready click under the
standing AGENTS.md exception and the registered `screenshotPNG` command.
The private capture executable is linked from a separately compiled copy of
the client translation unit; production sources and build objects are not
modified by the capture hook. Runtime manifests pin executable/renderer hashes,
command lines, image hashes, and server observations. Installation metadata
remains unchanged, and writable homes are removed after each run.

Evidence is retained under `.tmp/rtcw-player-fixes/`, especially
`baseline-regressions.log`, `focused-x86.log`, `ray-regressions.log`,
`final-and-menu.log`, `full-suite.log`, the `linux-*.log` files, and
`runtime/manifest.json`. All six PNGs under `runtime/eyes-*/` were inspected.

These checks establish the activation command contract, corrected server
pickup preference, and sampled SP eye rendering. They do not establish a
live door interaction, walking over a weapon, native ray-tracing GPU output,
all character skins, or full campaign/interoperability compatibility.

## Provenance and other observations

The original [SP client and game](https://github.com/id-Software/RTCW-SP/tree/master/src)
and [MP client and renderer](https://github.com/id-Software/RTCW-MP/tree/master/src)
provide the observed button encodings, userinfo ownership, and blink contracts.
Their GPLv3 source was inspected for behavior only. The corrections and tests
are independently authored GPLv2-or-later code; no external implementation
text or proprietary assets/modules are incorporated.

Other gaps discovered: RTCW's named reload, sprint, zoom, kick, and standalone
lean commands are also absent from the current engine bindings. This slice
implements activation and its strafe-to-lean behavior. The subsequent
[engine command audit](2026-09-17-rtcw-engine-commands.md) supplies those named
bindings and records the rest of the original engine command inventory. The
subsequent
[transparent-fog correction](2026-09-17-rtcw-transparent-fog.md) removes the torch
rectangles reported in these captures and adds inspected captures in all three
renderers. RTX's overall fog brightness still differs from GL/Vulkan; that
separate observation is not attributed to the eye correction.
