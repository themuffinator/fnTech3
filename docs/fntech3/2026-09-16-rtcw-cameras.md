# RTCW camera evaluation

RTCW SP now progresses through the opening `escape1` cinematic and reaches
the first-person cell view with a knife and health HUD. The separately
authorized 90-second camera test completed without an engine error. Its
engine screenshots show the briefing, two cinematic views and the view after
the intro. Character/attachment rendering defects are visible during the
cutscene; audio was disabled and no movement or combat was exercised.

The previous fifth startup retest stopped at SP `CG_LOADCAMERA` (98), after
the installed game requested `cs01a_01`. This change supplies that subsystem.
All five earlier ready clicks and the separate camera-test click are consumed.

## Contract and implementation

One owned manager holds 64 camera slots. Loading a valid slot clears its
previous definition before file lookup; a failed replacement leaves it empty.
Invalid indices preserve the original false/no-op behavior without reading
extra syscall arguments. Cgame shutdown clears every slot and primary-camera
state, including partial initialization.

The parser supports fixed, interpolated and sampled cubic spline positions,
moving targets, authored velocity intervals, target events, waits, FOV
transitions and numeric fade commands. Evaluation preserves query-dependent
distance integration, file-order event processing, inclusive velocity
intervals, integer-second expiry, target-duration assignment, the original
final-spline-segment behavior and zero-duration FOV retention. The blank
unquoted name in `cs01a_01` consumes `baseVelocity` as its name; original
position-key recovery still finds the following position. The reference
executable confirms this, superseding the initial parser-loss inference.

SP clears camera velocity intervals and starts the position before rebuilding
the timeline. Original MP retains intervals, builds the timeline before
starting the position, and has no feather event. The selected profile binds
that timing policy when the file is loaded. iortcw MP adopts SP timing; it is
recorded as a separate reference observation rather than used to overwrite
the original MP contract.

The native/QVM adapter copies bounded filenames, reads through the selected
filesystem, checks disjoint complete output spans and publishes owned values
with `memcpy`. Pitch/yaw are written while roll and output padding remain
untouched. SP's primary-camera flag controls the existing logical key
dispatcher: Escape/Space/Enter request `cameraInterrupt`, other key presses
are consumed outside UI/console ownership, and ordinary releases remain
available to unwind held actions. STOP clears this input policy without
disabling subsequent path evaluation, as in the original SP import.

## Bounds and explicit differences

- Files are capped at 1 MiB; tokens at 1,023 bytes; targets at 128; events at
  2,048; controls per spline at 4,096; sampled points per definition at 65,536;
  velocity intervals per path at 8,192; total duration at one day.
- Malformed numbers, incomplete syntax, invalid granularity, resource limits,
  arithmetic overflow and calls before load/start fail explicitly. A bounded
  binary32 store makes overflow admission consistent with x87 excess precision.
- Coincident interpolated endpoints and zero-length spline spans return a
  finite position. Original MP can instead produce NaNs; these are classified
  separately from finite numerical comparisons.
- Original camera-chain event 10 can replace its own object and return an
  uninitialized direction. It remains an explicit unsupported event. None of
  the 210 installed camera files examined uses it.
- Fade parameters become a fixed numeric command dispatched after evaluation.
  No camera string becomes executable command text, and no module callback
  runs while the evaluator retains path references. This does not establish
  original command-buffer interleaving or live fade-image parity.
- Default-initialized state replaces undefined uninitialized original fields.
  Missing start events for moving targets are diagnosed; no fabricated frame
  is returned from undefined state.

## Validation and evidence

`tests/rtcw_camera_tests.py` executes paths, events, SP/MP timing, slot lifetime,
3,000 bounded parser mutations, resource/number limits, native/QVM dispatch,
partial file reads, failed replacement, output ownership and logical key
policy. These fixtures do not use device input.

`scripts/probe_rtcw_cameras.py` compiles unmodified local original SP/MP and
iortcw sources into disposable reference executables. It reads installed
camera bytes through pipes and records identities, hashes and numerical
comparisons; no retail camera files are copied or committed. Each file is
sampled at dense and irregular intervals, across expiry and two starts.
iortcw's actual `qcommon/q_shared.c` dependency is compiled as C, matching its
Makefile; its obsolete `splines/q_shared.cpp` is not used. A temporary header
shim isolates legacy MSVC typedef/attribute declarations from current standard
headers without changing the camera implementation.

Final Windows x86 and x64 source probes each cover 210 installed files,
382 queries per file and four references: 320,880 sample pairs per architecture.
All 80,220 original-SP samples and 67,470 finite original-MP samples match
exactly. The other 12,750 original-MP samples contain nonfinite source outputs
and are classified separately; the candidate remains finite. Each iortcw
reference contributes 80,220 finite comparisons with maximum absolute
difference 0.000016. Reports are
`.tmp/rtcw-manager/camera-mode-source-{x86,x64}/report.json`.

Nine focused tests pass on Windows x86/x64 and Linux x86 with address/undefined
behavior sanitizers and leak detection. Final client/server builds pass.
The final complete suite passes 111 Meson tests with two skips, including
1,470 Python tests with eleven skips in 511.371 seconds. The final source and
binary identities are recorded in
`.tmp/rtcw-manager/camera-source-identities.json`; the complete suite log is
`camera-mode-testlog.txt` in the same directory. All six final
`camera-mode-live` Q3/QL engine images
were personally inspected: Q3 world/weapon/HUD and QL join UI/world through
GL/GLSL, Vulkan and RTX raster mode. These are regression smokes, not image
parity or peer-interoperability evidence.

## Observed SP startup

The isolated Windows x86 development executable was
`4fc3449d0640cdcababcf4ab76c5d071a1d568545828160af9907326d25bce86`, with
GL renderer `1d5a93ca976183cbee982ad0fab0c2a970b5da603d25f66e70804ec731a64235`.
The source checkpoint records the dirty-worktree base commit. Runtime manifests
record the installed native modules, full command, copied binary identities,
temporary instrumentation and explicit one-click authorization under
`.tmp/rtcw-manager/camera-ready-retest-1/`.

The run used the registered `screenshotPNG` command at approximately 2, 35
and 90 seconds after the ready click. At 2 and 35 seconds, the log reports a
valid active snapshot and primary camera mode. Ten `startCam` commands cover
`cs01a_01` through `cs01a_11d`. At 90 seconds, the snapshot remains valid,
camera mode is zero and the image shows the first-person knife and health
HUD. The client's snapshot time advanced from 3,433 to 91,435 ms. The recorded
`sv_paused` cvar remained one while client pause and `cg_norender` were zero;
the cvar alone is not used as proof of server pause or gameplay readiness.

The process exited normally, the installation inventory was unchanged, and
the isolated writable home was removed. The temporary logical ready-click
instrumentation was restored byte-for-byte before rebuilding the ordinary
development binary. No OS input, desktop switching or OS capture was used.

All four captures were personally inspected. The middle capture shows
character/attachment defects; the console also contains repeated missing
head-skin shader warnings and unsupported material-token warnings. In
particular, `picmip2` is rejected for the same head shaders subsequently
reported missing. `ParseShader` returns false for that unknown keyword; the
original SP renderer uses it to select character texture mip behavior. This
is a concrete material-parser defect to address next. `mapcomp` and
`animmapcomp` are also rejected. The final capture is
`runtime/rtcw-sp-after-ready-late.png`, SHA-256
`0ee6826f8dc08c657caf8922cb2327702780fcb675c72b7ad0de2129b74851fb`.
`runtime-observations.json` records the images, camera sequence, states and
warning counts without equating the screenshot with tested player controls.

Active SP voice amplitude, movement/combat, campaign/save-load coverage,
sustained MP gameplay and renderer image parity remain open. The ordinary
release gate remains closed; this development run does not establish full
RTCW compatibility.

## Provenance

The original [SP camera implementation](https://github.com/id-Software/RTCW-SP/tree/master/src/splines),
[MP camera implementation](https://github.com/id-Software/RTCW-MP/tree/master/src/splines)
and [iortcw references](https://github.com/iortcw/iortcw) are GPLv3-or-later.
Their notices and identities are checked for the disposable observational
probe. The production model, parser, manager and adapters are independently
written GPLv2-or-later code; no reference implementation or retail assets are
incorporated. Credits are recorded in `CREDITS.md`.

Previously discovered unrelated issues remain open: the generic Q3/QL routing
cache reader bounds audit, and the source-inferred generic OpenAL big-endian
PCM16 mismatch outside RTCW entity-stream decoding.
